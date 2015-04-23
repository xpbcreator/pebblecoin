// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletmodeltransaction.h"

#include "interface/wallet.h"
#include "interface/placeholders.h"

WalletModelTransaction::WalletModelTransaction(const QList<SendCoinsRecipient> &recipients)
    : recipients(recipients)
    , fee(0)
    , registeringDelegate(false)
{
}

WalletModelTransaction::~WalletModelTransaction()
{
}

QList<SendCoinsRecipient> WalletModelTransaction::getRecipients() const
{
    return recipients;
}

qint64 WalletModelTransaction::getTransactionFee() const
{
    return fee;
}

void WalletModelTransaction::setTransactionFee(qint64 newFee)
{
    fee = newFee;
}

qint64 WalletModelTransaction::getTotalTransactionAmount() const
{
    qint64 totalTransactionAmount = 0;
    foreach(const SendCoinsRecipient &rcp, recipients)
    {
        totalTransactionAmount += rcp.amount;
    }
    if (registeringDelegate)
        totalTransactionAmount += registrationFee;
    
    return totalTransactionAmount;
}

void WalletModelTransaction::setFakeOuts(qint64 min, qint64 desired)
{
    minFakeOuts = min;
    desiredFakeOuts = desired;
}

void WalletModelTransaction::getFakeOuts(qint64& min, qint64& desired) const
{
    min = minFakeOuts;
    desired = desiredFakeOuts;
}

void WalletModelTransaction::setRegisteringDelegate(cryptonote::delegate_id_t newDelegateId, qint64 newRegistrationFee)
{
    registeringDelegate = true;
    delegateId = newDelegateId;
    registrationFee = newRegistrationFee;
}

bool WalletModelTransaction::getRegisteringDelegate(cryptonote::delegate_id_t& outDelegateId, qint64& outRegistrationFee) const
{
    if (!registeringDelegate)
        return false;
    
    outDelegateId = delegateId;
    outRegistrationFee = registrationFee;
    return true;
}

void WalletModelTransaction::setPaymentId(const QString& paymentId_in)
{
    paymentId = paymentId_in;
}

QString WalletModelTransaction::getPaymentId() const
{
    return paymentId;
}


