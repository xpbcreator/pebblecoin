// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "account.h"
#include "cryptonote_core/cryptonote_boost_serialization.h"
#include "cryptonote_core/cryptonote_format_utils.h"

namespace boost
{
  namespace serialization
  {
    template <class Archive>
    inline void serialize(Archive &a, cryptonote::account_keys &x, const boost::serialization::version_type ver)
    {
      a & x.m_account_address;
      a & x.m_spend_secret_key;
      a & x.m_view_secret_key;
    }

    template <class Archive>
    inline void serialize(Archive &a, cryptonote::account_public_address &x, const boost::serialization::version_type ver)
    {
      a & x.m_spend_public_key;
      a & x.m_view_public_key;
    }

    template <class Archive>
    inline void serialize(Archive &a, cryptonote::tx_destination_entry& x, const boost::serialization::version_type ver)
    {
      if (ver >= (boost::serialization::version_type)(2))
      {
        a & x.cp;
      }
      else {
        if (typename Archive::is_saving()) {
          throw std::runtime_error("Invalid cp for old archive");
        }
        x.cp = cryptonote::CP_XPB;
      }
      a & x.amount;
      a & x.addr;
    }
  }
}

BOOST_CLASS_VERSION(cryptonote::tx_destination_entry, 2)
