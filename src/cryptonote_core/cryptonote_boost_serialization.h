// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/variant.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/map.hpp>
#include <boost/foreach.hpp>
#include <boost/serialization/is_bitwise_serializable.hpp>

#include "common/unordered_containers_boost_serialization.h"
#include "crypto/crypto.h"

#include "cryptonote_basic.h"
#include "keypair.h"

//namespace cryptonote {
namespace boost
{
  namespace serialization
  {

  //---------------------------------------------------
  template <class Archive>
  inline void serialize(Archive &a, crypto::public_key &x, const boost::serialization::version_type ver)
  {
    a & reinterpret_cast<char (&)[sizeof(crypto::public_key)]>(x);
  }
  template <class Archive>
  inline void serialize(Archive &a, crypto::secret_key &x, const boost::serialization::version_type ver)
  {
    a & reinterpret_cast<char (&)[sizeof(crypto::secret_key)]>(x);
  }
  template <class Archive>
  inline void serialize(Archive &a, cryptonote::keypair &x, const boost::serialization::version_type ver)
  {
    a & x.pub;
    a & x.sec;
  }
  template <class Archive>
  inline void serialize(Archive &a, crypto::key_derivation &x, const boost::serialization::version_type ver)
  {
    a & reinterpret_cast<char (&)[sizeof(crypto::key_derivation)]>(x);
  }
  template <class Archive>
  inline void serialize(Archive &a, crypto::key_image &x, const boost::serialization::version_type ver)
  {
    a & reinterpret_cast<char (&)[sizeof(crypto::key_image)]>(x);
  }

  template <class Archive>
  inline void serialize(Archive &a, crypto::signature &x, const boost::serialization::version_type ver)
  {
    a & reinterpret_cast<char (&)[sizeof(crypto::signature)]>(x);
  }
  template <class Archive>
  inline void serialize(Archive &a, crypto::hash &x, const boost::serialization::version_type ver)
  {
    a & reinterpret_cast<char (&)[sizeof(crypto::hash)]>(x);
  }

  template <class Archive>
  inline void serialize(Archive &a, cryptonote::coin_type &x, const boost::serialization::version_type ver)
  {
    a & x.currency;
    a & x.contract_type;
    a & x.backed_by_currency;
  }

  template <class Archive>
  inline void serialize(Archive &a, cryptonote::txout_to_script &x, const boost::serialization::version_type ver)
  {
    a & x.keys;
    a & x.script;
  }

  template <class Archive>
  inline void serialize(Archive &a, cryptonote::txout_to_key &x, const boost::serialization::version_type ver)
  {
    a & x.key;
  }

  template <class Archive>
  inline void serialize(Archive &a, cryptonote::txout_to_scripthash &x, const boost::serialization::version_type ver)
  {
    a & x.hash;
  }

  template <class Archive>
  inline void serialize(Archive &a, cryptonote::txin_gen &x, const boost::serialization::version_type ver)
  {
    a & x.height;
  }

  template <class Archive>
  inline void serialize(Archive &a, cryptonote::txin_to_script &x, const boost::serialization::version_type ver)
  {
    a & x.prev;
    a & x.prevout;
    a & x.sigset;
  }

  template <class Archive>
  inline void serialize(Archive &a, cryptonote::txin_to_scripthash &x, const boost::serialization::version_type ver)
  {
    a & x.prev;
    a & x.prevout;
    a & x.script;
    a & x.sigset;
  }

  template <class Archive>
  inline void serialize(Archive &a, cryptonote::txin_to_key &x, const boost::serialization::version_type ver)
  {
    a & x.amount;
    a & x.key_offsets;
    a & x.k_image;
  }

  template <class Archive>
  inline void serialize(Archive &a, cryptonote::txin_mint &x, const boost::serialization::version_type ver)
  {
    a & x.currency;
    a & x.description;
    a & x.decimals;
    a & x.amount;
    a & x.remint_key;
  }
    
  template <class Archive>
  inline void serialize(Archive &a, cryptonote::txin_remint &x, const boost::serialization::version_type ver)
  {
    a & x.currency;
    a & x.amount;
    a & x.new_remint_key;
    a & x.sig;
  }

  template <class Archive>
  inline void serialize(Archive &a, cryptonote::txin_create_contract &x, const boost::serialization::version_type ver)
  {
    a & x.contract;
    a & x.description;
    a & x.grading_key;
    a & x.fee_scale;
    a & x.expiry_block;
    a & x.default_grade;
  }

  template <class Archive>
  inline void serialize(Archive &a, cryptonote::txin_mint_contract &x, const boost::serialization::version_type ver)
  {
    a & x.contract;
    a & x.backed_by_currency;
    a & x.amount;
  }
    
  template <class Archive>
  inline void serialize(Archive &a, cryptonote::txin_grade_contract &x, const boost::serialization::version_type ver)
  {
    a & x.contract;
    a & x.grade;
    a & x.fee_amounts;
    a & x.sig;
  }
    
  template <class Archive>
  inline void serialize(Archive &a, cryptonote::txin_resolve_bc_coins &x, const boost::serialization::version_type ver)
  {
    a & x.contract;
    a & x.is_backing_coins;
    a & x.backing_currency;
    a & x.source_amount;
    a & x.graded_amount;
  }

  template <class Archive>
  inline void serialize(Archive &a, cryptonote::txin_fuse_bc_coins &x, const boost::serialization::version_type ver)
  {
    a & x.contract;
    a & x.backing_currency;
    a & x.amount;
  }
    
  template <class Archive>
  inline void serialize(Archive &a, cryptonote::txin_register_delegate &x, const boost::serialization::version_type ver)
  {
    a & x.delegate_id;
    a & x.registration_fee;
    a & x.delegate_address;
  }
    
  template <class Archive>
  inline void serialize(Archive &a, cryptonote::txin_vote &x, const boost::serialization::version_type ver)
  {
    a & x.ink;
    a & x.seq;
    a & x.votes;
  }
  
  template <class Archive>
  inline void serialize(Archive &a, cryptonote::tx_out &x, const boost::serialization::version_type ver)
  {
    a & x.amount;
    a & x.target;
  }

  template <class Archive>
  inline void serialize(Archive &a, cryptonote::transaction &x, const boost::serialization::version_type ver)
  {
    if (!x.boost_serialize(a))
      throw std::runtime_error("Error boost_serializing a tx");
  }

  template <class Archive>
  inline void serialize(Archive &a, cryptonote::block &b, const boost::serialization::version_type ver)
  {
    if (!b.boost_serialize(a))
      throw std::runtime_error("Error boost_serializing a block");
  }
}
}

//}
