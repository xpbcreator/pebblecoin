// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "serialization/keyvalue_serialization.h"

#include "crypto/crypto.h"

#include "packing.h"

namespace cryptonote
{
  PACK(struct account_public_address
  {
    crypto::public_key m_spend_public_key;
    crypto::public_key m_view_public_key;
    
    BEGIN_SERIALIZE_OBJECT()
      FIELD(m_spend_public_key)
      FIELD(m_view_public_key)
    END_SERIALIZE()
    
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE_VAL_POD_AS_BLOB_FORCE(m_spend_public_key)
      KV_SERIALIZE_VAL_POD_AS_BLOB_FORCE(m_view_public_key)
    END_KV_SERIALIZE_MAP()
  })

  struct account_keys
  {
    account_public_address m_account_address;
    crypto::secret_key   m_spend_secret_key;
    crypto::secret_key   m_view_secret_key;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(m_account_address)
      KV_SERIALIZE_VAL_POD_AS_BLOB_FORCE(m_spend_secret_key)
      KV_SERIALIZE_VAL_POD_AS_BLOB_FORCE(m_view_secret_key)
    END_KV_SERIALIZE_MAP()
  };

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  class account_base
  {
  public:
    account_base();
    void generate();
    const account_keys& get_keys() const;
    std::string get_public_address_str();

    uint64_t get_createtime() const { return m_creation_timestamp; }
    void set_createtime(uint64_t val) { m_creation_timestamp = val; }

    bool load(const std::string& file_path);
    bool store(const std::string& file_path);

    template <class t_archive>
    inline void serialize(t_archive &a, const unsigned int /*ver*/)
    {
      a & m_keys;
      a & m_creation_timestamp;
    }

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(m_keys)
      KV_SERIALIZE(m_creation_timestamp)
    END_KV_SERIALIZE_MAP()

  private:
    void set_null();
    account_keys m_keys;
    uint64_t m_creation_timestamp;
  };
  
  PACK(struct public_address_outer_blob
  {
    uint8_t m_ver;
    account_public_address m_address;
    uint8_t check_sum;
  })
  
  bool operator ==(const cryptonote::account_public_address& a, const cryptonote::account_public_address& b);
  bool operator !=(const cryptonote::account_public_address& a, const cryptonote::account_public_address& b);
  bool operator <(const cryptonote::account_public_address& a, const cryptonote::account_public_address& b);
}

namespace std {
  template<>
  struct hash<cryptonote::account_public_address> {
    std::size_t operator()(const cryptonote::account_public_address &v) const {
      return boost::hash<std::pair<crypto::public_key, crypto::public_key> >()(
          std::make_pair(v.m_spend_public_key, v.m_view_public_key));
    }
  };
}

namespace cryptonote {
  inline std::size_t hash_value(const cryptonote::account_public_address &v) {
    return std::hash<cryptonote::account_public_address>()(v);
  }
}
