#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QFutureWatcher>
#include <QSettings>
#include <QStringListModel>
#include <QWidget>

#include "mobidict.h"
#include "ui_mainwindow.h"

class Settings;

class MainWindow : public QWidget {
  Q_OBJECT

 public:
  MainWindow();
  ~MainWindow();

  void discoverDictionaries();
  void loadMatches(const QString&);
  void searchItem(const QModelIndex&);
  void searchWord();

 public slots:
  void dictionaryLoaded();
  void loadDictionary(const QString&);
  void openLink(const QUrl& link);
  void showSettingsDialog();
  void copyWordToClipboard(const QModelIndex&);
  void clearAndFocus();
  void handleSelectionChanged(const QItemSelection&);

 protected:
  bool eventFilter(QObject* obj, QEvent* ev) override;
  void closeEvent(QCloseEvent*) override;
  void showEvent(QShowEvent*) override;

 private:
  Ui::MainWindow* m_ui;

  MobiDict* m_currentDict;
  QString m_currentDictName;
  QString m_deviceSerial;
  QString m_emojiFont;
  QString m_html;
  QString m_fontName;
  QString m_dictPath;
  int m_fontSize;
  QStringListModel* m_model;

  QFutureWatcher<MOBI_RET> m_watcher;
  QFuture<MOBI_RET> m_future;

  Settings* m_settings;

  void createResources(const QString&);

#ifdef AUTOTEST
  void selfTest();
  bool m_stopTesting;
#endif
};

#endif
