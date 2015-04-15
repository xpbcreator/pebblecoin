// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "net_util.h"

#include "crypto/hash.h"

namespace tools
{
  crypto::hash get_proof_of_trust_hash(const nodetool::proof_of_trust& pot)
  {
    std::string s;
    s.append(reinterpret_cast<const char*>(&pot.peer_id), sizeof(pot.peer_id));
    s.append(reinterpret_cast<const char*>(&pot.time), sizeof(pot.time));
    return crypto::cn_fast_hash(s.data(), s.size());
  }
}
