// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WALLETMODELTRANSACTION_H
#define WALLETMODELTRANSACTION_H

#include "walletmodel.h"

#include <QObject>

class SendCoinsRecipient;

class CReserveKey;
class CWallet;
class CWalletTx;

/** Data model for a walletmodel transaction. */
class WalletModelTransaction
{
public:
    explicit WalletModelTransaction(const QList<SendCoinsRecipient> &recipients);
    ~WalletModelTransaction();

    QList<SendCoinsRecipient> getRecipients();

    void setTransactionFee(qint64 newFee);
    qint64 getTransactionFee();

    qint64 getTotalTransactionAmount();

private:
    const QList<SendCoinsRecipient> recipients;
    qint64 fee;
};

#endif // WALLETMODELTRANSACTION_H
