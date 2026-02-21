#ifndef WALLETMANAGER_H
#define WALLETMANAGER_H

#include <QString>

class WalletManager {
public:
    static QString loadToken();
    static void saveToken(const QString &token);

private:
    static const QString FOLDER_NAME;
    static const QString KEY_NAME;
};

#endif // WALLETMANAGER_H
