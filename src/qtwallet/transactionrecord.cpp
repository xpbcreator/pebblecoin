// Copyright (c) 2011-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <stdint.h>

#include <boost/foreach.hpp>

#include "cryptonote_core/cryptonote_basic_impl.h"

#include "interface/base58.h"
#include "interface/wallet.h"
#include "interface/main.h"
#include "bitcoin/util.h"
#include "transactionrecord.h"

/* Return positive answer if transaction should be shown in list.
 */
bool TransactionRecord::showTransaction(const CWalletTx &wtx)
{
    /*if (wtx.IsCoinBase())
    {
        // Ensures we show generated coins / mined transactions at depth 1
        if (!wtx.IsInMainChain())
        {
            return false;
        }
    }*/
    return true;
}

/*
 * Decompose CWallet transaction to model transaction records.
 */
QList<TransactionRecord> TransactionRecord::decomposeTransaction(const CWallet *wallet, const CWalletTx &wtx)
{
    QList<TransactionRecord> parts;
    
    // Three parts: mined credits, non-mined credits, and debits.
    uint64_t nMined, nCredit, nDebit;
    wtx.GetCreditDebit(nMined, nCredit, nDebit);
    
    bool isPayment = wtx.IsPayment(*wallet);
    std::string paymentId;
    if (isPayment)
    {
        tools::wallet2::payment_details pd;
        wtx.GetPaymentInfo(*wallet, paymentId, pd);
    }

    LOG_PRINT_L2("inMain=" << wtx.IsInMainChain() << ", nMined=" << nMined << ", nDebit=" << nDebit << ", nCredit=" << nCredit);
    
    // Mined credits are easy
    if (nMined > 0)
    {
        auto rec = TransactionRecord(wtx.txHash, wtx.nTimestamp, TransactionRecord::Generated, "",
                                     0, nMined);
        rec.payment_id = paymentId;
        rec.idx = parts.size();
        parts.append(rec);
    }
    
    // If there's a known transfer, use it to figure out what's up
    
    if (wtx.optKnownTransfers)
    {
        auto& kd = *wtx.optKnownTransfers;
        bool fIncludeFee = true;
        LOG_PRINT_L2("kd.m_fee = " << kd.m_fee << ", m_xpb_change=" << kd.m_xpb_change);
        BOOST_FOREACH(const auto& dest, kd.m_dests)
        {
            uint64_t nThisAmount = dest.amount;
            if (fIncludeFee)
            {
                nThisAmount += kd.m_fee;
            }
            if (nThisAmount > nDebit)
            {
                LOG_PRINT_RED_L0("Known transfer amount " << cryptonote::print_money(nThisAmount) << " greater than debit amount " << cryptonote::print_money(nDebit) << "?");
                continue;
            }
            nDebit -= nThisAmount;
            
            auto rec = TransactionRecord(wtx.txHash, wtx.nTimestamp,
                                         isPayment ? TransactionRecord::PayToAddress : TransactionRecord::SendToAddress,
                                         cryptonote::get_account_address_as_str(dest.addr),
                                         -((qint64)nThisAmount), 0);
            rec.payment_id = paymentId;
            rec.idx = parts.size();

            if (wallet->GetWallet2()->is_mine(dest.addr))
            {
                rec.type = TransactionRecord::SendToSelf;
                rec.address = "";
                
                if (nCredit < dest.amount)
                {
                    LOG_PRINT_RED_L0("Known transfer to self greater than credit amount?");
                    nCredit = 0;
                }
                else
                {
                    nCredit -= dest.amount;
                }
                
                rec.debit -= dest.amount; // leave fee in if fee was included here
                rec.credit = 0;
            }
            
            parts.append(rec);
            
            LOG_PRINT_L2("accounted sending " << dest.amount << ", includedFee=?" << fIncludeFee << " to self or other");
            
            fIncludeFee = false;
        }
        
        LOG_PRINT_L2("after transfers, nDebit=" << nDebit << ", nCredit=" << nCredit);
        if (kd.m_xpb_change > nCredit)
        {
            LOG_PRINT_RED_L0("Known transfer amount change " << cryptonote::print_money(kd.m_xpb_change) << " greater than credit amount " << cryptonote::print_money(nCredit) << "?");
            nCredit = 0;
        }
        else
        {
            nCredit -= kd.m_xpb_change;
        }
        if (kd.m_xpb_change > nDebit)
        {
            LOG_PRINT_RED_L0("Known transfer amount change " << cryptonote::print_money(kd.m_xpb_change) << " greater than debit amount " << cryptonote::print_money(nDebit) << "?");
            nDebit = 0;
        }
        else
        {
            nDebit -= kd.m_xpb_change;
        }
    }
    
    // If there are left-overs...
    // If net is negative but have both credit & debit, assume user sent & got change back
    if (nCredit > 0 && nDebit > 0 && nDebit > nCredit)
    {
        auto rec = TransactionRecord(wtx.txHash, wtx.nTimestamp,
                                     wtx.IsPayment(*wallet) ? TransactionRecord::PayToOther : TransactionRecord::SendToOther,
                                     "", -((qint64)(nDebit - nCredit)), 0);
        rec.payment_id = paymentId;
        rec.idx = parts.size();
        parts.append(rec);
    }
    else
    {
        // Put them as separate transactions:
        if (nCredit > 0)
        {
            auto rec = TransactionRecord(wtx.txHash, wtx.nTimestamp,
                                         wtx.IsPayment(*wallet) ? TransactionRecord::PayFromOther : TransactionRecord::RecvFromOther,
                                         "",
                                         0, nCredit);
            rec.payment_id = paymentId;
            rec.idx = parts.size();
            parts.append(rec);
        }
        if (nDebit > 0)
        {
            auto rec = TransactionRecord(wtx.txHash, wtx.nTimestamp,
                                         isPayment ? TransactionRecord::PayToAddress : TransactionRecord::SendToAddress,
                                         "", -((qint64)nDebit), 0);
            rec.payment_id = paymentId;
            rec.idx = parts.size();
            parts.append(rec);
        }
    }
    
    /*uint64_t m_block_height;
    cryptonote::transaction m_tx;
    size_t m_internal_output_index;
    uint64_t m_global_output_index;
    bool m_spent;
    cryptonote::transaction m_spent_by_tx;
    crypto::key_image m_key_image; //TODO: key_image stored twice :(*/
    
    
    /*int64_t nTime = wtx.GetTxTime();
    int64_t nCredit = wtx.GetCredit();
    int64_t nDebit = wtx.GetDebit();
    int64_t nNet = nCredit - nDebit;
    std::string hash = wtx.GetHash();
    std::map<std::string, std::string> mapValue = wtx.mapValue;

    if (nNet > 0 || wtx.IsCoinBase())
    {
        //
        // Credit
        //
        BOOST_FOREACH(const CTxOut& txout, wtx.vout)
        {
            if(wallet->IsMine(txout))
            {
                TransactionRecord sub(hash, nTime);
                CTxDestination address;
                sub.idx = parts.size(); // sequence number
                sub.credit = txout.nValue;
                if (ExtractDestination(txout.scriptPubKey, address) && IsMine(*wallet, address))
                {
                    // Received by Bitcoin Address
                    sub.type = TransactionRecord::RecvWithAddress;
                    sub.address = CBitcoinAddress(address).ToString();
                }
                else
                {
                    // Received by IP connection (deprecated features), or a multisignature or other non-simple transaction
                    sub.type = TransactionRecord::RecvFromOther;
                    sub.address = mapValue["from"];
                }
                if (wtx.IsCoinBase())
                {
                    // Generated
                    sub.type = TransactionRecord::Generated;
                }

                parts.append(sub);
            }
        }
    }
    else
    {
        bool fAllFromMe = true;
        BOOST_FOREACH(const CTxIn& txin, wtx.vin)
            fAllFromMe = fAllFromMe && wallet->IsMine(txin);

        bool fAllToMe = true;
        BOOST_FOREACH(const CTxOut& txout, wtx.vout)
            fAllToMe = fAllToMe && wallet->IsMine(txout);

        if (fAllFromMe && fAllToMe)
        {
            // Payment to self
            int64_t nChange = wtx.GetChange();

            parts.append(TransactionRecord(hash, nTime, TransactionRecord::SendToSelf, "",
                            -(nDebit - nChange), nCredit - nChange));
        }
        else if (fAllFromMe)
        {
            //
            // Debit
            //
            int64_t nTxFee = nDebit - wtx.GetValueOut();

            for (unsigned int nOut = 0; nOut < wtx.vout.size(); nOut++)
            {
                const CTxOut& txout = wtx.vout[nOut];
                TransactionRecord sub(hash, nTime);
                sub.idx = parts.size();

                if(wallet->IsMine(txout))
                {
                    // Ignore parts sent to self, as this is usually the change
                    // from a transaction sent back to our own address.
                    continue;
                }

                CTxDestination address;
                if (ExtractDestination(txout.scriptPubKey, address))
                {
                    // Sent to Bitcoin Address
                    sub.type = TransactionRecord::SendToAddress;
                    sub.address = CBitcoinAddress(address).ToString();
                }
                else
                {
                    // Sent to IP, or other non-address transaction like OP_EVAL
                    sub.type = TransactionRecord::SendToOther;
                    sub.address = mapValue["to"];
                }

                int64_t nValue = txout.nValue;
                // Add fee to first output
                if (nTxFee > 0)
                {
                    nValue += nTxFee;
                    nTxFee = 0;
                }
                sub.debit = -nValue;

                parts.append(sub);
            }
        }
        else
        {
            //
            // Mixed debit transaction, can't break down payees
            //
            parts.append(TransactionRecord(hash, nTime, TransactionRecord::Other, "", nNet, 0));
        }
    }*/
    
    return parts;
}

void TransactionRecord::updateStatus(const CWalletTx &wtx)
{
    AssertLockHeld(cs_main);
    if (!pcore)
        return;
    if (!pwalletMain)
        return;
    
    // Determine transaction status

    // Sort order, unrecorded transactions sort to the top
    status.sortKey = strprintf("%010d-%01d-%010u-%03d",
        wtx.IsInMainChain() ? wtx.nBlockHeight : std::numeric_limits<int>::max(),
        (wtx.IsCoinBase() ? 1 : 0),
        wtx.nTimestamp,
        idx);
    status.countsForBalance = wtx.IsTrusted() && !(wtx.GetBlocksToMaturity() > 0);
    status.depth = wtx.GetDepthInMainChain();
    status.cur_daemon_blocks = DaemonProcessedHeight();
    status.cur_wallet_blocks = WalletProcessedHeight();

    /*if (!IsFinalTx(wtx, chainActive.Height() + 1))
    {
        if (wtx.nLockTime < LOCKTIME_THRESHOLD)
        {
            status.status = TransactionStatus::OpenUntilBlock;
            status.open_for = wtx.nLockTime - chainActive.Height();
        }
        else
        {
            status.status = TransactionStatus::OpenUntilDate;
            status.open_for = wtx.nLockTime;
        }
    }
    // For generated transactions, determine maturity
    else*/ if(type == TransactionRecord::Generated)
    {
        if (wtx.GetBlocksToMaturity() > 0)
        {
            status.status = TransactionStatus::Immature;

            if (wtx.IsInMainChain())
            {
                status.matures_in = wtx.GetBlocksToMaturity();

                // Check if the block was requested by anyone
                /*if (GetAdjustedTime() - wtx.nTimestamp > 2 * 60 && wtx.GetRequestCount() == 0)
                    status.status = TransactionStatus::MaturesWarning;*/
            }
            else
            {
                status.status = TransactionStatus::NotAccepted;
            }
        }
        else
        {
            status.status = TransactionStatus::Confirmed;
        }
    }
    else
    {
        if (status.depth < 0)
        {
            status.status = TransactionStatus::Conflicted;
        }
        /*else if (GetAdjustedTime() - wtx.nTimestamp > 2 * 60 && wtx.GetRequestCount() == 0)
        {
            status.status = TransactionStatus::Offline;
        }*/
        else if (status.depth == 0)
        {
            status.status = TransactionStatus::Unconfirmed;
        }
        else if (status.depth < RecommendedNumConfirmations)
        {
            status.status = TransactionStatus::Confirming;
        }
        else
        {
            status.status = TransactionStatus::Confirmed;
        }
    }
}

bool TransactionRecord::statusUpdateNeeded()
{
    AssertLockHeld(cs_main);
    if (!pcore)
        return false;
    if (!pwalletMain)
        return false;
    return (int64_t)status.cur_daemon_blocks != DaemonProcessedHeight() ||
           (int64_t)status.cur_wallet_blocks != WalletProcessedHeight();
}

QString TransactionRecord::getTxID() const
{
    return formatSubTxId(hash, idx);
}

QString TransactionRecord::formatSubTxId(const std::string &hash, int vout)
{
  return QString::fromStdString(hash + strprintf("-%03d", vout));
}

QString TransactionRecord::getPaymentID() const
{
    return QString::fromStdString(payment_id);
}
