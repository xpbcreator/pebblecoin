#include "main.h"

CCriticalSection cs_main;

bool fImporting = false;
bool fReindex = false;
bool fBenchmark = false;

int64_t CTransaction_nMinTxFee = 1337;

core_t *pcore = NULL;
node_server_t *pnodeSrv = NULL;

CWallet *pwalletMain = NULL;
