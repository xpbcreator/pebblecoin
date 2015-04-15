// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>

#include "p2p/p2p_protocol_defs.h"
#include "crypto/crypto.h"

namespace tools
{
  crypto::hash get_proof_of_trust_hash(const nodetool::proof_of_trust& pot);
}
