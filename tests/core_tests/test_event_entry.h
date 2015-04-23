// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/variant.hpp>

#include "common/boost_serialization_helper.h"
#include "common/types.h"
#include "cryptonote_config.h"
#include "cryptonote_core/account_boost_serialization.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/cryptonote_boost_serialization.h"

typedef boost::function<bool (core_t& c, size_t ev_index)> verify_callback_func;

struct callback_entry
{
  std::string callback_name;
  BEGIN_SERIALIZE_OBJECT()
    FIELD(callback_name)
  END_SERIALIZE()

private:
  friend class boost::serialization::access;

  template<class Archive>
  void serialize(Archive & ar, const unsigned int /*version*/)
  {
    ar & callback_name;
  }
};

struct callback_entry_func
{
  verify_callback_func cb;
  BEGIN_SERIALIZE_OBJECT()
    throw std::runtime_error("Can't epee serialize callback_entry_func");
  END_SERIALIZE()

private:
  friend class boost::serialization::access;

  template<class Archive>
  void serialize(Archive & ar, const unsigned int /*version*/)
  {
    throw std::runtime_error("Can't boost serialize callback_entry_func");
  }
};

struct dont_mark_spent_tx
{
  BEGIN_SERIALIZE_OBJECT()
  END_SERIALIZE()

private:
  friend class boost::serialization::access;
  
  template<class Archive>
  void serialize(Archive & ar, const unsigned int /*version*/)
  {
  }
};

struct set_dpos_switch_block_struct
{
  int64_t block;
  int64_t registration_block;
  BEGIN_SERIALIZE_OBJECT()
    FIELD(block);
    FIELD(registration_block);
  END_SERIALIZE()
  
  set_dpos_switch_block_struct() { }
  set_dpos_switch_block_struct(int64_t block_in, int64_t registration_block_in)
      : block(block_in), registration_block(registration_block_in) { }
  
private:
  friend class boost::serialization::access;
  
  template<class Archive>
  void serialize(Archive & ar, const unsigned int /*version*/)
  {
    ar & block;
  }
};

struct debug_mark
{
  std::string message;
  BEGIN_SERIALIZE_OBJECT()
    FIELD(message)
  END_SERIALIZE()

private:
  friend class boost::serialization::access;

  template<class Archive>
  void serialize(Archive & ar, const unsigned int /*version*/)
  {
    ar & message;
  }
};

struct register_delegate_account
{
  cryptonote::delegate_id_t delegate_id;
  cryptonote::account_base acct;
  BEGIN_SERIALIZE_OBJECT()
    FIELD(delegate_id)
    FIELD(acct)
  END_SERIALIZE()
  
private:
  friend class boost::serialization::access;
  
  template<class Archive>
  void serialize(Archive & ar, const unsigned int /*version*/)
  {
    ar & delegate_id;
    ar & acct;
  }
};

template<typename T>
struct serialized_object
{
  serialized_object() { }

  serialized_object(const cryptonote::blobdata& a_data)
    : data(a_data)
  {
  }

  cryptonote::blobdata data;
  BEGIN_SERIALIZE_OBJECT()
    FIELD(data)
    END_SERIALIZE()

private:
  friend class boost::serialization::access;

  template<class Archive>
  void serialize(Archive & ar, const unsigned int /*version*/)
  {
    ar & data;
  }
};

typedef serialized_object<cryptonote::block> serialized_block;
typedef serialized_object<cryptonote::transaction> serialized_transaction;

struct event_visitor_settings
{
  int valid_mask;
  bool txs_keeped_by_block;
  bool check_can_create_block_from_template;

  enum settings
  {
    set_txs_keeped_by_block = 1 << 0,
    set_check_can_create_block_from_template = 1 << 1
  };

  event_visitor_settings(int a_valid_mask = 0, bool a_txs_keeped_by_block = false,
                         bool a_check_can_create_block_from_template = true)
    : valid_mask(a_valid_mask)
    , txs_keeped_by_block(a_txs_keeped_by_block)
    , check_can_create_block_from_template(a_check_can_create_block_from_template)
  {
  }

private:
  friend class boost::serialization::access;

  template<class Archive>
  void serialize(Archive & ar, const unsigned int /*version*/)
  {
    ar & valid_mask;
    ar & txs_keeped_by_block;
  }
};

VARIANT_TAG(binary_archive, callback_entry, 0xcb);
VARIANT_TAG(binary_archive, cryptonote::account_base, 0xcc);
VARIANT_TAG(binary_archive, serialized_block, 0xcd);
VARIANT_TAG(binary_archive, serialized_transaction, 0xce);
VARIANT_TAG(binary_archive, event_visitor_settings, 0xcf);
VARIANT_TAG(binary_archive, dont_mark_spent_tx, 0xd0);
VARIANT_TAG(binary_archive, set_dpos_switch_block_struct, 0xd1);
VARIANT_TAG(binary_archive, debug_mark, 0xd2);
VARIANT_TAG(binary_archive, register_delegate_account, 0xd3);
VARIANT_TAG(binary_archive, callback_entry_func, 0xd4);

typedef boost::variant<cryptonote::block, cryptonote::transaction, cryptonote::account_base, callback_entry, serialized_block, serialized_transaction, event_visitor_settings, dont_mark_spent_tx, set_dpos_switch_block_struct, debug_mark, register_delegate_account, callback_entry_func> test_event_entry;
