// Copyright (c) 2015 The Pebblecoin developers
// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WALLETMODELTRANSACTION_H
#define WALLETMODELTRANSACTION_H

#include <QObject>
#include <QString>

#include "common/types.h"

#include "walletmodel.h"

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

    QList<SendCoinsRecipient> getRecipients() const;

    void setTransactionFee(qint64 newFee);
    qint64 getTransactionFee() const;

    qint64 getTotalTransactionAmount() const;
    
    void setFakeOuts(qint64 min, qint64 desired);
    void getFakeOuts(qint64& min, qint64& desired) const;

    void setRegisteringDelegate(cryptonote::delegate_id_t delegateId, qint64 registrationFee);
    bool getRegisteringDelegate(cryptonote::delegate_id_t& delegateId, qint64& registrationFee) const;
    
    void setPaymentId(const QString& paymentId);
    QString getPaymentId() const;

private:
    const QList<SendCoinsRecipient> recipients;
    qint64 fee;
    qint64 minFakeOuts;
    qint64 desiredFakeOuts;
    
    bool registeringDelegate;
    cryptonote::delegate_id_t delegateId;
    qint64 registrationFee;
    
    QString paymentId;
};

#endif // WALLETMODELTRANSACTION_H
