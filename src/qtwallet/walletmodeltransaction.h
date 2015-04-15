// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WALLETMODELTRANSACTION_H
#define WALLETMODELTRANSACTION_H

#include "walletmodel.h"

#include <QObject>

#include "cryptonote_core/delegate_types.h"

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
    
    void setFakeOuts(qint64 min, qint64 desired);
    void getFakeOuts(qint64& min, qint64& desired);

    void setRegisteringDelegate(cryptonote::delegate_id_t delegateId, qint64 registrationFee);
    
    bool getRegisteringDelegate(cryptonote::delegate_id_t& delegateId, qint64& registrationFee);

private:
    const QList<SendCoinsRecipient> recipients;
    qint64 fee;
    qint64 minFakeOuts;
    qint64 desiredFakeOuts;
    
    bool registeringDelegate;
    cryptonote::delegate_id_t delegateId;
    qint64 registrationFee;
};

#endif // WALLETMODELTRANSACTION_H
