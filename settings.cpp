#include "settings.h"

#include <QFileDialog>
#include <QSettings>

Settings::Settings(QWidget* parent)
    : QDialog(parent), m_ui(new Ui::Settings)
{
  m_ui->setupUi(this);

// On windows force ini format
#ifdef Q_OS_WIN
  m_settings = new QSettings(QSettings::IniFormat, QSettings::UserScope,
                             qApp->organizationName(), qApp->applicationName());
#else
  m_settings  = new QSettings;
#endif

  for (const auto& point : QFontDatabase::standardSizes())
    m_ui->pointComboBox->addItem(QString::number(point));

  m_ui->serialNumber->setToolTip(
      "For <b>your own</b> dictionaries with DRM, enter your e-reader's serial "
      "number here.");

  connect(this, &QDialog::accepted, this, &Settings::saveSettings);
  connect(this->m_ui->dictDirectoryButton, &QPushButton::clicked, this,
          &Settings::selectDictDirectory);

  loadSettings();
}

Settings::~Settings()
{
  delete m_ui;
  m_ui = nullptr;
}

void Settings::loadSettings()
{
  m_fontName = m_settings->value("viewer/fontName", "Consolas").toString();
  m_fontSize = m_settings->value("viewer/fontSize", 18).toInt();
  m_deviceSerial =
      m_settings->value("viewer/deviceSerial", QString()).toString();
  m_dictPath = m_settings
                   ->value("viewer/dictPath",
                           QString("%1/Dictionaries").arg(QDir::homePath()))
                   .toString();

  m_ui->fontComboBox->setCurrentFont(QFont(m_fontName, m_fontSize));
  m_ui->serialNumber->setText(m_deviceSerial);
  m_ui->pointComboBox->setCurrentText(QString::number(m_fontSize));
  m_ui->dictDirectoryLabel->setText(m_dictPath);
}

void Settings::saveSettings()
{
  QString fontName     = m_ui->fontComboBox->currentFont().family();
  int pointSize        = m_ui->pointComboBox->currentText().toUInt(nullptr);
  QString deviceSerial = m_ui->serialNumber->text().remove(' ');
  QString dictPath     = m_ui->dictDirectoryLabel->text();

  m_settings->setValue("viewer/fontName", fontName);
  m_settings->setValue("viewer/fontSize", pointSize);
  m_settings->setValue("viewer/deviceSerial", deviceSerial);
  m_settings->setValue("viewer/dictPath", dictPath);
  m_settings->sync();
}

void Settings::selectDictDirectory()
{
  QString dir = QFileDialog::getExistingDirectory(
      this, "Select dictionary folder",
      QString("%1/Dictionaries").arg(QDir::homePath()));
  m_ui->dictDirectoryLabel->setText(dir);
}

const QString& Settings::fontName()
{
  return m_fontName;
}

const int& Settings::fontSize()
{
  return m_fontSize;
}

const QString& Settings::deviceSerial()
{
  return m_deviceSerial;
}

const QString& Settings::dictPath()
{
  return m_dictPath;
}

const QRect& Settings::windowGeometry() {

}

void Settings::saveWindowGeometry(const QRect& rect) {
  m_settings->setValue("mainwindow/geometry", rect);
}

void Settings::saveSplitterSizes(const QByteArray& state) {
  m_settings->setValue("mainwindow/splitterSizes", state);
}

void Settings::saveLastDictionary(const QString& lastDictionary) {
  m_settings->setValue("viewer/lastDictionary", lastDictionary);
}