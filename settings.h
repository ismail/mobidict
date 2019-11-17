#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDialog>

#include "ui_settings.h"

class QSettings;
class QShowEvent;

class Settings : public QDialog {
  Q_OBJECT

 public:
  Settings(QWidget* parent);
  ~Settings();

  const QString& fontName();
  const int& fontSize();
  const QString& deviceSerial();
  const QString& dictPath();
  const QRect& windowGeometry();
  const QByteArray& splitterSizes();
  const QString& lastDictionary();

  void saveWindowGeometry(const QRect&);
  void saveSplitterSizes(const QByteArray&);
  void saveLastDictionary(const QString&);

 private slots:
  void saveSettings();
  void selectDictDirectory();

 private:
  Ui::Settings* m_ui;
  QSettings* m_settings;

  void loadSettings();

  QString m_fontName;
  int m_fontSize;
  QString m_deviceSerial;
  QString m_dictPath;
};

#endif
