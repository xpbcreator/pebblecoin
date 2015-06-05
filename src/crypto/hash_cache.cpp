// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "hash_cache.h"
#include "hash.h"
#include "hash_options.h"

#include "common/util.h"
#include "cryptonote_config.h"
#include "cryptonote_genesis_config.h"
#include "crypto/crypto.h"
#include "serialization/binary_utils.h"

#include "file_io_utils.h"
#include "string_tools.h"


namespace crypto
{
  bool hash_cache::init(const std::string& config_folder)
  {
    m_config_folder = config_folder;
    LOG_PRINT_L0("Loading hash cache...");
    
    const std::string filename = m_config_folder + "/" + CRYPTONOTE_HASHCACHEDATA_FILENAME;
    
    bool r = false;
    {
      CRITICAL_REGION_LOCAL(m_hashes_lock);
      std::string buf;
      r = epee::file_io_utils::load_file_to_string(filename, buf);
      if (r)
      {
        r = ::serialization::parse_binary(buf, *this);
      }
    }
    
    if (!r)
    {
      if (GENESIS_BLOCK_ID_HEX && GENESIS_WORK_HASH_HEX && GENESIS_HASH_SIGNATURE_HEX)
      {
        LOG_PRINT_L0("Can't load hash cache from file " << filename << ", initializing with genesis block");
        
        signed_hash_entry entry;
        
        epee::string_tools::hex_to_pod(GENESIS_BLOCK_ID_HEX, entry.block_id);
        epee::string_tools::hex_to_pod(GENESIS_WORK_HASH_HEX, entry.work_hash);
        epee::string_tools::hex_to_pod(GENESIS_HASH_SIGNATURE_HEX, entry.sig);
        
        if (!add_signed_longhash(entry))
        {
          LOG_ERROR("Failed to add signed genesis block work hash");
        }
      }
      else
      {
        LOG_PRINT_L0("Can't load hash cache from file " << filename << ", no genesis block hashes provided");        
      }
    }
    
    LOG_PRINT_GREEN("Hash cache initialized, have " << m_hash_cache.size() << " hashes and " << m_signed_hash_cache.size() << " signed hashes", LOG_LEVEL_0);
    
    return true;
  }
  
  bool hash_cache::store() const
  {
    LOG_PRINT_L0("Storing hash cache...");
    if (!tools::create_directories_if_necessary(m_config_folder))
    {
      LOG_PRINT_L0("Failed to create data directory: " << m_config_folder);
      return false;
    }
    
    const std::string filename = m_config_folder + "/" + CRYPTONOTE_HASHCACHEDATA_FILENAME;
    
    CRITICAL_REGION_LOCAL(m_hashes_lock);
    
    std::string buf;
    bool r = ::serialization::dump_binary(*this, buf);
    CHECK_AND_ASSERT_MES(r, false, "Failed to serialize hash_cache");
    r = r && epee::file_io_utils::save_string_to_file(filename, buf);
    CHECK_AND_ASSERT_MES(r, false, "Failed to save hashcache to file: " << filename);
    return true;
  }
  
  bool hash_cache::deinit() const
  {
    return store();
  }
  

  bool hash_cache::set_hash_signing_key(crypto::secret_key& prvk)
  {
    public_key pub_from_prvk;
    if (!secret_key_to_public_key(prvk, pub_from_prvk))
    {
      LOG_PRINT_RED_L0("Couldn't derive public key from secret key");
      return false;
    }
    
    public_key pub_from_config;
    epee::string_tools::hex_to_pod(HASH_SIGNING_TRUSTED_PUB_KEY, pub_from_config);
    
    if (pub_from_prvk != pub_from_config)
    {
      LOG_PRINT_RED_L0("Secret key derives to different public key");
      return false;
    }
    
    m_hash_signing_priv_key = prvk;
    m_priv_key_set = true;
    
    return true;
  }
  

  bool hash_cache::get_cached_longhash(const crypto::hash& block_id, crypto::hash& work_hash) const
  {
    CRITICAL_REGION_LOCAL(m_hashes_lock);
    const auto& mi = m_hash_cache.find(block_id);
    if (mi == m_hash_cache.end())
      return false;

    work_hash = mi->second;
    return true;
  }

  bool hash_cache::add_cached_longhash(const crypto::hash& block_id, const crypto::hash& work_hash)
  {
    CRITICAL_REGION_LOCAL(m_hashes_lock);
    m_hash_cache[block_id] = work_hash;
    return true;
  }
  
  bool hash_cache::get_signed_longhash_entry(const crypto::hash& block_id, signed_hash_entry& entry) const
  {
    CRITICAL_REGION_LOCAL(m_hashes_lock);
    const auto& mi = m_signed_hash_cache.find(block_id);
    
    if (mi == m_signed_hash_cache.end())
      return false;
    
    entry = mi->second;
    return true;
  }
  
  bool hash_cache::get_signed_longhash(const crypto::hash& block_id, crypto::hash& work_hash) const
  {
    signed_hash_entry e;
    
    if (!get_signed_longhash_entry(block_id, e))
      return false;
    
    work_hash = e.work_hash;
    return true;
  }

  bool hash_cache::have_signed_longhash_for(const crypto::hash& block_id) const
  {
    CRITICAL_REGION_LOCAL(m_hashes_lock);
    return m_signed_hash_cache.find(block_id) != m_signed_hash_cache.end();
  }

  
  bool hash_cache::add_signed_longhash(const signed_hash_entry& entry)
  {
    public_key pubk;
    epee::string_tools::hex_to_pod(HASH_SIGNING_TRUSTED_PUB_KEY, pubk);
    
    if (!check_signature(entry.get_prefix_hash(), pubk, entry.sig))
    {
      LOG_PRINT_RED_L0("Invalid signature on signed hash");
      return false;
    }
    
    CRITICAL_REGION_LOCAL(m_hashes_lock);
    m_signed_hash_cache[entry.block_id] = entry;
    return true;
  }

  bool hash_cache::create_signed_hash(const crypto::hash& block_id, const crypto::hash& work_hash, crypto::signature& sig)
  {
    if (!m_priv_key_set)
      return false;
    
    public_key pubk;
    epee::string_tools::hex_to_pod(HASH_SIGNING_TRUSTED_PUB_KEY, pubk);
    
    signed_hash_entry entry;
    entry.block_id = block_id;
    entry.work_hash = work_hash;

    generate_signature(entry.get_prefix_hash(),
                       pubk, m_hash_signing_priv_key,
                       entry.sig);
    
    if (!add_signed_longhash(entry))
    {
      LOG_PRINT_RED_L0("Sig that was just generated is invalid");
      return false;
    }
    
    sig = entry.sig;
    
    return true;
  }

  hash_cache g_hash_cache;
}

