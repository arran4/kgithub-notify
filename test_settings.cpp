#include <QCoreApplication>
#include <QSettings>
#include <QSet>
#include <QStringList>
#include <QDebug>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    QSettings settings("arran4", "kgithub-notify-trending");
    QSet<QString> m_selectedUrls;

    m_selectedUrls.insert("url1");
    m_selectedUrls.insert("url2");

    settings.setValue("seen_urls", QStringList(m_selectedUrls.values()));
    settings.sync();

    QSettings s2("arran4", "kgithub-notify-trending");
    QSet<QString> loaded(s2.value("seen_urls").toStringList().begin(), s2.value("seen_urls").toStringList().end());
    qDebug() << loaded;

    return 0;
}
