#include "WalletManager.h"

#include <KWallet>
#include <QFutureInterface>
#include <QObject>

namespace {
const QString FOLDER_NAME = "Kgithub-notify";
const QString KEY_NAME = "token";
}  // namespace

class WalletLoader : public QObject {
    Q_OBJECT
   public:
    WalletLoader() { interface.reportStarted(); }

    QFuture<QString> start() {
        wallet = KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), 0, KWallet::Wallet::Asynchronous);
        if (wallet) {
            connect(wallet, &KWallet::Wallet::walletOpened, this, &WalletLoader::onWalletOpened);
        } else {
            interface.reportResult(QString());
            interface.reportFinished();
            deleteLater();
        }
        return interface.future();
    }

   private slots:
    void onWalletOpened(bool success) {
        if (success && wallet) {
            if (!wallet->hasFolder(FOLDER_NAME)) {
                wallet->createFolder(FOLDER_NAME);
            }
            wallet->setFolder(FOLDER_NAME);
            QString token;
            wallet->readPassword(KEY_NAME, token);
            interface.reportResult(token);
        } else {
            interface.reportResult(QString());
        }
        interface.reportFinished();
        if (wallet) {
            wallet->deleteLater();
            wallet = nullptr;
        }
        deleteLater();
    }

   private:
    KWallet::Wallet *wallet = nullptr;
    QFutureInterface<QString> interface;
};

QString WalletManager::loadToken() {
    KWallet::Wallet *wallet =
        KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), 0, KWallet::Wallet::Synchronous);
    if (wallet) {
        if (!wallet->hasFolder(FOLDER_NAME)) {
            wallet->createFolder(FOLDER_NAME);
        }
        wallet->setFolder(FOLDER_NAME);
        QString token;
        wallet->readPassword(KEY_NAME, token);
        delete wallet;
        return token;
    }
    return QString();
}

QFuture<QString> WalletManager::loadTokenAsync() {
    WalletLoader *loader = new WalletLoader();
    return loader->start();
}

void WalletManager::saveToken(const QString &token) {
    KWallet::Wallet *wallet =
        KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), 0, KWallet::Wallet::Synchronous);
    if (wallet) {
        if (!wallet->hasFolder(FOLDER_NAME)) {
            wallet->createFolder(FOLDER_NAME);
        }
        wallet->setFolder(FOLDER_NAME);
        wallet->writePassword(KEY_NAME, token);
        delete wallet;
    }
}

#include "WalletManager.moc"
