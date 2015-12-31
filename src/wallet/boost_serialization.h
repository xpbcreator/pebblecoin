// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <boost/serialization/list.hpp>
#include <boost/serialization/vector.hpp>

#include "cryptonote_core/account_boost_serialization.h"
#include "common/unordered_containers_boost_serialization.h"
#include "rpc/core_rpc_server_commands_defs.h"

#include "wallet2.h"

namespace tools
{
  template <class t_archive>
  inline void wallet2::serialize(t_archive &a, const unsigned int ver)
  {
    if(ver < 5)
      return;
    a & m_blockchain;
    a & m_transfers;
    a & m_account_public_address;
    a & m_key_images;
    if(ver < 6)
      return;
    a & m_unconfirmed_txs;
    if(ver < 7)
      return;
    a & m_payments;
    if(ver < 8)
      return;
    a & m_autovote_delegates;
    if(ver < 9)
      return;
    a & m_user_delegates;
    if(ver < 10)
      return;
    a & m_voting_user_delegates;
  }
}

BOOST_CLASS_VERSION(tools::wallet2, 10)
BOOST_CLASS_VERSION(tools::wallet2::known_transfer_details, 3)
BOOST_CLASS_VERSION(tools::wallet2::payment_details, 2)

namespace boost
{
  namespace serialization
  {
    template <class Archive>
    inline void serialize(Archive &a, tools::wallet2::transfer_details &x, const boost::serialization::version_type ver)
    {
      a & x.m_block_height;
      a & x.m_from_miner_tx;
      a & x.m_global_output_index;
      a & x.m_internal_output_index;
      a & x.m_tx;
      a & x.m_spent;
      a & x.m_spent_by_tx;
      a & x.m_key_image;
    }

    template <class Archive>
    inline void serialize(Archive &a, tools::wallet2::unconfirmed_transfer_details &x, const boost::serialization::version_type ver)
    {
      a & x.m_change;
      a & x.m_sent_time;
      a & x.m_tx;
    }

    template <class Archive>
    inline void serialize(Archive& a, tools::wallet2::payment_details& x, const boost::serialization::version_type ver)
    {
      a & x.m_tx_hash;
      a & x.m_amount;
      a & x.m_block_height;
      a & x.m_unlock_time;
      if (ver >= (boost::serialization::version_type)(2))
      {
        a & x.m_sent;
      }
      else
      {
        if (!(typename Archive::is_saving()))
        {
          x.m_sent = false;
        }
      }
    }

    template <class Archive>
    inline void serialize(Archive& a, tools::wallet2::known_transfer_details& x, const boost::serialization::version_type ver)
    {
      a & x.m_tx_hash;
      a & x.m_dests;
      a & x.m_fee;
      a & x.m_xpb_change;
      if (ver >= (boost::serialization::version_type)(1))
      {
        a & x.m_all_change;
        a & x.m_currency_minted;
        a & x.m_amount_minted;
      }
      if (ver >= (boost::serialization::version_type)(2))
      {
        a & x.m_delegate_id_registered;
        a & x.m_delegate_address_registered;
        a & x.m_registration_fee_paid;
      }
      if (ver >= (boost::serialization::version_type)(3))
      {
        a & x.m_votes;
      }
    }

    template <class Archive>
    inline void serialize(Archive &a, tools::out_entry &x, const boost::serialization::version_type ver)
    {
      // work around packed struct
      auto global_amount_index = x.global_amount_index;
      auto out_key = x.out_key;
      
      a & global_amount_index;
      a & out_key;
      
      x.global_amount_index = global_amount_index;
      x.out_key = out_key;
    }
    
    template <class Archive>
    inline void serialize(Archive &a, tools::wallet2_voting_batch &x, const boost::serialization::version_type ver)
    {
      a & x.m_transfer_indices;
      a & x.m_fake_outs;
      a & x.m_vote_history;
    }
    
    template <class Archive>
    inline void serialize(Archive& a, tools::wallet2_votes_info& x, const boost::serialization::version_type ver)
    {
      a & x.m_batches;
      a & x.m_transfer_batch_map;
    }
  }
}
