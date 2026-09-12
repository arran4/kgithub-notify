#ifndef WALLETMANAGER_H
#define WALLETMANAGER_H

#include <QFuture>
#include <QString>

struct WalletResult {
    bool success;
    QString token;
    QString errorMessage;
};

class WalletBackend {
   public:
    virtual ~WalletBackend() = default;
    virtual QFuture<WalletResult> loadTokenAsync() = 0;
    virtual QFuture<WalletResult> saveTokenAsync(const QString& token) = 0;
    virtual QFuture<WalletResult> clearTokenAsync() = 0;
};

WalletBackend* getBackend();

class WalletManager {
   public:
    static void setBackend(WalletBackend* backend);

    static QString loadToken();
    static QFuture<WalletResult> loadTokenAsync();
    static QFuture<WalletResult> saveTokenAsync(const QString& token);
    static QFuture<WalletResult> clearTokenAsync();

   private:
    static WalletBackend* s_backend;
    friend WalletBackend* getBackend();
};

#endif  // WALLETMANAGER_H
