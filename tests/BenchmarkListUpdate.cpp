#include <QApplication>
#include <QListWidget>
#include <QListWidgetItem>
#include <QElapsedTimer>
#include <QDebug>
#include "../src/NotificationItemWidget.h"
#include "../src/GitHubClient.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // We need a widget. In headless, layout calculations still happen.
    QListWidget listWidget;
    listWidget.resize(400, 600);
    // listWidget.show(); // Avoid showing window in headless environment if possible, or maybe it's fine.

    QList<Notification> notifications;
    for (int i = 0; i < 1000; ++i) {
        Notification n;
        n.id = QString::number(i);
        n.title = "Issue " + QString::number(i);
        n.type = "Issue";
        n.repository = "owner/repo";
        n.url = "https://api.github.com/repos/owner/repo/issues/" + QString::number(i);
        n.updatedAt = "2023-01-01T00:00:00Z";
        n.unread = true;
        notifications.append(n);
    }

    qDebug() << "Benchmarking list update with" << notifications.size() << "items...";

    // Run Baseline
    {
        listWidget.clear();
        QElapsedTimer timer;
        timer.start();

        for (const Notification &n : notifications) {
            QListWidgetItem *item = new QListWidgetItem();
            NotificationItemWidget *widget = new NotificationItemWidget(n);

            item->setData(Qt::UserRole, n.url);
            item->setData(Qt::UserRole + 1, n.id);
            item->setData(Qt::UserRole + 2, n.title);
            item->setData(Qt::UserRole + 3, n.repository);
            item->setSizeHint(widget->sizeHint());

            listWidget.addItem(item);
            listWidget.setItemWidget(item, widget);
        }

        // Force layout processing
        app.processEvents();

        qint64 elapsed = timer.elapsed();
        qDebug() << "Baseline (updates enabled):" << elapsed << "ms";
    }

    // Run Optimized
    {
        listWidget.clear();
        QElapsedTimer timer;
        timer.start();

        listWidget.setUpdatesEnabled(false); // OPTIMIZATION START

        for (const Notification &n : notifications) {
            QListWidgetItem *item = new QListWidgetItem();
            NotificationItemWidget *widget = new NotificationItemWidget(n);

            item->setData(Qt::UserRole, n.url);
            item->setData(Qt::UserRole + 1, n.id);
            item->setData(Qt::UserRole + 2, n.title);
            item->setData(Qt::UserRole + 3, n.repository);
            item->setSizeHint(widget->sizeHint());

            listWidget.addItem(item);
            listWidget.setItemWidget(item, widget);
        }

        listWidget.setUpdatesEnabled(true); // OPTIMIZATION END

        // Force layout processing
        app.processEvents();

        qint64 elapsed = timer.elapsed();
        qDebug() << "Optimized (updates disabled):" << elapsed << "ms";
    }

    return 0;
}
