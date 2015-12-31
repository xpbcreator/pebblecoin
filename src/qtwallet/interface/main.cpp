#include <set>

#include "common/types.h"
#include "crypto/hash.h"
#include "cryptonote_config.h"
#include "cryptonote_core/blockchain_storage.h"
#include "cryptonote_core/cryptonote_core.h"
#include "cryptonote_protocol/cryptonote_protocol_handler.h"
#include "p2p/p2p_protocol_defs.h"
#include "p2p/net_node.h"

#include "main.h"
#include "wallet.h"

CCriticalSection cs_main;

bool fImporting = false;
bool fReindex = false;
bool fBenchmark = false;

int64_t CTransaction_nMinTxFee = DEFAULT_FEE;

core_t *pcore = NULL;
node_server_t *pnodeSrv = NULL;

CWallet *pwalletMain = NULL;


int64_t WalletProcessedHeight()
{
  LOCK(cs_main);
  if (!pcore)
    return 0;
  if (!pwalletMain)
    return 0;
  return (int64_t)(pwalletMain->GetWallet2()->get_blockchain_current_height() - 1);
}

int64_t DaemonProcessedHeight()
{
  LOCK(cs_main);
  if (!pcore)
    return 0;
  
  return (int64_t)pcore->get_blockchain_storage().get_top_block_height();
}

int64_t NumBlocksOfPeers()
{
  LOCK(cs_main);
  if (!pnodeSrv)
    return 0;
  
  std::set<uint64_t> block_heights;
  
  pnodeSrv->for_each_connection([&](cryptonote::cryptonote_connection_context& context, nodetool::peerid_type peer_id) {
    block_heights.insert(context.m_remote_blockchain_height);
    return true;
  });
  
  auto it = block_heights.begin();
  std::advance(it, block_heights.size() / 2);
  return (int64_t)(*it);
}

bool GetDposRegisterInfo(cryptonote::delegate_id_t& unused_delegate_id, uint64_t& registration_fee)
{
  LOCK(cs_main);
  if (!pcore)
    return false;
  
  return pcore->get_dpos_register_info(unused_delegate_id, registration_fee);
}

bool GetDelegateInfo(const cryptonote::account_public_address& addr, cryptonote::bs_delegate_info& info)
{
  LOCK(cs_main);
  if (!pcore)
    return false;
  
  return pcore->get_delegate_info(addr, info);
}

std::vector<cryptonote::bs_delegate_info> GetDelegateInfos()
{
  LOCK(cs_main);
  if (!pcore)
    return std::vector<cryptonote::bs_delegate_info>();
  
  return pcore->get_delegate_infos();
}
