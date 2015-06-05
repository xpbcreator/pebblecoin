#ifndef PEBBLECOIN_BCWALLET_H
#define PEBBLECOIN_BCWALLET_H

#include <memory>

#include <boost/signals2/signal.hpp>

#include "common/types.h"
#include "common/ui_interface.h"
#include "common/concurrent_queue.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/i_tx_pool_callback.h"
#include "wallet/wallet2.h"
#include "serialization/serialization.h"

#include "bitcoin/sync.h"

namespace boost {
  class thread;
}

namespace cryptonote {
  class tx_memory_pool;
}

class CWalletTx {
private:
  cryptonote::transaction tx;
  
public:
  std::string txHash;
  uint64_t nBlockHeight;
  std::string blockHash;
  uint64_t nTimestamp;

  std::vector<tools::wallet2::transfer_details> vecTransfersIn;  // details for transfers in (m_tx)
  std::vector<tools::wallet2::transfer_details> vecTransfersOut; // details for transfers out (m_spent_by_tx)
  boost::optional<tools::wallet2::known_transfer_details> optKnownTransfers; // specific sent-out info for this tx
  
  CWalletTx() { }
  CWalletTx(const cryptonote::transaction& txIn);
  
  void CheckUpdateHeight();
  
  bool IsCoinBase() const;
  bool IsInMainChain() const;
  int GetDepthInMainChain() const;
  int GetBlocksToMaturity() const;
  bool IsTrusted() const;
  bool IsPayment(const CWallet& wallet) const;
  
  bool GetPaymentInfo(const CWallet& wallet, std::string& paymentId, tools::wallet2::payment_details& paymentDetails) const;
  bool GetCreditDebit(uint64_t& nMined, uint64_t& nCredit, uint64_t& nDebit) const;
  
  BEGIN_SERIALIZE_OBJECT()
    FIELD(tx)
    FIELD(txHash)
    FIELD(nBlockHeight)
    FIELD(blockHash)
    FIELD(nTimestamp)
  END_SERIALIZE()
};

struct CAddressBookData
{
  std::string name;
  std::string purpose;
  
  BEGIN_SERIALIZE_OBJECT()
    FIELD(name)
    FIELD(purpose)
  END_SERIALIZE()
};

class CWallet : public tools::i_wallet2_callback, public cryptonote::i_tx_pool_callback {
  tools::wallet2 *pwallet2;
  std::string cwalletFileName;
  
  void ProcTransferDetails(const tools::wallet2::transfer_details& td, bool fProcIn, bool fProcOut);
  void ProcKnownTransfer(const crypto::hash& tx_hash, const tools::wallet2::known_transfer_details& known_transfers);
  void SyncAllTransferDetails();
  
  bool AddTransaction(const cryptonote::transaction& tx);
  bool RemoveTransaction(const cryptonote::transaction& tx);
  
  mutable CCriticalSection cs_refresh;
  
  std::atomic<bool> m_stop;

public:
  mutable CCriticalSection cs_wallet;
  std::map<address_t, CAddressBookData> mapAddressBook;
  std::map<crypto::hash, CWalletTx> mapWallet;
  
  explicit CWallet(const std::string& cwalletFileNameArg);
  ~CWallet();
  
  bool HasWallet2() const;
  void SetWallet2(tools::wallet2 *pwalletArg);
  tools::wallet2 *GetWallet2() const;
  
  void ProcTxUpdated(std::string txHash, bool fAdded);
  
  void SetTxMemoryPool(cryptonote::tx_memory_pool *pmempool);
  
  bool SetAddressBook(const address_t& address, const std::string& strName, const std::string& purpose);
  bool DelAddressBook(const address_t& address);
  
  bool RefreshOnce();
  
  void Shutdown();
  
  /** Address book entry changed.
   * @note called with lock cs_wallet held.
   */
  boost::signals2::signal<void (CWallet *wallet, const address_t &address,
                                const std::string &label, bool isMine,
                                const std::string &purpose,
                                ChangeType status)> NotifyAddressBookChanged;

  /** Wallet transaction added, removed or updated.
   * @note called with lock cs_wallet held.
   */
  boost::signals2::signal<void (CWallet *wallet, const std::string &hashTx,
                                ChangeType status)> NotifyTransactionChanged;
  
  /** i_wallet2_callback functions */
  void on_new_block_processed(uint64_t height, const cryptonote::block& block);
  void on_money_received(uint64_t height, const tools::wallet2::transfer_details& td);
  void on_money_spent(uint64_t height, const tools::wallet2::transfer_details& td);
  void on_skip_transaction(uint64_t height, const cryptonote::transaction& tx);
  void on_new_transfer(const cryptonote::transaction& tx, const tools::wallet2::known_transfer_details& kd);

  /** i_tx_pool_callback functions */
  void on_tx_added(const crypto::hash& tx_hash, const cryptonote::transaction& tx);
  void on_tx_removed(const crypto::hash& tx_hash, const cryptonote::transaction& tx);
  
  /** Serialization */
  bool StoreCWallet() const;
  bool LoadCWallet();
  
  BEGIN_SERIALIZE_OBJECT()
    FIELD(mapAddressBook)
    FIELD(mapWallet)
  END_SERIALIZE()
  
private:
  // do all callbacks in a new thread to prevent deadlocking
  struct wallet_task
  {
    enum TaskType {
      TASK_NEW_BLOCK,
      TASK_MONEY_RECEIVED,
      TASK_MONEY_SPENT,
      TASK_SKIP_TRANSACTION,
      TASK_NEW_TRANSFER,
      TASK_TX_ADDED,
      TASK_TX_REMOVED,
      TASK_CHECK_EXIT
    };
    
    TaskType type;
    uint64_t height;
    cryptonote::block block;
    tools::wallet2::transfer_details td;
    cryptonote::transaction tx;
    cryptonote::transaction tx2;
    tools::wallet2::known_transfer_details kd;
  };
  
  concurrent_queue<wallet_task> tasks;
  std::unique_ptr<boost::thread> p_wallet_task_thread;
  
  void WalletTaskThread();
};

extern int64_t nTransactionFee;

#endif
