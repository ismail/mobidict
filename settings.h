#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDialog>

#include "ui_settings.h"

class QSettings;
class QShowEvent;

class Settings : public QDialog {
  Q_OBJECT

 public:
  Settings(QWidget* parent, QSettings*);
  ~Settings();

 private slots:
  void saveSettings();
  void selectDictDirectory();

 protected:
  void showEvent(QShowEvent*) override;

 private:
  Ui::Settings* m_ui;
  QSettings* m_settings;
};

#endif
