#include <QtConcurrent/qtconcurrentrun.h>

#include <QApplication>
#include <QClipboard>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QKeyEvent>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QScreen>
#include <QShortcut>
#include <QWidget>

#ifdef AUTOTEST
#include <QTest>
#define SELF_TEST
#endif

#include "mainwindow.h"
#include "settings.h"

MainWindow::MainWindow() : QWidget(), m_ui(new Ui::MainWindow())
{
  m_ui->setupUi(this);
  m_ui->splitter->setStretchFactor(0, 2);
  m_ui->splitter->setStretchFactor(1, 8);

  m_currentDict     = nullptr;
  m_currentDictName = QString();
  m_deviceSerial    = QString();
  m_html            = QString();

  m_model = new QStringListModel;
  m_ui->matchesView->setModel(m_model);
  m_ui->matchesView->setUniformItemSizes(true);
  m_ui->matchesView->setEditTriggers(QAbstractItemView::NoEditTriggers);

#ifdef AUTOTEST
  m_stopTesting = false;

  // We need to handle Esc key to stop testing.
  installEventFilter(this);
#endif

// On windows force ini format
#ifdef Q_OS_WIN
  m_settings = new QSettings(QSettings::IniFormat, QSettings::UserScope,
                             qApp->organizationName(), qApp->applicationName());
#else
  m_settings  = new QSettings;
#endif

#if defined(Q_OS_WIN)
  m_emojiFont = "Segoe UI Emoji";
#elif defined(Q_OS_LINUX)
  m_emojiFont = "NotoColorEmoji";
#elif defined(Q_OS_MAC)
  m_emojiFont = "Apple Color Emoji";
#endif

  m_settingsDialog = new Settings(this, m_settings);
  m_ui->searchLine->installEventFilter(this);

  connect(m_ui->searchLine, &QLineEdit::returnPressed, this,
          &MainWindow::searchWord);
  connect(m_ui->searchLine, &QLineEdit::textChanged, this,
          &MainWindow::loadMatches);
  connect(m_ui->matchesView, &QListView::activated, this,
          &MainWindow::searchItem);
  connect(m_ui->matchesView, &QListView::clicked, this,
          &MainWindow::searchItem);
  connect(m_ui->matchesView, &QListView::doubleClicked, this,
          &MainWindow::copyWordToClipboard);
  connect(m_ui->matchesView->selectionModel(),
          &QItemSelectionModel::selectionChanged, this,
          &MainWindow::handleSelectionChanged);
  connect(m_ui->resultBrowser, &QTextBrowser::anchorClicked, this,
          &MainWindow::openLink);
  connect(
      m_ui->dictComboBox,
      static_cast<void (QComboBox::*)(const QString&)>(&QComboBox::activated),
      this, &MainWindow::loadDictionary);
  connect(&m_watcher, &QFutureWatcher<bool>::finished, this,
          &MainWindow::dictionaryLoaded);
  connect(m_ui->settingsButton, &QAbstractButton::clicked, this,
          &MainWindow::showSettingsDialog);

  new QShortcut(QKeySequence(Qt::Key_Escape), this, SLOT(clearAndFocus()));
  new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_L), m_ui->searchLine,
                SLOT(setFocus()));
  new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_M), m_ui->matchesView,
                SLOT(setFocus()));

#ifdef Q_OS_OSX
  m_ui->matchesView->setAttribute(Qt::WA_MacShowFocusRect, 0);
#endif
}

MainWindow::~MainWindow()
{
  delete m_ui;
  m_ui = nullptr;

  delete m_currentDict;
  m_currentDict = nullptr;

  delete m_model;
  m_model = nullptr;

  delete m_settings;
  m_settings = nullptr;

  delete m_settingsDialog;
  m_settingsDialog = nullptr;
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
  if (event->type() == QEvent::KeyPress) {
    const QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
    if (obj == m_ui->searchLine) {
      if (keyEvent->key() == Qt::Key_Down) {
        m_ui->matchesView->setFocus();
        m_ui->matchesView->setCurrentIndex(
            m_ui->matchesView->model()->index(0, 0));
      }
    }

#ifdef AUTOTEST
    if (keyEvent->key() == Qt::Key_Escape)
      m_stopTesting = true;
#endif
  }
  return false;
}

void MainWindow::closeEvent(QCloseEvent* ev)
{
  m_settings->setValue("mainwindow/geometry", geometry());
  m_settings->setValue("mainwindow/splitterSizes", m_ui->splitter->saveState());
  m_settings->setValue("viewer/lastDictionary",
                       m_ui->dictComboBox->currentText());

  QWidget::closeEvent(ev);
}

void MainWindow::showEvent(QShowEvent* ev)
{
  if (!ev->spontaneous()) {
    QRect rect = m_settings->value("mainwindow/geometry", QRect()).toRect();
    QByteArray splitterSizes =
        m_settings->value("mainwindow/splitterSizes", QByteArray())
            .toByteArray();

    QString lastDictionary =
        m_settings->value("viewer/lastDictionary", QString()).toString();

    m_fontName = m_settings->value("viewer/fontName", "Consolas").toString();
    m_fontSize = m_settings->value("viewer/fontSize", 18).toInt();
    m_dictPath = m_settings
                     ->value("viewer/dictPath",
                             QString("%1/Dictionaries").arg(QDir::homePath()))
                     .toString();

    QString deviceSerial =
        m_settings->value("viewer/deviceSerial", QString()).toString();

    if (!deviceSerial.isEmpty()) {
      qDebug() << "Device serial number:" << deviceSerial;
      m_deviceSerial = deviceSerial;
    }

    m_ui->resultBrowser->document()->setDefaultStyleSheet(
        QString("* {font-size: %1px; font-family:%2 }")
            .arg(m_fontSize)
            .arg(m_fontName));

    if (!rect.isEmpty())
      setGeometry(rect);
    else {
      setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(),
                                      qApp->screens()[0]->availableGeometry()));
    }

    if (!splitterSizes.isEmpty())
      m_ui->splitter->restoreState(splitterSizes);

    if (!lastDictionary.isNull())
      loadDictionary(lastDictionary);
    else
      loadDictionary(m_ui->dictComboBox->currentText());
  }

  QWidget::showEvent(ev);
}

void MainWindow::clearAndFocus()
{
  m_ui->searchLine->clear();
  m_ui->searchLine->setFocus();
}

void MainWindow::dictionaryLoaded()
{
  MOBI_RET result = m_future.result();

  if (result != MOBI_SUCCESS) {
    QMessageBox::critical(
        this, "Error opening dictionary",
        QString("Error code %1: %2").arg(result).arg(libmobi_msg(result)));

    setWindowTitle("Mobidict");
    delete m_currentDict;
    m_currentDict     = nullptr;
    m_currentDictName = QString();
  }
  else {
    setWindowTitle(m_currentDict->title());
    m_ui->searchLine->setEnabled(true);

    // Populate the list widget
    loadMatches(QString());

    // and select the first item
    m_ui->matchesView->setCurrentIndex(m_ui->matchesView->model()->index(0, 0));

#ifdef SELF_TEST
    selfTest();
#endif
  }

  m_ui->dictComboBox->setEnabled(true);
}

void MainWindow::discoverDictionaries()
{
  QDir dir(m_settings
               ->value("viewer/dictPath",
                       QString("%1/Dictionaries").arg(QDir::homePath()))
               .toString());
  QStringList formats;
  formats << "*.azw"
          << "*.mobi";

  QStringList dictionaries =
      dir.entryList(formats, QDir::Files | QDir::Readable, QDir::Name);

  if (dictionaries.isEmpty()) {
    QMessageBox::critical(
        nullptr, "No dictionary found",
        QString("No dictionary file (in azw/mobi format) found under "
                "<b>%1</b>, you might want to change the dictionary folder "
                "under the settings.")
            .arg(m_settings
                     ->value("viewer/dictPath",
                             QString("%1/Dictionaries").arg(QDir::homePath()))
                     .toString()));
    return;
  }

  for (auto dict : dictionaries)
    m_ui->dictComboBox->addItem(dict);

  return;
}

void MainWindow::loadDictionary(const QString& text)
{
  if (text.isEmpty())
    return;

  if (m_currentDictName == text)
    return;

  if (m_currentDict)
    delete m_currentDict;

  m_currentDictName = text;
  m_currentDict =
      new MobiDict(m_settings
                       ->value("viewer/dictPath",
                               QString("%1/Dictionaries").arg(QDir::homePath()))
                       .toString(),
                   m_deviceSerial);
  m_future = QtConcurrent::run(m_currentDict, &MobiDict::open);
  m_watcher.setFuture(m_future);

  m_ui->searchLine->setEnabled(false);
  m_ui->dictComboBox->setEnabled(false);
  m_ui->dictComboBox->setCurrentText(text);
  m_ui->resultBrowser->clear();

  setWindowTitle(QString("Loading %1 ...").arg(text));
}

void MainWindow::searchWord()
{
  QString word = m_ui->searchLine->text();
  if (word.isEmpty())
    return;

  m_html = m_currentDict->lookupWord(word);

  if (m_html.isNull())
    m_html = QString(
                 "<br><br><center><font face='%1' "
                 "size='+6'>🤔</font><br><br></span> The "
                 "word <b>\"%2\"</b> is not found in the dictionary.</center>")
                 .arg(m_emojiFont)
                 .arg(word);
  else
    createResources(m_html);

  m_ui->resultBrowser->setHtml(m_html);
}

void MainWindow::handleSelectionChanged(const QItemSelection& selection)
{
  searchItem(selection.indexes().first());
}

void MainWindow::searchItem(const QModelIndex& index)
{
  m_html = m_currentDict->lookupWord(index.data().toString());
  createResources(m_html);

  m_ui->resultBrowser->setHtml(m_html);
}

void MainWindow::loadMatches(const QString& word)
{
  m_model->setStringList(
      m_currentDict->words().filter(word, Qt::CaseInsensitive));
}

void MainWindow::createResources(const QString& html)
{
  QRegularExpression re("src=\"(\\d+)\"");
  QMap<QString, QImage*> resourceMap;
  size_t uid = 0;

  QRegularExpressionMatchIterator i = re.globalMatch(html);
  while (i.hasNext()) {
    QRegularExpressionMatch match = i.next();
    QString word                  = match.captured(1);

    if (resourceMap.constFind(word) != resourceMap.constEnd())
      continue;

    uid            = word.toUInt(nullptr, 10);
    MOBIPart* flow = m_currentDict->getResourceByUid(uid);

    if (flow != nullptr) {
      switch (flow->type) {
        case MOBIFiletype::T_SVG:
        case MOBIFiletype::T_JPG:
        case MOBIFiletype::T_GIF:
        case MOBIFiletype::T_PNG:
        case MOBIFiletype::T_BMP: {
          QImage* img  = new QImage;
          bool success = img->loadFromData(
              QByteArray((const char*)flow->data, flow->size));
          if (!success) {
            qWarning() << "Failed to load image for" << match;
            delete img;
            img = nullptr;
          }
          else
            resourceMap[word] = img;
        } break;
        default:
          break;
      }
    }
    else
      qWarning() << "Failed to get a resource for" << word;
  }

  m_ui->resultBrowser->setResourceMap(resourceMap);
}

void MainWindow::openLink(const QUrl& link)
{
  QString match = m_currentDict->resolveLink(link.toString());

  // TODO: Have to provide feedback for broken links
  if (!match.isEmpty()) {
    m_html = m_currentDict->lookupWord(match);
    createResources(m_html);
    m_ui->resultBrowser->setHtml(m_html);
  }
}

void MainWindow::showSettingsDialog()
{
  m_settingsDialog->exec();

  // Apply possible new values
  m_deviceSerial =
      m_settings->value("viewer/deviceSerial", QString()).toString();

  QString fontName =
      m_settings->value("viewer/fontName", "Consolas").toString();
  int fontSize = m_settings->value("viewer/fontSize", 18).toInt();

  if (fontName != m_fontName || fontSize != m_fontSize) {
    m_fontName = fontName;
    m_fontSize = fontSize;
    m_ui->resultBrowser->document()->setDefaultStyleSheet(
        QString("* {font-size: %1px; font-family:%2 }")
            .arg(m_fontSize)
            .arg(m_fontName));

    // Reload the entry
    m_ui->resultBrowser->setHtml(m_html);
  }

  QString dictPath =
      m_settings
          ->value("viewer/dictPath",
                  QString("%1/Dictionaries").arg(QDir::homePath()))
          .toString();
  if (dictPath != m_dictPath) {
    discoverDictionaries();
    loadDictionary(m_ui->dictComboBox->currentText());
  }
}

void MainWindow::copyWordToClipboard(const QModelIndex& index)
{
  QApplication::clipboard()->setText(index.data().toString());
}

#ifdef AUTOTEST
void MainWindow::selfTest()
{
  m_stopTesting = false;

  QApplication::processEvents();
  for (int i = 0; i < m_ui->matchesView->model()->rowCount(); ++i) {
    if (m_stopTesting)
      break;

    m_ui->matchesView->setCurrentIndex(m_ui->matchesView->model()->index(i, 0));
    QApplication::processEvents();
  }
}
#endif
