// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/foreach.hpp>

#include "cryptonote_core/cryptonote_format_utils.h"

#include "split_strategies.h"

namespace tools {
namespace detail {
//----------------------------------------------------------------------------------------------------
void digit_split_strategy::split(const std::vector<cryptonote::tx_destination_entry>& dsts,
                                 const cryptonote::tx_destination_entry& change_dst, uint64_t dust_threshold,
                                 std::vector<cryptonote::tx_destination_entry>& splitted_dsts, uint64_t& dust) const
{
  splitted_dsts.clear();
  dust = 0;

  BOOST_FOREACH(auto& de, dsts)
  {
    cryptonote::decompose_amount_into_digits(de.amount, dust_threshold,
      [&](uint64_t chunk) { splitted_dsts.push_back(cryptonote::tx_destination_entry(de.cp, chunk, de.addr)); },
      [&](uint64_t a_dust) { splitted_dsts.push_back(cryptonote::tx_destination_entry(de.cp, a_dust, de.addr)); } );
  }

  cryptonote::decompose_amount_into_digits(change_dst.amount, dust_threshold,
    [&](uint64_t chunk) { splitted_dsts.push_back(cryptonote::tx_destination_entry(change_dst.cp, chunk, change_dst.addr)); },
    [&](uint64_t a_dust) { dust = a_dust; } );
}
//----------------------------------------------------------------------------------------------------
void null_split_strategy::split(const std::vector<cryptonote::tx_destination_entry>& dsts,
                                const cryptonote::tx_destination_entry& change_dst, uint64_t dust_threshold,
                                std::vector<cryptonote::tx_destination_entry>& splitted_dsts, uint64_t& dust) const
{
  splitted_dsts = dsts;

  dust = 0;
  uint64_t change = change_dst.amount;
  if (0 < dust_threshold)
  {
    for (uint64_t order = 10; order <= 10 * dust_threshold; order *= 10)
    {
      uint64_t dust_candidate = change_dst.amount % order;
      uint64_t change_candidate = (change_dst.amount / order) * order;
      if (dust_candidate <= dust_threshold)
      {
        dust = dust_candidate;
        change = change_candidate;
      }
      else
      {
        break;
      }
    }
  }

  if (0 != change)
  {
    splitted_dsts.push_back(cryptonote::tx_destination_entry(change_dst.cp, change, change_dst.addr));
  }
}
//----------------------------------------------------------------------------------------------------
} }
