// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <algorithm>

#include "common/functional.h"

#include "cryptonote_core/tx_builder.h"
#include "cryptonote_core/account.h"
#include "cryptonote_core/keypair.h"

namespace cryptonote
{
  template <class tx_mapper_t>
  bool construct_tx(const account_keys& sender_account_keys,
                    const std::vector<tx_source_entry>& sources, const std::vector<tx_destination_entry>& destinations,
                    std::vector<uint8_t> extra, transaction& tx, uint64_t unlock_time,
                    keypair& txkey, tx_mapper_t&& mapper) {
    tx_builder txb;
    
    txb.init(unlock_time, extra);
    txb.add_send(sender_account_keys, sources, destinations);
    txb.finalize(mapper);
    return txb.get_finalized_tx(tx, txkey);
  }
}
