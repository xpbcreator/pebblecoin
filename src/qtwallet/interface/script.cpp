#include "script.h"

#include "main.h"
#include "wallet.h"


bool IsMine(CWallet &wallet, const address_t& address)
{
  return wallet.GetWallet2()->is_mine(address);
}
