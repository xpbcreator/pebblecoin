// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "misc_language.h"

#include "nulls.h"

namespace cryptonote
{
  const crypto::hash null_hash = AUTO_VAL_INIT(null_hash);
  const crypto::public_key null_pkey = AUTO_VAL_INIT(null_pkey);
  const crypto::secret_key null_skey = AUTO_VAL_INIT(null_skey);
  const keypair null_keypair = AUTO_VAL_INIT(null_keypair);
  const account_public_address null_public_address = AUTO_VAL_INIT(null_public_address);
}
