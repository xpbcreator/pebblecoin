// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstddef>
#include <vector>

#include <boost/noncopyable.hpp>

#include "common/types.h"

namespace cryptonote
{
  struct tx_destination_entry;
}

namespace tools
{
  class wallet2;
  struct tx_dust_policy;
  namespace detail
  {
    struct split_strategy;
  }
  
  class wallet_tx_builder : private boost::noncopyable
  {
  public:
    wallet_tx_builder(wallet2& wallet);
    ~wallet_tx_builder();

    void init_tx(uint64_t unlock_time=0, const std::vector<uint8_t>& extra=std::vector<uint8_t>());
    void add_send(const std::vector<cryptonote::tx_destination_entry>& dsts, uint64_t fee,
                  size_t min_fake_outs, size_t fake_outputs_count, const detail::split_strategy& destination_split_strategy,
                  const tx_dust_policy& dust_policy);
    void add_register_delegate(cryptonote::delegate_id_t delegate_id,
                               const cryptonote::account_public_address& address,
                               uint64_t registration_fee);
    uint64_t add_votes(size_t min_fake_outs, size_t fake_outputs_count, const tx_dust_policy& dust_policy,
                       uint64_t num_votes, const cryptonote::delegate_votes& desired_votes,
                       uint64_t delegates_per_vote);
    void finalize(cryptonote::transaction& tx);
    void process_transaction_sent();
    
  private:
    class impl;
    impl *m_pimpl;
  };
}
