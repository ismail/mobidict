#include "mobidict.h"

#include <QCollator>
#include <QDebug>
#include <QElapsedTimer>

MobiDict::MobiDict(const QString &path, const QString &serial) : QObject()
{
  m_codec        = nullptr;
  m_deviceSerial = serial;
  m_isCP1252     = false;
  m_language     = QString();
  m_mobiData     = nullptr;
  m_rawMarkup    = nullptr;
  m_path         = path;
  m_title        = QString();
}

MobiDict::~MobiDict()
{
  mobi_free(m_mobiData);
  mobi_free_rawml(m_rawMarkup);

  for (const auto &key : m_wordHash.keys())
    qDeleteAll(m_wordHash[key]);
}

QString MobiDict::resolveLink(const QString &link)
{
  const uint32_t offset = link.toUInt();
  QString match         = QString();

  for (const auto &entry : m_wordHash.keys()) {
    for (const auto &mobiEntry : m_wordHash[entry]) {
      if (mobiEntry->startPos == offset) {
        match = entry;
        break;
      }
    }
  }

  return match;
}

QString MobiDict::lookupWord(const QString &word)
{
  if (m_wordHash.constFind(word) == m_wordHash.constEnd())
    return QString();

  // Force rich-text detection
  QString result = "<qt>";

  uint32_t entry_startpos = 0;
  uint32_t entry_textlen  = 0;

  for (const auto &mobiEntry : m_wordHash[word]) {
    const MobiEntry *const m = mobiEntry;
    entry_startpos           = m->startPos;
    entry_textlen            = m->textLength;

    std::string html;
    html.assign(m_rawMarkup->flow->data + entry_startpos,
                m_rawMarkup->flow->data + entry_startpos + entry_textlen);

    if (m_isCP1252)
      result.append(m_codec->toUnicode(html.c_str()));
    else
      result.append(QString::fromUtf8(html.c_str()));

    // Change filepos -> href, {hi,low}recindex -> src
    // so that Qt can give us a url in QTextBrowser::loadResource()
    result = result.replace("filepos=", "href=");
    result = result.replace("hirecindex=", "src=");
    result = result.replace("lowrecindex=", "src=");
    result = result.replace("recindex=", "src=");

    // qWarning() << "HTML entry:";
    // qWarning() << result;
  }

  return result;
}

MOBI_RET MobiDict::open()
{
  m_mobiData = mobi_init();
  if (m_mobiData == nullptr)
    return MOBI_MALLOC_FAILED;

#ifdef Q_OS_WIN
  wchar_t *w_path = new wchar_t[m_path.length() + 1];
  int len         = m_path.toWCharArray(w_path);
  w_path[len]     = NULL;

  FILE *file;
  _wfopen_s(&file, w_path, L"rb");

  delete[] w_path;
  w_path = nullptr;
#else
  FILE *file = fopen(m_path.toLocal8Bit(), "rb");
#endif

  if (file == nullptr)
    return MOBI_ERROR;

#ifndef NDEBUG
  QElapsedTimer timer;
  timer.start();
#endif

  MOBI_RET mobi_ret = mobi_load_file(m_mobiData, file);
  fclose(file);

  if (mobi_is_encrypted(m_mobiData)) {
    if (!m_deviceSerial.isEmpty()) {
      // qWarning() << "Using device serial" << m_deviceSerial;
      mobi_drm_setkey_serial(m_mobiData, m_deviceSerial.toLatin1());
    }
    else {
      return MOBI_FILE_ENCRYPTED;
    }
  }

  if (mobi_ret != MOBI_SUCCESS)
    return mobi_ret;

  m_isCP1252 = mobi_is_cp1252(m_mobiData);
  if (m_isCP1252)
    m_codec = QTextCodec::codecForName("cp1252");

  char *title    = mobi_meta_get_title(m_mobiData);
  m_title        = QString::fromUtf8(title);
  char *language = mobi_meta_get_language(m_mobiData);
  m_language     = QString::fromLatin1(language);

  free(title);
  free(language);

  m_rawMarkup = mobi_init_rawml(m_mobiData);
  if (m_rawMarkup == nullptr)
    return MOBI_MALLOC_FAILED;

  mobi_ret =
      mobi_parse_rawml_opt(m_rawMarkup, m_mobiData, false, /* parse toc */
                           true,                           /* parse dic */
                           false /* reconstruct */);

  if (mobi_ret != MOBI_SUCCESS)
    return mobi_ret;

  if (!m_rawMarkup->orth)
    return MOBI_FILE_UNSUPPORTED;

  uint32_t entry_startpos = 0;
  uint32_t entry_textlen  = 0;

  const size_t count = m_rawMarkup->orth->total_entries_count;

#ifndef NDEBUG
  QStringList multiples;
#endif

  for (size_t i = 0; i < count; ++i) {
    const MOBIIndexEntry *orth_entry = &m_rawMarkup->orth->entries[i];
    entry_startpos = mobi_get_orth_entry_start_offset(orth_entry);
    entry_textlen  = mobi_get_orth_entry_text_length(orth_entry);

    if (entry_startpos == 0 || entry_textlen == 0) {
      ++i;
      continue;
    }

    QString label;

    if (m_isCP1252)
      label = m_codec->toUnicode(orth_entry->label);
    else
      label = QString::fromUtf8(orth_entry->label);

#ifndef NDEBUG
    if (m_wordHash.constFind(label) != m_wordHash.constEnd())
      multiples << label;
#endif

    MobiEntry *mobiEntry  = new MobiEntry;
    mobiEntry->startPos   = entry_startpos;
    mobiEntry->textLength = entry_textlen;

    m_wordHash[label].append(mobiEntry);

    // qDebug("Adding %s", orth_entry->label);
  }

  sortKeys();

  if (m_wordHash.isEmpty()) {
    qWarning() << "Failed to find any word.";
    return MOBI_DATA_CORRUPT;
  }

#ifndef NDEBUG
  qDebug() << "Dictionary loaded in" << timer.elapsed() << "miliseconds";
  qDebug() << "Here are the words with multiple entries:";
  qDebug() << multiples;
#endif

  return MOBI_SUCCESS;
}

const QString &MobiDict::title()
{
  return m_title;
}

const QList<QString> &MobiDict::words()
{
  return m_sortedKeys;
}

void MobiDict::sortKeys()
{
  m_sortedKeys = m_wordHash.keys();
  QCollator sorter;

  sorter.setLocale(QLocale(m_language));
  sorter.setIgnorePunctuation(true);
  sorter.setNumericMode(true);
  sorter.setCaseSensitivity(Qt::CaseInsensitive);
  std::sort(m_sortedKeys.begin(), m_sortedKeys.end(),
            [&](const QString &a, const QString &b) {
              return sorter.compare(a, b) < 0;
            });
}

MOBIPart *MobiDict::getResourceByUid(const size_t &uid)
{
  return mobi_get_resource_by_uid(m_rawMarkup, uid);
}
