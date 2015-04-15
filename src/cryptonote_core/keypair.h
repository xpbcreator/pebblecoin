// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "crypto/crypto.h"

namespace cryptonote
{
  struct keypair
  {
    crypto::public_key pub;
    crypto::secret_key sec;
    
    static inline keypair generate()
    {
      keypair k;
      generate_keys(k.pub, k.sec);
      return k;
    }
  };
  
  inline bool operator==(const keypair& a, const keypair& b) {
    return (a.pub == b.pub) && (a.sec == b.sec);
  }
  inline bool operator!=(const keypair& a, const keypair& b) {
    return !(a == b);
  }
}
