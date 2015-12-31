#include <boost/thread.hpp>

#include "string_tools.h"
#include "file_io_utils.h"
#include "serialization/binary_utils.h"

#include "cryptonote_config.h"
#include "cryptonote_core/tx_pool.h"
#include "cryptonote_core/cryptonote_core.h"
#include "cryptonote_core/blockchain_storage.h"
#include "wallet/wallet2.h"
#include "wallet/wallet_errors.h"

#include "bitcoin/util.h"
#include "interface/script.h"
#include "interface/main.h"
#include "interface/wallet.h"

int64_t nTransactionFee = DEFAULT_FEE;

CWalletTx::CWalletTx(const cryptonote::transaction& txIn)
{
  tx = txIn;
  txHash = GetStrHash(tx);
  nBlockHeight = 0;
  nTimestamp = GetAdjustedTime();

  CheckUpdateHeight();
}

void CWalletTx::CheckUpdateHeight()
{
  if (!pcore)
    return;
  
  if (!pcore->get_blockchain_storage().have_tx(GetCryptoHash(txHash))) {
    return;
  }
  
  auto ce = pcore->get_blockchain_storage().get_tx_chain_entry(GetCryptoHash(txHash));
  
  nBlockHeight = ce.m_keeper_block_height;
  
  crypto::hash blk_hash = pcore->get_blockchain_storage().get_block_id_by_height(nBlockHeight);
  blockHash = GetStrHash(blk_hash);
  
  cryptonote::block blk;
  if (!pcore->get_blockchain_storage().get_block_by_hash(blk_hash, blk))
    throw std::runtime_error("Couldn't find the block the tx is part of");
  
  nTimestamp = blk.timestamp;
}

bool CWalletTx::IsCoinBase() const
{
  BOOST_FOREACH(const auto& td, vecTransfersIn)
  {
    if (td.m_from_miner_tx)
      return true;
  }
  return false;
}

bool CWalletTx::IsInMainChain() const
{
  if (!pcore)
    return nBlockHeight != 0;
  
  return pcore->get_blockchain_storage().have_tx(GetCryptoHash(txHash));
}

int CWalletTx::GetDepthInMainChain() const
{
  if (!IsInMainChain())
    return 0;
  
  if (!pcore)
    return 0;
  
  return pcore->get_blockchain_storage().get_current_blockchain_height() - nBlockHeight + 1;
}

int CWalletTx::GetBlocksToMaturity() const
{
  if (!IsCoinBase())
    return 0;
  
  return std::max(0, DEFAULT_TX_SPENDABLE_AGE - GetDepthInMainChain());
}

bool CWalletTx::IsTrusted() const
{
  int nDepth = GetDepthInMainChain();
  if (nDepth >= 1)
    return true;
  
  return false;
}

bool CWalletTx::IsPayment(const CWallet& wallet) const
{
  std::string paymentId;
  tools::wallet2::payment_details paymentDetails;
  return GetPaymentInfo(wallet, paymentId, paymentDetails);
}
  
bool CWalletTx::GetPaymentInfo(const CWallet& wallet, std::string& paymentId,
                               tools::wallet2::payment_details& paymentDetails) const
{
  LOCK(wallet.cs_wallet);
  
  crypto::hash paymentIdHash;
  bool r = wallet.GetWallet2()->get_payment_for_tx(GetCryptoHash(txHash), paymentIdHash, paymentDetails);
  if (!r)
    return false;
  
  paymentId = GetStrHash(paymentIdHash);
  return true;
}

bool CWalletTx::GetCreditDebit(uint64_t& nMined, uint64_t& nCredit, uint64_t& nDebit) const
{
  LOG_PRINT_L2("GetCreditDebit() on transaction with hash " << txHash);
  nMined = 0;
  nCredit = 0;
  nDebit = 0;
  
  // if in main chain, use transfers - else trust the known transfer if there is one
  if (IsInMainChain())
  {
    BOOST_FOREACH(const auto& td, vecTransfersIn)
    {
      if (td.m_from_miner_tx)
        nMined += td.amount();
      else
        nCredit += td.amount();
    }
    BOOST_FOREACH(const auto& td, vecTransfersOut)
    {
      nDebit += td.amount();
    }
    return true;
  }
  
  if (optKnownTransfers)
  {
    auto& kd = *optKnownTransfers;
    BOOST_FOREACH(const auto& dest, kd.m_dests)
    {
      nDebit += dest.amount;
    }
    nDebit += kd.m_fee;
    nDebit += kd.m_xpb_change;
    nCredit += kd.m_xpb_change;
    
    return true;
  }
  
  return false;
}




CWallet::CWallet(const std::string& cwalletFileNameArg)
    : pwallet2(NULL)
    , cwalletFileName(cwalletFileNameArg)
    , p_wallet_task_thread(new boost::thread(boost::bind(&CWallet::WalletTaskThread, this)))
    , m_stop(false)
{
  LoadCWallet();
}

CWallet::~CWallet()
{
}


void CWallet::ProcTxUpdated(std::string txHash, bool fAdded)
{
  // Delete & re-add since parts may have changed
  if (fAdded)
  {
    NotifyTransactionChanged(this, txHash, CT_DELETED);
  }
  NotifyTransactionChanged(this, txHash, CT_NEW);
}

void CWallet::ProcTransferDetails(const tools::wallet2::transfer_details& td, bool fProcIn, bool fProcOut)
{
  LOG_PRINT_L4("LOCK(cs_wallet) ProcTransferDetails");
  LOCK(cs_wallet); // mapWallet
  LOG_PRINT_L4("LOCK(cs_wallet) ProcTransferDetails acquired");
  
  if (fProcIn)
  {
    auto mi = mapWallet.find(GetCryptoHash(td.m_tx));
    if (mi != mapWallet.end())
    {
      LOG_PRINT_L2("Added a td.m_tx to " << mi->first);
      mi->second.vecTransfersIn.push_back(td);
    }
  }
  
  if (fProcOut && td.m_spent)
  {
    auto mi = mapWallet.find(GetCryptoHash(td.m_spent_by_tx));
    if (mi != mapWallet.end())
    {
      LOG_PRINT_L2("Added a td.m_spent_by_tx to " << mi->first);
      mi->second.vecTransfersOut.push_back(td);
    }
  }
}

void CWallet::ProcKnownTransfer(const crypto::hash& tx_hash, const tools::wallet2::known_transfer_details& kd)
{
  LOG_PRINT_L4("LOCK(cs_wallet) ProcKnownTransfer");
  LOCK(cs_wallet); // mapWallet
  LOG_PRINT_L4("LOCK(cs_wallet) ProcKnownTransfer acquired");
  
  auto mi = mapWallet.find(tx_hash);
  if (mi != mapWallet.end())
  {
    LOG_PRINT_L2("Added a known transfer to " << tx_hash);
    mi->second.optKnownTransfers = kd;
  }
}

void CWallet::SyncAllTransferDetails()
{
  if (!pwallet2)
    return;
  
  LOG_PRINT_L4("LOCK(cs_wallet) SyncAllTransferDetails");
  LOCK(cs_wallet); // mapWallet
  LOG_PRINT_L4("LOCK(cs_wallet) SyncAllTransferDetails acquired");
  
  BOOST_FOREACH(auto& item, mapWallet)
  {
    item.second.vecTransfersIn.clear();
    item.second.vecTransfersOut.clear();
    item.second.optKnownTransfers = boost::none;
  }
  
  tools::wallet2::transfer_container transfers;
  pwallet2->get_transfers(transfers);
  BOOST_FOREACH(const auto& td, transfers)
  {
    ProcTransferDetails(td, true, true);
  }
  
  tools::wallet2::known_transfer_container known_transfers;
  pwallet2->get_known_transfers(known_transfers);
  BOOST_FOREACH(const auto& item, known_transfers)
  {
    ProcKnownTransfer(item.first, item.second);
  }
}


bool CWallet::AddTransaction(const cryptonote::transaction& tx)
{
  LOG_PRINT_L4("LOCK(cs_wallet) AddTransaction");
  LOCK(cs_wallet); // mapWallet
  LOG_PRINT_L4("LOCK(cs_wallet) AddTransaction acquired");
  
  auto cryptoHash = GetCryptoHash(tx);
  auto mi = mapWallet.find(cryptoHash);
  bool fNew = mi == mapWallet.end();
  LOG_PRINT_L2("CWallet::AddTransaction, fNew = " << fNew);
  if (!fNew)
  {
    if (!pwallet2)
      throw std::runtime_error("should have pwallet2 by now");
    
    tools::wallet2::known_transfer_details kd;
    if (pwallet2->get_known_transfer(cryptoHash, kd))
      ProcKnownTransfer(cryptoHash, kd);
    
    mi->second.CheckUpdateHeight();
    
    return false;
  }
  
  mapWallet[cryptoHash] = CWalletTx(tx);
  //StoreCWallet();
  return true;
}

bool CWallet::RemoveTransaction(const cryptonote::transaction& tx)
{
  LOG_PRINT_L4("LOCK(cs_wallet) RemoveTransaction");
  LOCK(cs_wallet); // mapWallet
  LOG_PRINT_L4("LOCK(cs_wallet) RemoveTransaction acquired");
  
  auto txHash = GetCryptoHash(tx);
  auto mi = mapWallet.find(txHash);
  if (mi == mapWallet.end())
  {
    LOG_PRINT_L2("CWallet::RemoveTransaction, CWalletTx wasn't present with hash = " << txHash);
    return false;
  }
  
  LOG_PRINT_L2("CWallet::RemoveTransaction, removing CWalletTx with hash = " << txHash);
  mapWallet.erase(mi);
  //StoreCWallet();
  return true;
}


bool CWallet::HasWallet2() const
{
  return pwallet2 ? true : false;
}

void CWallet::SetWallet2(tools::wallet2 *pwalletArg)
{
  if (HasWallet2())
    throw std::runtime_error("pwallet2 is already set");
  
  pwallet2 = pwalletArg;
  pwallet2->callback(this);
  
  SyncAllTransferDetails();
}

tools::wallet2 *CWallet::GetWallet2() const
{
  if (!HasWallet2())
    throw std::runtime_error("pwallet2 is not set");
  return pwallet2;
}



void CWallet::SetTxMemoryPool(cryptonote::tx_memory_pool *pmempool)
{
  pmempool->callback(this);
  
  std::list<cryptonote::transaction> txs;
  pmempool->get_transactions(txs);
  BOOST_FOREACH(const auto& tx, txs)
  {
    bool fAdded = AddTransaction(tx);
    ProcTxUpdated(GetStrHash(tx), fAdded);
  }
}


bool CWallet::SetAddressBook(const address_t& address, const std::string& strName, const std::string& strPurpose)
{
  bool fUpdated = false;
  {
    LOG_PRINT_L4("LOCK(cs_wallet) SetAddressBook");
    LOCK(cs_wallet); // mapAddressBook
    LOG_PRINT_L4("LOCK(cs_wallet) SetAddressBook acquired");
    auto mi = mapAddressBook.find(address);
    fUpdated = mi != mapAddressBook.end();
    mapAddressBook[address].name = strName;
    if (!strPurpose.empty()) /* update purpose only if requested */
      mapAddressBook[address].purpose = strPurpose;
  }
  NotifyAddressBookChanged(this, address, strName, ::IsMine(*this, address),
                           strPurpose, (fUpdated ? CT_UPDATED : CT_NEW) );
  return StoreCWallet();
}

bool CWallet::DelAddressBook(const address_t& address)
{
  {
    LOG_PRINT_L4("LOCK(cs_wallet) DelAddressBook");
    LOCK(cs_wallet); // mapAddressBook
    LOG_PRINT_L4("LOCK(cs_wallet) DelAddressBook acquired");
    mapAddressBook.erase(address);
  }
  
  NotifyAddressBookChanged(this, address, "", ::IsMine(*this, address), "", CT_DELETED);
  
  return StoreCWallet();
}


bool CWallet::RefreshOnce()
{
  LOCK(cs_refresh); // make sure only called from one thread
  
  size_t fetched_blocks = 0;
  bool ok = false;
  std::ostringstream ss;
  try
  {
    GetWallet2()->refresh(fetched_blocks);
    ok = true;
  }
  catch (const tools::error::daemon_busy&)
  {
    ss << "daemon is busy. Please try later";
  }
  catch (const tools::error::no_connection_to_daemon&)
  {
    ss << "no connection to daemon. Please, make sure daemon is running";
  }
  catch (const tools::error::wallet_rpc_error& e)
  {
    LOG_ERROR("Unknown RPC error: " << e.to_string());
    ss << "RPC error \"" << e.what() << '"';
  }
  catch (const tools::error::refresh_error& e)
  {
    LOG_ERROR("refresh error: " << e.to_string());
    ss << e.what();
  }
  catch (const tools::error::wallet_internal_error& e)
  {
    LOG_ERROR("internal error: " << e.to_string());
    ss << "internal error: " << e.what();
  }
  catch (const std::exception& e)
  {
    LOG_ERROR("unexpected error: " << e.what());
    ss << "unexpected error: " << e.what();
  }
  catch (...)
  {
    LOG_ERROR("Unknown error");
    ss << "unknown error";
  }
  
  if (!ok)
  {
    LOG_PRINT_RED("Refresh failed: " << ss.str() << ". Blocks received: " << fetched_blocks, LOG_LEVEL_0);
  }
  
  ok = false;
  
  try
  {
    if (fetched_blocks > 0)
      GetWallet2()->store();
    ok = true;
  }
  catch (const std::exception& e)
  {
    LOG_ERROR("unexpected error: " << e.what());
    ss << "unexpected error: " << e.what();
  }
  
  if (!ok)
  {
    LOG_PRINT_RED("Store failed: " << ss.str(), LOG_LEVEL_0);
  }
  
  wallet_task task;
  task.type = wallet_task::TASK_CHECK_EXIT;
  tasks.push(task);
  
  return ok;
}

void CWallet::Shutdown()
{
  m_stop = true;
  
  if (HasWallet2())
    GetWallet2()->stop(); // if refreshing, interrupt refreshing, leads to wallet being stored
  
  {
    LOCK(cs_refresh); // if refreshing, wait until done, otherwise post it right away
    wallet_task task;
    task.type = wallet_task::TASK_CHECK_EXIT;
    tasks.push(task);
  }
  
  LOG_PRINT_L0("Joining wallet task thread...");
  p_wallet_task_thread->join();
  
  StoreCWallet();
}


void CWallet::on_new_block_processed(uint64_t height, const cryptonote::block& block)
{
  /*LOG_PRINT_GREEN("on_new_block_processed in CWallet", LOG_LEVEL_2);
  
  wallet_task task;
  task.type = wallet_task::TASK_NEW_BLOCK;
  task.height = height;
  task.block = block;
  tasks.push(task);*/
}

void CWallet::on_money_received(uint64_t height, const tools::wallet2::transfer_details& td)
{
  LOG_PRINT_GREEN("on_money_received in CWallet", LOG_LEVEL_2);
  
  wallet_task task;
  task.type = wallet_task::TASK_MONEY_RECEIVED;
  task.height = height;
  task.td = td;
  tasks.push(task);
}

void CWallet::on_money_spent(uint64_t height, const tools::wallet2::transfer_details& td)
{
  LOG_PRINT_GREEN("on_money_spent in CWallet", LOG_LEVEL_2);
  
  wallet_task task;
  task.type = wallet_task::TASK_MONEY_SPENT;
  task.height = height;
  task.td = td;
  tasks.push(task);
}

void CWallet::on_skip_transaction(uint64_t height, const cryptonote::transaction& tx)
{
  /*LOG_PRINT_GREEN("on_skip_transaction in CWallet", LOG_LEVEL_2);
   
   wallet_task task;
   task.type = wallet_task::TASK_NEW_BLOCK;
   task.height = height;
   task.tx = tx;
   tasks.push(task);*/
}

void CWallet::on_new_transfer(const cryptonote::transaction& tx, const tools::wallet2::known_transfer_details& kd)
{
  LOG_PRINT_GREEN("on_new_transfer in CWallet", LOG_LEVEL_2);
  
  wallet_task task;
  task.type = wallet_task::TASK_NEW_TRANSFER;
  task.kd = kd;
  tasks.push(task);
}


void CWallet::on_tx_added(const crypto::hash& tx_hash, const cryptonote::transaction& tx)
{
  LOG_PRINT_GREEN("on_tx_added in CWallet", LOG_LEVEL_2);

  wallet_task task;
  task.type = wallet_task::TASK_TX_ADDED;
  task.tx = tx;
  tasks.push(task);
}

void CWallet::on_tx_removed(const crypto::hash& tx_hash, const cryptonote::transaction& tx)
{
  LOG_PRINT_GREEN("on_tx_removed in CWallet", LOG_LEVEL_2);
  
  wallet_task task;
  task.type = wallet_task::TASK_TX_REMOVED;
  task.tx = tx;
  tasks.push(task);
}


bool CWallet::StoreCWallet() const
{
  LOG_PRINT_L4("LOCK(cs_wallet) StoreCWallet");
  LOCK(cs_wallet); // mapWallet, mapAddressBook
  LOG_PRINT_L4("LOCK(cs_wallet) StoreCWallet acquired");
  
  std::string buf;
  bool r = ::serialization::dump_binary(*this, buf);
  CHECK_AND_ASSERT_MES(r, false, "failed to serialize CWallet");
  r = r && epee::file_io_utils::save_string_to_file(cwalletFileName, buf);
  CHECK_AND_ASSERT_MES(r, false, "failed to save CWallet file " << cwalletFileName);
  return true;
}

bool CWallet::LoadCWallet()
{
  LOG_PRINT_L4("LOCK(cs_wallet) LoadCWallet");
  LOCK(cs_wallet); // mapWallet, mapAddressBook
  LOG_PRINT_L4("LOCK(cs_wallet) LoadCWallet acquired");
  
  std::string buf;
  bool r = epee::file_io_utils::load_file_to_string(cwalletFileName, buf);
  CHECK_AND_ASSERT_MES(r, false, "failed to load CWallet file " << cwalletFileName);
  r = ::serialization::parse_binary(buf, *this);
  CHECK_AND_ASSERT_MES(r, false, "failed to deserialize CWallet file " << cwalletFileName);
  
  SyncAllTransferDetails();
  
  return true;
}

void CWallet::WalletTaskThread()
{
  LOG_PRINT_L0("Wallet task thread started");
  
  while (1)
  {
    if (m_stop && tasks.empty())
      break;
    
    wallet_task task;
    tasks.wait_and_pop(task);
    
    switch (task.type)
    {
      case wallet_task::TASK_NEW_BLOCK: {
        LOG_PRINT_GREEN("TASK_NEW_BLOCK", LOG_LEVEL_2);
      } break;
        
      case wallet_task::TASK_MONEY_RECEIVED: {
        LOG_PRINT_GREEN("TASK_MONEY_RECEIVED", LOG_LEVEL_2);
        
        bool fAdded = AddTransaction(task.td.m_tx);
        ProcTransferDetails(task.td, true, false);
        
        ProcTxUpdated(GetStrHash(task.td.m_tx), fAdded);
      } break;
        
      case wallet_task::TASK_MONEY_SPENT: {
        LOG_PRINT_GREEN("TASK_MONEY_SPENT", LOG_LEVEL_2);
        
        bool fAdded1 = AddTransaction(task.td.m_tx);
        bool fAdded2 = AddTransaction(task.td.m_spent_by_tx);
        
        ProcTransferDetails(task.td, false, true);
        
        ProcTxUpdated(GetStrHash(task.td.m_tx), fAdded1);
        ProcTxUpdated(GetStrHash(task.td.m_spent_by_tx), fAdded2);
      } break;
        
      case wallet_task::TASK_SKIP_TRANSACTION: {
        LOG_PRINT_GREEN("TASK_SKIP_TRANSACTION", LOG_LEVEL_2);
      } break;
        
      case wallet_task::TASK_NEW_TRANSFER: {
        LOG_PRINT_GREEN("TASK_NEW_TRANSFER", LOG_LEVEL_2);
        ProcKnownTransfer(task.kd.m_tx_hash, task.kd);
      } break;
        
      case wallet_task::TASK_TX_ADDED: {
        LOG_PRINT_GREEN("TASK_TX_ADDED", LOG_LEVEL_2);
        bool fAdded = AddTransaction(task.tx);
        ProcTxUpdated(GetStrHash(task.tx), fAdded);
      } break;
        
      case wallet_task::TASK_TX_REMOVED: {
        LOG_PRINT_GREEN("TASK_TX_REMOVED", LOG_LEVEL_2);
        bool fRemoved = RemoveTransaction(task.tx);
        if (fRemoved)
          NotifyTransactionChanged(this, GetStrHash(task.tx), CT_DELETED);
      } break;
        
      case wallet_task::TASK_CHECK_EXIT: {
        LOG_PRINT_GREEN("TASK_CHECK_EXIT", LOG_LEVEL_2);
      } break;
        
      default: {
        LOG_PRINT_RED("Unknown task: " << task.type, LOG_LEVEL_2);
      } break;
    }
  }
  
  LOG_PRINT_L0("Wallet task thread stopped");
}

