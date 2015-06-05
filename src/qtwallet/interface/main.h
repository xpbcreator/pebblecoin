#ifndef PEBBLECOIN_BCMAIN_H
#define PEBBLECOIN_BCMAIN_H

#include <string>
#include <map>

#include "common/types.h"

#include "bitcoin/sync.h"

class CWallet;

namespace cryptonote {
  struct bs_delegate_info;
  struct account_public_address;
}

extern CCriticalSection cs_main;

extern bool fImporting;
extern bool fReindex;
extern bool fBenchmark;

extern int64_t CTransaction_nMinTxFee;

extern core_t *pcore;
extern node_server_t *pnodeSrv;
extern CWallet *pwalletMain;


int64_t WalletProcessedHeight();
int64_t DaemonProcessedHeight();
int64_t NumBlocksOfPeers();

bool GetDposRegisterInfo(cryptonote::delegate_id_t& unused_delegate_id, uint64_t& registration_fee);
bool GetDelegateInfo(const cryptonote::account_public_address& addr, cryptonote::bs_delegate_info& info);
std::vector<cryptonote::bs_delegate_info> GetDelegateInfos();

#endif
