#include "NotificationWindow.h"
#include <QVBoxLayout>
#include <QLabel>

NotificationWindow::NotificationWindow(const Notification &n, QWidget *parent)
    : QWidget(parent, Qt::Window) {
    setWindowTitle(tr("Notification Details"));
    resize(400, 300);

    QVBoxLayout *layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(tr("<b>Title:</b> %1").arg(n.title.toHtmlEscaped())));
    layout->addWidget(new QLabel(tr("<b>Repository:</b> %1").arg(n.repository.toHtmlEscaped())));
    layout->addWidget(new QLabel(tr("<b>Type:</b> %1").arg(n.type.toHtmlEscaped())));
    layout->addWidget(new QLabel(tr("<b>Updated At:</b> %1").arg(n.updatedAt.toHtmlEscaped())));

    QString url = n.htmlUrl.isEmpty() ? n.url : n.htmlUrl;
    QLabel *urlLabel = new QLabel(tr("<b>URL:</b> <a href=\"%1\">%1</a>").arg(url.toHtmlEscaped()));
    urlLabel->setOpenExternalLinks(true);
    layout->addWidget(urlLabel);

    layout->addStretch();
}
