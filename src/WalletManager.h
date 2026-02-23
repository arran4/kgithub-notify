#ifndef WALLETMANAGER_H
#define WALLETMANAGER_H

#include <QFuture>
#include <QString>

class WalletManager {
   public:
    static QString loadToken();
    static QFuture<QString> loadTokenAsync();
    static void saveToken(const QString &token);
};

#endif  // WALLETMANAGER_H
