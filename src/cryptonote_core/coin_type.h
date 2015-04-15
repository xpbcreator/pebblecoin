// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stdint.h>
#include <stddef.h>

#include "serialization/serialization.h"

#include "tx_versions.h"

namespace cryptonote
{
  // indicate what the contract type of a coin is
  enum CoinContractType {
    NotContract = 0,      // regular non-contract coin
    BackingCoin = 1,
    ContractCoin = 2,
    ContractTypeNA = 255,  // N/A for given input/output type (e.g. txin_create_contract)
  };
  
  struct coin_type {
    coin_type() { }
    explicit coin_type(uint64_t currency_in, CoinContractType contract_type_in=NotContract,
                       uint64_t backed_by_currency_in=BACKED_BY_N_A)
    : currency(currency_in), contract_type(contract_type_in), backed_by_currency(backed_by_currency_in) { }
    
    uint64_t currency;
    CoinContractType contract_type;
    uint64_t backed_by_currency;
    
    size_t minimum_tx_version() const
    {
      if (contract_type != NotContract || backed_by_currency != BACKED_BY_N_A)
        return CONTRACT_TRANSACTION_VERSION;
      if (currency != CURRENCY_XPB)
        return CURRENCY_TRANSACTION_VERSION;
      return VANILLA_TRANSACTION_VERSION;
    }
    
    bool is_valid_tx_version(size_t version) const
    {
      return version >= minimum_tx_version();
    }
    
    BEGIN_SERIALIZE_OBJECT()
      VARINT_FIELD(currency);
      VARINT_FIELD(contract_type);
      VARINT_FIELD(backed_by_currency);
    END_SERIALIZE()
  };
  
  inline bool operator ==(const cryptonote::coin_type& a, const cryptonote::coin_type& b)
  {
    return a.currency == b.currency && a.contract_type == b.contract_type && a.backed_by_currency == b.backed_by_currency;
  }
  inline bool operator !=(const cryptonote::coin_type& a, const cryptonote::coin_type& b)
  {
    return !(a == b);
  }
  inline bool operator <(const cryptonote::coin_type& a, const cryptonote::coin_type& b)
  {
    if (a.currency == b.currency)
    {
      if (a.contract_type == b.contract_type)
        return a.backed_by_currency < b.backed_by_currency;
      return a.contract_type < b.contract_type;
    }
    return a.currency < b.currency;
  }
  inline std::ostream &operator <<(std::ostream &o, const cryptonote::coin_type& v)
  {
    return o << "<" << v.currency << "/" << v.contract_type << "/" << v.backed_by_currency << ">";
  }
  
  //---------------------------------------------------------------
  
  typedef std::unordered_map<coin_type, uint64_t> currency_map;
  
  //---------------------------------------------------------------
  
  const static coin_type CP_XPB = coin_type(CURRENCY_XPB, NotContract, BACKED_BY_N_A);
  const static coin_type CP_N_A = coin_type(CURRENCY_N_A, ContractTypeNA, BACKED_BY_N_A);
}

namespace std {
  template<>
  struct hash<cryptonote::coin_type> {
    std::size_t operator()(const cryptonote::coin_type &v) const {
      return boost::hash<std::pair<uint64_t, std::pair<cryptonote::CoinContractType, uint64_t> > >()(
          std::make_pair(v.currency, std::make_pair(v.contract_type, v.backed_by_currency)));
    }
  };
}

namespace cryptonote {
  inline std::size_t hash_value(const cryptonote::coin_type &v) {
    return std::hash<cryptonote::coin_type>()(v);
  }
}
