#include "htmlbrowser.h"

#include <QDebug>

HtmlBrowser::HtmlBrowser(QWidget* parent) : QTextBrowser(parent) {}

HtmlBrowser::~HtmlBrowser()
{
  qDeleteAll(m_resourceMap);
}

void HtmlBrowser::setText(const QString& text)
{
  QTextBrowser::setText(text);
}

void HtmlBrowser::setResourceMap(QMap<QString, QImage*> resourceMap)
{
  if (!m_resourceMap.empty()) {
    qDeleteAll(m_resourceMap);
    m_resourceMap.clear();
  }

  m_resourceMap = resourceMap;
}

QVariant HtmlBrowser::loadResource(int type, const QUrl& name)
{
  // qDebug() << name.toString();
  if (m_resourceMap.constFind(name.toString()) == m_resourceMap.constEnd()) {
    qWarning() << "Failed to find resource" << name.toString();
    return QTextBrowser::loadResource(type, name);
  }
  else
    return QVariant::fromValue(*m_resourceMap[name.toString()]);
}
