#ifndef PEBBLECOIN_BCSCRIPT_H
#define PEBBLECOIN_BCSCRIPT_H

#include "common/types.h"

class CScript {
  
};

class CWallet;

bool IsMine(CWallet& wallet, const address_t& address);

#endif