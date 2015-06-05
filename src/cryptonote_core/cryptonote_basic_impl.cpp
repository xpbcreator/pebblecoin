// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include "include_base_utils.h"
#include "misc_language.h"
#include "string_tools.h"

#include "serialization/binary_utils.h"

#include "cryptonote_config.h"
#include "common/base58.h"
#include "crypto/hash.h"
#include "common/int-util.h"

#include "cryptonote_basic.h"
#include "cryptonote_basic_impl.h"
#include "cryptonote_format_utils.h"

using namespace epee;

namespace cryptonote {
  /************************************************************************/
  /* Cryptonote helper functions                                          */
  /************************************************************************/
  //-----------------------------------------------------------------------------------------------
  size_t get_max_block_size()
  {
    return CRYPTONOTE_MAX_BLOCK_SIZE;
  }
  //-----------------------------------------------------------------------------------------------
  size_t get_max_tx_size()
  {
    return CRYPTONOTE_MAX_TX_SIZE;
  }
  //-----------------------------------------------------------------------------------------------
  namespace detail
  {
    // exponential start of rewards to allow ironing out any launch difficulties
    uint64_t standard_reward = 300 * COIN;
    uint64_t warmup_period_rewards[] = {
      1,
      standard_reward / 10000,
      standard_reward / 1000,
      standard_reward / 100,
      standard_reward / 10,
      standard_reward
    };
    size_t num_warmup_periods = sizeof(warmup_period_rewards) / sizeof(uint64_t) - 1;
    uint64_t total_warmup_period() { return config::week_height() * 3; }
    uint64_t warmup_period_length() { return total_warmup_period() / num_warmup_periods; }
    
    // linear gradual cooldown over years
    size_t num_reward_eras = 4;
    uint64_t reward_era_length() { return 3 * config::year_height(); }
    uint64_t penalty_per_era = standard_reward / (num_reward_eras - 1);
  }
  
  uint64_t get_base_reward(uint64_t target_block_height)
  {
    using namespace detail;
    
    if (target_block_height == 0) // genesis block
      return 0;
    
    if (config::in_pos_era(target_block_height)) // no more rewards once switch to DPOS
    {
      return 0;
    }
    
    if (config::no_reward_ramp) // true for testing
    {
      return standard_reward;
    }
    
    if (target_block_height < total_warmup_period())
    {
      size_t subperiod = target_block_height / warmup_period_length();
      uint64_t height_in_period = target_block_height % warmup_period_length();
      uint64_t start_reward = warmup_period_rewards[subperiod];
      uint64_t end_reward = warmup_period_rewards[subperiod + 1];
      
      return start_reward + ((end_reward - start_reward) * height_in_period) / warmup_period_length();
    }
    
    size_t reward_era = target_block_height / reward_era_length();
    if (reward_era >= num_reward_eras)
      return 0;
    
    return standard_reward - penalty_per_era * reward_era;
  }
  
  bool get_block_reward(size_t median_size, size_t current_block_size, uint64_t already_generated_coins, uint64_t target_block_height, uint64_t &reward) {
    uint64_t base_reward = get_base_reward(target_block_height);
    LOG_PRINT_L4("Base reward for height " << target_block_height << " is " << base_reward);
    
    //make it soft
    if (median_size < CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE) {
      median_size = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE;
    }

    if (current_block_size <= median_size) {
      reward = base_reward;
      return true;
    }

    if(current_block_size > 2 * median_size) {
      LOG_PRINT_L4("Block cumulative size is too big: " << current_block_size << ", expected less than " << 2 * median_size);
      return false;
    }
    
    CHECK_AND_ASSERT(median_size < std::numeric_limits<uint32_t>::max(), false);
    CHECK_AND_ASSERT(current_block_size < std::numeric_limits<uint32_t>::max(), false);
    
    uint64_t product_hi;
    uint64_t product_lo = mul128(base_reward, current_block_size * (2 * median_size - current_block_size), &product_hi);
    
    uint64_t reward_hi;
    uint64_t reward_lo;
    div128_32(product_hi, product_lo, static_cast<uint32_t>(median_size), &reward_hi, &reward_lo);
    div128_32(reward_hi, reward_lo, static_cast<uint32_t>(median_size), &reward_hi, &reward_lo);
    CHECK_AND_ASSERT(0 == reward_hi, false);
    CHECK_AND_ASSERT(reward_lo < base_reward || base_reward == 0, false);
    
    reward = reward_lo;
    return true;
  }
  //------------------------------------------------------------------------------------
  uint8_t get_account_address_checksum(const public_address_outer_blob& bl)
  {
    const unsigned char* pbuf = reinterpret_cast<const unsigned char*>(&bl);
    uint8_t summ = 0;
    for(size_t i = 0; i!= sizeof(public_address_outer_blob)-1; i++)
      summ += pbuf[i];

    return summ;
  }
  //-----------------------------------------------------------------------
  std::string get_account_address_as_str(const account_public_address& adr)
  {
    return tools::base58::encode_addr(cryptonote::config::address_base58_prefix(), t_serializable_object_to_blob(adr));
  }
  //-----------------------------------------------------------------------
  bool is_coinbase(const transaction& tx)
  {
    if(tx.ins().size() != 1)
      return false;

    if(tx.ins()[0].type() != typeid(txin_gen))
      return false;

    return true;
  }
  //-----------------------------------------------------------------------
  bool get_account_address_from_str(account_public_address& adr, const std::string& str)
  {
    if (2 * sizeof(public_address_outer_blob) != str.size())
    {
      blobdata data;
      uint64_t prefix;
      if (!tools::base58::decode_addr(str, prefix, data))
      {
        LOG_PRINT_L1("Invalid address format");
        return false;
      }

      if (cryptonote::config::address_base58_prefix() != prefix)
      {
        LOG_PRINT_L1("Wrong address prefix: " << prefix << ", expected " << cryptonote::config::address_base58_prefix());
        return false;
      }

      if (!::serialization::parse_binary(data, adr))
      {
        LOG_PRINT_L1("Account public address keys can't be parsed");
        return false;
      }

      if (!crypto::check_key(adr.m_spend_public_key) || !crypto::check_key(adr.m_view_public_key))
      {
        LOG_PRINT_L1("Failed to validate address keys");
        return false;
      }
    }
    else
    {
      // Old address format
      std::string buff;
      if(!string_tools::parse_hexstr_to_binbuff(str, buff))
        return false;

      if(buff.size()!=sizeof(public_address_outer_blob))
      {
        LOG_PRINT_L1("Wrong public address size: " << buff.size() << ", expected size: " << sizeof(public_address_outer_blob));
        return false;
      }

      public_address_outer_blob blob = *reinterpret_cast<const public_address_outer_blob*>(buff.data());


      if(blob.m_ver > CRYPTONOTE_PUBLIC_ADDRESS_TEXTBLOB_VER)
      {
        LOG_PRINT_L1("Unknown version of public address: " << blob.m_ver << ", expected " << CRYPTONOTE_PUBLIC_ADDRESS_TEXTBLOB_VER);
        return false;
      }

      if(blob.check_sum != get_account_address_checksum(blob))
      {
        LOG_PRINT_L1("Wrong public address checksum");
        return false;
      }

      //we success
      adr = blob.m_address;
    }

    return true;
  }

  /*  bool operator ==(const cryptonote::transaction& a, const cryptonote::transaction& b) {
    return a.version == b.version &&
           a.unlock_time == b.unlock_time &&
           a.extra == b.extra &&
           a.vin == b.vin &&
           a.vout == b.vout &&
           a.vin_coin_types == b.vin_coin_types &&
           a.vout_coin_types == b.vout_coin_types;
  }

  bool operator ==(const cryptonote::block& a, const cryptonote::block& b) {
    if (is_pow_block(a)) {
      return a.major_version == b.major_version &&
             a.minor_version == b.minor_version &&
             a.timestamp = b.timestamp &&
             a.prev_id == b.prev_id &&
             a.nonce == b.nonce &&
             a.miner_tx == b.miner_tx &&
             a.tx_hashes == b.tx_hashes;
    }
    else {
      return a.major_version == b.major_version &&
             a.minor_version == b.minor_version &&
             a.timestamp = b.timestamp &&
             a.prev_id == b.prev_id &&
             a.miner_tx == b.miner_tx &&
             a.tx_hashes == b.tx_hashes &&
             a.signing_delegate_id == b.signing_delegate_id &&
             a.dpos_sig == b.dpos_sig;
    }
  }*/
  
  // rough & ready easy equality for sanity checking only
  bool operator ==(const cryptonote::transaction& a, const cryptonote::transaction& b) {
    return cryptonote::get_transaction_hash(a) == cryptonote::get_transaction_hash(b);
  }

  bool operator ==(const cryptonote::block& a, const cryptonote::block& b) {
    return cryptonote::get_block_hash(a) == cryptonote::get_block_hash(b);
  }
  
  bool operator !=(const cryptonote::transaction& a, const cryptonote::transaction& b) {
    return !(a == b);
  }
  
  bool operator !=(const cryptonote::block& a, const cryptonote::block& b) {
    return !(a == b);
  }
  //-----------------------------------------------------------------------
  std::ostream &operator <<(std::ostream &o, const cryptonote::account_public_address& v)
  {
    return o << get_account_address_as_str(v);
  }
}

//--------------------------------------------------------------------------------
