// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "crypto.h"

#include "serialization/serialization.h"

#include "syncobj.h"
#include "serialization/keyvalue_serialization.h"

#include <string>
#include <unordered_map>


namespace crypto {
  class hash_cache
  {
  public:
    struct signed_hash_entry
    {
      crypto::hash block_id;
      crypto::hash work_hash;
      crypto::signature sig;
      
      crypto::hash get_prefix_hash() const
      {
        std::string s;
        s.append(reinterpret_cast<const char*>(&block_id), sizeof(block_id));
        s.append(reinterpret_cast<const char*>(&work_hash), sizeof(work_hash));
        return crypto::cn_fast_hash(s.data(), s.size());
      }
      
      BEGIN_SERIALIZE_OBJECT()
        FIELD(block_id)
        FIELD(work_hash)
        FIELD(sig)
      END_SERIALIZE()
      
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_VAL_POD_AS_BLOB(block_id)
        KV_SERIALIZE_VAL_POD_AS_BLOB(work_hash)
        KV_SERIALIZE_VAL_POD_AS_BLOB(sig)
      END_KV_SERIALIZE_MAP()
    };
    
    hash_cache() : m_priv_key_set(false) {}
    
    bool set_hash_signing_key(crypto::secret_key& prvk);
    inline bool is_hash_signing_key_set() const { return m_priv_key_set; }
    
    bool init(const std::string& config_folder);
    bool store() const;
    bool deinit() const;
    
    bool get_cached_longhash(const crypto::hash& block_id, crypto::hash& work_hash) const;
    bool add_cached_longhash(const crypto::hash& block_id, const crypto::hash& work_hash);
    bool get_signed_longhash_entry(const crypto::hash& block_id, crypto::hash_cache::signed_hash_entry& entry) const;
    bool get_signed_longhash(const crypto::hash& block_id, crypto::hash& work_hash) const;
    bool have_signed_longhash_for(const crypto::hash& block_id) const;
    
    bool add_signed_longhash(const crypto::hash_cache::signed_hash_entry& entry);
    bool create_signed_hash(const crypto::hash& block_id, const crypto::hash& work_hash, crypto::signature& sig);
    
    BEGIN_SERIALIZE_OBJECT()
      FIELD(m_hash_cache)
      FIELD(m_signed_hash_cache)
    END_SERIALIZE()
    
  private:
    bool m_priv_key_set;
    secret_key m_hash_signing_priv_key;
    
    std::unordered_map<crypto::hash, crypto::hash> m_hash_cache;
    std::unordered_map<crypto::hash, signed_hash_entry> m_signed_hash_cache;
    mutable epee::critical_section m_hashes_lock;
    
    std::string m_config_folder;
  };
  
  extern hash_cache g_hash_cache;
}
