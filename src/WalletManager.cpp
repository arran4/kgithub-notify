#include "WalletManager.h"
#include <KWallet>

const QString WalletManager::FOLDER_NAME = "Kgithub-notify";
const QString WalletManager::KEY_NAME = "token";

QString WalletManager::loadToken() {
    KWallet::Wallet *wallet = KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), 0, KWallet::Wallet::Synchronous);
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

void WalletManager::saveToken(const QString &token) {
    KWallet::Wallet *wallet = KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), 0, KWallet::Wallet::Synchronous);
    if (wallet) {
        if (!wallet->hasFolder(FOLDER_NAME)) {
             wallet->createFolder(FOLDER_NAME);
        }
        wallet->setFolder(FOLDER_NAME);
        wallet->writePassword(KEY_NAME, token);
        delete wallet;
    }
}
