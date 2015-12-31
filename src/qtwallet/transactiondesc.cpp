// Copyright (c) 2011-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "transactiondesc.h"

#include "bitcoinunits.h"
#include "guiutil.h"

#include "interface/base58.h"
//#include "db.h"
#include "interface/main.h"
#include "transactionrecord.h"
#include "interface/wallet.h"

#include "common/ui_interface.h"

#include <stdint.h>
#include <string>

#include <boost/foreach.hpp>

QString TransactionDesc::FormatTxStatus(const CWalletTx& wtx)
{
    return "TransactionDesc::FormatTxStatus NYI";
    /*AssertLockHeld(cs_main);
    if (!IsFinalTx(wtx, chainActive.Height() + 1))
    {
        if (wtx.nLockTime < LOCKTIME_THRESHOLD)
            return tr("Open for %n more block(s)", "", wtx.nLockTime - chainActive.Height());
        else
            return tr("Open until %1").arg(GUIUtil::dateTimeStr(wtx.nLockTime));
    }
    else
    {
        int nDepth = wtx.GetDepthInMainChain();
        if (nDepth < 0)
            return tr("conflicted");
        else if (GetAdjustedTime() - wtx.nTimeReceived > 2 * 60 && wtx.GetRequestCount() == 0)
            return tr("%1/offline").arg(nDepth);
        else if (nDepth < 6)
            return tr("%1/unconfirmed").arg(nDepth);
        else
            return tr("%1 confirmations").arg(nDepth);
    }*/
}

QString TransactionDesc::toHTML(CWallet *wallet, CWalletTx &wtx, int vout, int unit)
{
    QString strHTML;

    strHTML.reserve(4000);
    strHTML += "<html><font face='verdana, arial, helvetica, sans-serif'>";
    
    //
    // Status
    //
    // TODO

    strHTML += "<b>" + tr("Date") + ":</b> " + (wtx.nTimestamp ? GUIUtil::dateTimeStr(wtx.nTimestamp) : "") + "<br>";

    //
    // From
    //
    if (wtx.IsCoinBase())
    {
        strHTML += "<b>" + tr("Source") + ":</b> " + tr("Generated") + "<br>";
    }
    else
    {
        strHTML += "<b>" + tr("Source") + ":</b> " + tr("Anonymous Pebblecoin Address") + "<br>";
    }
    
    //
    // To
    //
    // TODO
    
    //
    // Amount
    //
    uint64_t nMined, nCredit, nDebit;
    wtx.GetCreditDebit(nMined, nCredit, nDebit);
    
    if (nMined) {
        strHTML += "<b>" + tr("Mined") + ":</b> " + BitcoinUnits::formatWithUnit(unit, nMined) + "<br>";;
    }
    if (nCredit) {
        strHTML += "<b>" + tr("Credit") + ":</b> " + BitcoinUnits::formatWithUnit(unit, nCredit) + "<br>";;
    }
    if (nDebit) {
        strHTML += "<b>" + tr("Debit") + ":</b> " + BitcoinUnits::formatWithUnit(unit, nDebit) + "<br>";;
    }
    
    int64_t nNet = 0;
    nNet += nMined;
    nNet += nCredit;
    nNet -= nDebit;
    
    strHTML += "<b>" + tr("Net amount") + ":</b> " + BitcoinUnits::formatWithUnit(unit, nNet, true) + "<br>";

    strHTML += "<b>" + tr("Transaction ID") + ":</b> " + QString::fromStdString(wtx.txHash) + "<br>";
    
    if (wtx.IsPayment(*wallet))
    {
        std::string paymentId;
        tools::wallet2::payment_details pd;
        wtx.GetPaymentInfo(*wallet, paymentId, pd);
        
        strHTML += "<b>" + tr("Payment ID") + ":</b> " + QString::fromStdString(paymentId) + "<br>";
    }

    return strHTML;
}
