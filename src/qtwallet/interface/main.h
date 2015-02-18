#ifndef PEBBLECOIN_BCMAIN_H
#define PEBBLECOIN_BCMAIN_H

#include "../bitcoin/sync.h"
#include "common/types.h"

#include <string>

class CWallet;

extern CCriticalSection cs_main;

extern bool fImporting;
extern bool fReindex;
extern bool fBenchmark;

extern int64_t CTransaction_nMinTxFee;

extern core_t *pcore;
extern node_server_t *pnodeSrv;
extern CWallet *pwalletMain;


int WalletProcessedHeight();
int DaemonProcessedHeight();
int NumBlocksOfPeers();

#endif

