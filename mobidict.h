#ifndef MOBIDICT_H
#define MOBIDICT_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QTextCodec>

#include <mobi.h>

typedef struct {
  uint32_t startPos;
  uint32_t textLength;
} MobiEntry;

class MobiDict : public QObject {
 public:
  MobiDict(const QString&, const QString&);
  ~MobiDict();

  MOBI_RET open();
  const QString& title();

  MOBIPart* getResourceByUid(const size_t& uid);

  const QList<QString> words();
  QString resolveLink(const QString&);
  QString lookupWord(const QString&);

 private:

  void sortKeys();

  MOBIData* m_mobiData;
  MOBIRawml* m_rawMarkup;

  QString m_deviceSerial;
  QString m_language;
  QString m_path;
  QString m_title;

  bool m_isCP1252;

  QHash<QString, QList<MobiEntry*>> m_wordHash;
  QList<QString> m_sortedKeys;
  QTextCodec* m_codec;
};

#endif
