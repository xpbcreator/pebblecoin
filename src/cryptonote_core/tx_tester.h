// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "cryptonote_basic.h"

namespace cryptonote {
  
  // non-const access to vin/vout to help testing
  class tx_tester
  {
  public:
    transaction& tx;
    
    std::vector<txin_v>& vin;
    std::vector<tx_out>& vout;
    std::vector<coin_type>& vin_coin_types;
    std::vector<coin_type>& vout_coin_types;
    
    tx_tester(transaction& tx_in) : tx(tx_in), vin(tx.vin), vout(tx.vout)
                                  , vin_coin_types(tx.vin_coin_types), vout_coin_types(tx.vout_coin_types)
    { }
  };
  
}
