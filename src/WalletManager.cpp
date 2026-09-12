#include "WalletManager.h"

#include <KWallet>
#include <QFutureInterface>
#include <QObject>

namespace {
const QString FOLDER_NAME = "Kgithub-notify";
const QString KEY_NAME = "token";
}  // namespace

class KWalletBackend : public WalletBackend, public QObject {
   public:
    QFuture<WalletResult> loadTokenAsync() override;
    QFuture<WalletResult> saveTokenAsync(const QString& token) override;
    QFuture<WalletResult> clearTokenAsync() override;
};

WalletBackend* WalletManager::s_backend = nullptr;

void WalletManager::setBackend(WalletBackend* backend) { s_backend = backend; }

WalletBackend* getBackend() {
    if (!WalletManager::s_backend) {
        static KWalletBackend defaultBackend;
        return &defaultBackend;
    }
    return WalletManager::s_backend;
}

class WalletLoader : public QObject {
    Q_OBJECT
   public:
    WalletLoader() { interface.reportStarted(); }

    QFuture<WalletResult> start() {
        wallet = KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), 0, KWallet::Wallet::Asynchronous);
        if (wallet) {
            connect(wallet, &KWallet::Wallet::walletOpened, this, &WalletLoader::onWalletOpened);
        } else {
            interface.reportResult({false, "", "Failed to open KWallet asynchronously"});
            interface.reportFinished();
            deleteLater();
        }
        return interface.future();
    }

   private slots:
    void onWalletOpened(bool success) {
        if (!success || !wallet) {
            interface.reportResult({false, "", "Failed to open KWallet"});
        } else {
            if (!wallet->hasFolder(FOLDER_NAME)) {
                if (!wallet->createFolder(FOLDER_NAME)) {
                    interface.reportResult({false, "", "Failed to create KWallet folder"});
                    interface.reportFinished();
                    cleanup();
                    return;
                }
            }
            if (!wallet->setFolder(FOLDER_NAME)) {
                interface.reportResult({false, "", "Failed to set KWallet folder"});
                interface.reportFinished();
                cleanup();
                return;
            }
            QString token;
            if (wallet->readPassword(KEY_NAME, token) == 0) {
                interface.reportResult({true, token, ""});
            } else {
                interface.reportResult({false, "", "Failed to read token from KWallet"});
            }
        }
        interface.reportFinished();
        cleanup();
    }

   private:
    void cleanup() {
        if (wallet) {
            wallet->deleteLater();
            wallet = nullptr;
        }
        deleteLater();
    }

    KWallet::Wallet* wallet = nullptr;
    QFutureInterface<WalletResult> interface;
};

class WalletSaver : public QObject {
    Q_OBJECT
   public:
    WalletSaver(const QString& token, bool clear) : m_token(token), m_clear(clear) { interface.reportStarted(); }

    QFuture<WalletResult> start() {
        wallet = KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), 0, KWallet::Wallet::Asynchronous);
        if (wallet) {
            connect(wallet, &KWallet::Wallet::walletOpened, this, &WalletSaver::onWalletOpened);
        } else {
            interface.reportResult({false, "", "Failed to open KWallet asynchronously"});
            interface.reportFinished();
            deleteLater();
        }
        return interface.future();
    }

   private slots:
    void onWalletOpened(bool success) {
        if (!success || !wallet) {
            interface.reportResult({false, "", "Failed to open KWallet"});
        } else {
            if (!wallet->hasFolder(FOLDER_NAME)) {
                if (!wallet->createFolder(FOLDER_NAME)) {
                    interface.reportResult({false, "", "Failed to create KWallet folder"});
                    interface.reportFinished();
                    cleanup();
                    return;
                }
            }
            if (!wallet->setFolder(FOLDER_NAME)) {
                interface.reportResult({false, "", "Failed to set KWallet folder"});
                interface.reportFinished();
                cleanup();
                return;
            }

            if (m_clear) {
                if (wallet->hasEntry(KEY_NAME)) {
                    if (wallet->removeEntry(KEY_NAME) != 0) {
                        interface.reportResult({false, "", "Failed to remove token from KWallet"});
                    } else {
                        interface.reportResult({true, "", ""});  // Success
                    }
                } else {
                    interface.reportResult({true, "", ""});  // Success
                }
            } else {
                if (wallet->writePassword(KEY_NAME, m_token) != 0) {
                    interface.reportResult({false, "", "Failed to write token to KWallet"});
                } else {
                    interface.reportResult({true, "", ""});  // Success
                }
            }
        }
        interface.reportFinished();
        cleanup();
    }

   private:
    void cleanup() {
        if (wallet) {
            wallet->deleteLater();
            wallet = nullptr;
        }
        deleteLater();
    }

    KWallet::Wallet* wallet = nullptr;
    QFutureInterface<WalletResult> interface;
    QString m_token;
    bool m_clear;
};

QFuture<WalletResult> KWalletBackend::loadTokenAsync() {
    WalletLoader* loader = new WalletLoader();
    return loader->start();
}

QFuture<WalletResult> KWalletBackend::saveTokenAsync(const QString& token) {
    WalletSaver* saver = new WalletSaver(token, false);
    return saver->start();
}

QFuture<WalletResult> KWalletBackend::clearTokenAsync() {
    WalletSaver* saver = new WalletSaver(QString(), true);
    return saver->start();
}

QString WalletManager::loadToken() {
    KWallet::Wallet* wallet =
        KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), 0, KWallet::Wallet::Synchronous);
    if (wallet) {
        wallet->setFolder(FOLDER_NAME);
        QString token;
        wallet->readPassword(KEY_NAME, token);
        delete wallet;
        return token;
    }
    return QString();
}

QFuture<WalletResult> WalletManager::loadTokenAsync() { return getBackend()->loadTokenAsync(); }

QFuture<WalletResult> WalletManager::saveTokenAsync(const QString& token) {
    return getBackend()->saveTokenAsync(token);
}

QFuture<WalletResult> WalletManager::clearTokenAsync() { return getBackend()->clearTokenAsync(); }

#include "WalletManager.moc"
