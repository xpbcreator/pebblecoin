// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletmodeltransaction.h"

#include "interface/wallet.h"
#include "interface/placeholders.h"

WalletModelTransaction::WalletModelTransaction(const QList<SendCoinsRecipient> &recipients) :
    recipients(recipients),
    fee(0)
{
}

WalletModelTransaction::~WalletModelTransaction()
{
}

QList<SendCoinsRecipient> WalletModelTransaction::getRecipients()
{
    return recipients;
}

qint64 WalletModelTransaction::getTransactionFee()
{
    return fee;
}

void WalletModelTransaction::setTransactionFee(qint64 newFee)
{
    fee = newFee;
}

qint64 WalletModelTransaction::getTotalTransactionAmount()
{
    qint64 totalTransactionAmount = 0;
    foreach(const SendCoinsRecipient &rcp, recipients)
    {
        totalTransactionAmount += rcp.amount;
    }
    return totalTransactionAmount;
}
