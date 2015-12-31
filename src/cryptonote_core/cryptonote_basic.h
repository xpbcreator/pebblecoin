// Copyright (c) 2014 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <vector>

#include <boost/variant.hpp>

#include "string_tools.h"
#include "misc_language.h"
#include "serialization/keyvalue_serialization.h" // eepe named serialization

#include "serialization/serialization.h"
#include "serialization/binary_archive.h"
#include "serialization/json_archive.h"
#include "serialization/debug_archive.h"

#include "common/types.h"
#include "crypto/crypto.h"
#include "crypto/hash.h"

#include "tx_extra.h"
#include "account.h"
#include "coin_type.h"
#include "tx_versions.h"

#define CURRENCY_DESCRIPTION_MAX_SIZE 256
#define CONTRACT_DESCRIPTION_MAX_SIZE 1024

namespace cryptonote
{
  typedef std::vector<crypto::signature> ring_signature;
  
  //---------------------------------------------------------------

  const static delegate_votes DPOS_NO_VOTES = delegate_votes();
  
  //---------------------------------------------------------------
  /* outputs */

  struct txout_to_script
  {
    std::vector<crypto::public_key> keys;
    std::vector<uint8_t> script;

    BEGIN_SERIALIZE_OBJECT()
      FIELD(keys)
      FIELD(script)
    END_SERIALIZE()
  };

  struct txout_to_scripthash
  {
    crypto::hash hash;
  };

  struct txout_to_key
  {
    txout_to_key() { }
    txout_to_key(const crypto::public_key &_key) : key(_key) { }
    crypto::public_key key;
  };
  
  //---------------------------------------------------------------
  /* inputs */

  struct txin_gen
  {
    size_t height;

    BEGIN_SERIALIZE_OBJECT()
      VARINT_FIELD(height)
    END_SERIALIZE()
  };
  
  struct txin_to_script
  {
    crypto::hash prev;
    size_t prevout;
    std::vector<uint8_t> sigset;

    BEGIN_SERIALIZE_OBJECT()
      FIELD(prev)
      VARINT_FIELD(prevout)
      FIELD(sigset)
    END_SERIALIZE()
  };

  struct txin_to_scripthash
  {
    crypto::hash prev;
    size_t prevout;
    txout_to_script script;
    std::vector<uint8_t> sigset;

    BEGIN_SERIALIZE_OBJECT()
      FIELD(prev)
      VARINT_FIELD(prevout)
      FIELD(script)
      FIELD(sigset)
    END_SERIALIZE()
  };

  struct txin_to_key
  {
    uint64_t amount;
    std::vector<uint64_t> key_offsets;
    crypto::key_image k_image;      // double spending protection

    BEGIN_SERIALIZE_OBJECT()
      VARINT_FIELD(amount)
      FIELD(key_offsets)
      FIELD(k_image)
    END_SERIALIZE()
  };
  
  struct txin_mint
  {
    uint64_t currency;             // unique currency id - non-zero, unique with contract id as well
    std::string description;       // unique <= CURRENCY_DESCRIPTION_MAX_SIZE-char description
    uint64_t decimals;             // divisible by 10^decimals
    uint64_t amount;               // number of atomic units to mint
    crypto::public_key remint_key; // key which can remint; null for no remint
    
    BEGIN_SERIALIZE_OBJECT()
      VARINT_FIELD(currency)
      FIELD(description)
      VARINT_FIELD(decimals)
      VARINT_FIELD(amount)
      FIELD(remint_key)
    END_SERIALIZE()
  };
  
  struct txin_remint
  {
    uint64_t currency;                 // currency being reminted
    uint64_t amount;                   // number of atomic units to mint
    crypto::public_key new_remint_key; // new key which can remint; null for no more remint
    crypto::signature sig;             // sig of hash of above with old remint_key
    
    crypto::hash get_prefix_hash() const
    {
      std::string s;
      s.append("txin_remint sig");
      s.append(reinterpret_cast<const char*>(&currency), sizeof(currency));
      s.append(reinterpret_cast<const char*>(&amount), sizeof(amount));
      s.append(reinterpret_cast<const char*>(&new_remint_key), sizeof(new_remint_key));
      return crypto::cn_fast_hash(s.data(), s.size());
    }
    
    BEGIN_SERIALIZE_OBJECT()
      VARINT_FIELD(currency)
      VARINT_FIELD(amount)
      FIELD(new_remint_key)
      FIELD(sig)
    END_SERIALIZE()
  };
  
  struct txin_create_contract
  {
    uint64_t contract;               // unique contract id - non-zero, unique with currency id as well
    std::string description;         // unique <= CONTRACT_DESCRIPTION_MAX_SIZE-char description
    crypto::public_key grading_key;  // key used to grade the contract, must be valid & non-null
    // fee_scale is pct of coins minted that can be spent by grader from txin_grade_contract,
    // scaled from 0 to GRADE_SCALE_MAX
    // e.g. if grade is 90% (900,000,000), and fee_scale is 5% (50,000,000), then:
    //  # contract coins | # contract coins can spend  | # backing coins can spend | # grader can spend (fee)
    // ------------------+-----------------------------+---------------------------+-------------------------------
    //     10000         |         8550                |           950             |     450 + 50 = 500
    //       300         |          257                |            29             |      13 +  1 =  14
    // fee doesn't apply if the contract expires
    uint32_t fee_scale;
    uint64_t expiry_block;           // block the contract expires at if not graded by then
    uint32_t default_grade;          // the grade if the contract expires without being graded - no fees in this case
    
    BEGIN_SERIALIZE_OBJECT()
      VARINT_FIELD(contract)
      FIELD(description)
      FIELD(grading_key)
      VARINT_FIELD(fee_scale)
      VARINT_FIELD(expiry_block)
      VARINT_FIELD(default_grade)
    END_SERIALIZE()
  };
  
  struct txin_mint_contract
  {
    uint64_t contract;           // contract currency id
    uint64_t backed_by_currency; // backed_by currency
    uint64_t amount;             // amount of backing + contract coins to create
    
    BEGIN_SERIALIZE_OBJECT()
      VARINT_FIELD(contract)
      VARINT_FIELD(backed_by_currency)
      VARINT_FIELD(amount)
    END_SERIALIZE()
  };
  
  struct txin_grade_contract
  {
    uint64_t contract;     // contract being graded
    uint32_t grade;        // the grade scaled from 0 to GRADE_SCALE_MAX
    std::map<uint64_t, uint64_t> fee_amounts; // map of currency to the fees in that currency the grader can take
                                              // unclaimed fees are destroyed
    crypto::signature sig; // sig of above signed with secret grading_key
    
    crypto::hash get_prefix_hash() const
    {
      std::string s;
      s.append("txin_grade_contract sig");
      s.append(reinterpret_cast<const char*>(&contract), sizeof(contract));
      s.append(reinterpret_cast<const char*>(&grade), sizeof(grade));
      for (const auto& item : fee_amounts) {
        s.append(reinterpret_cast<const char*>(&item.first), sizeof(item.first));
        s.append(reinterpret_cast<const char*>(&item.second), sizeof(item.first));
      }
      return crypto::cn_fast_hash(s.data(), s.size());
    }
    
    BEGIN_SERIALIZE_OBJECT()
      VARINT_FIELD(contract)
      VARINT_FIELD(grade)
      FIELD(fee_amounts)
      FIELD(sig)
    END_SERIALIZE()
  };
  
  struct txin_resolve_bc_coins
  {
    uint64_t contract;         // contract being resolved
    uint8_t is_backing_coins;  // true for BackingCoins, false for ContractCoins
    uint64_t backing_currency; // currency the coins are backed by
    uint64_t source_amount;    // amount of Backing/Contract coins used as the source for this input
    uint64_t graded_amount;    // amount of coins of the backing currency spendable from this input
                               // must be exactly (grade_contract_amount/grade_backing_amount)(source_amount, fee_scale, grade)
    
    BEGIN_SERIALIZE_OBJECT()
      VARINT_FIELD(contract)
      VARINT_FIELD(is_backing_coins)
      VARINT_FIELD(backing_currency)
      VARINT_FIELD(source_amount)
      VARINT_FIELD(graded_amount)
    END_SERIALIZE()
  };
  
  struct txin_fuse_bc_coins
  {
    uint64_t contract;
    uint64_t backing_currency;
    uint64_t amount;

    BEGIN_SERIALIZE_OBJECT()
      VARINT_FIELD(contract)
      VARINT_FIELD(backing_currency)
      VARINT_FIELD(amount)
    END_SERIALIZE()
  };
  
  struct txin_register_delegate
  {
    delegate_id_t delegate_id;
    uint64_t registration_fee;
    account_public_address delegate_address;
    
    BEGIN_SERIALIZE_OBJECT()
      VARINT_FIELD(delegate_id);
      VARINT_FIELD(registration_fee);
      FIELD(delegate_address);
    END_SERIALIZE()
  };
  
  struct txin_vote
  {
    txin_to_key ink; // including double-voting protectin
    uint16_t seq;     // 0 for first vote, 1 for 1st change vote, 2 for 2nd change vote, ...
    delegate_votes votes;
    
    BEGIN_SERIALIZE_OBJECT()
      FIELD(ink)
      VARINT_FIELD(seq)
      FIELD(votes)
    END_SERIALIZE()
  };
  
  typedef boost::variant<txin_gen, txin_to_script, txin_to_scripthash, txin_to_key, txin_mint, txin_remint,
                         txin_create_contract, txin_mint_contract, txin_grade_contract,
                         txin_resolve_bc_coins, txin_fuse_bc_coins,
                         txin_register_delegate, txin_vote> txin_v;

  typedef boost::variant<txout_to_script, txout_to_scripthash, txout_to_key> txout_target_v;

  struct tx_out
  {
    uint64_t amount;
    txout_target_v target;
    
    tx_out() { }
    tx_out(uint64_t _amount, const txout_target_v& _target) : amount(_amount), target(_target) { }

    BEGIN_SERIALIZE_OBJECT()
      VARINT_FIELD(amount)
      FIELD(target)
    END_SERIALIZE()
  };

  size_t inp_minimum_tx_version(const txin_v& inp);
  size_t outp_minimum_tx_version(const tx_out& outp);
  
  class tx_tester;
  class transaction_prefix
  {
    friend class tx_tester;
    
  public:
    // tx information
    size_t   version;
    uint64_t unlock_time;  //number of block (or time), used as a limitation like: spend this tx not early then block/time

    std::vector<uint8_t> extra;

  protected:
    std::vector<txin_v> vin;
    std::vector<tx_out> vout;
    // serialized differently depending on the version
    std::vector<coin_type> vin_coin_types;
    std::vector<coin_type> vout_coin_types;

  public:
    const std::vector<txin_v>& ins() const { return vin; }
    const std::vector<tx_out>& outs() const { return vout; }

    void add_in(const txin_v& inp, const coin_type& ct)
    {
      vin.push_back(inp);
      vin_coin_types.push_back(ct);
      
      if (!has_valid_coin_types()) {
        throw std::runtime_error("Added invalid input for tx version");
      }
    }
    void add_out(const tx_out& outp, const coin_type& ct)
    {
      vout.push_back(outp);
      vout_coin_types.push_back(ct);
      
      if (!has_valid_coin_types()) {
        throw std::runtime_error("Added invalid output for tx version");
      }
    }
    
    void clear_ins();
    void clear_outs();
    
    bool has_valid_coin_types() const;    
    bool has_valid_in_out_types() const;

    const coin_type& in_cp(size_t vin_index) const
    {
      if (vin_index >= vin_coin_types.size())
      {
        throw std::runtime_error("Invalid vin_index >= vin_coin_types.size()");
      }
      
      return vin_coin_types[vin_index];
    }

    const coin_type& out_cp(size_t vout_index) const
    {
      if (vout_index >= vout_coin_types.size())
      {
        throw std::runtime_error("Invalid vout_index >= vout_coin_types()");
      }
      
      return vout_coin_types[vout_index];
    }
    
    void replace_vote_seqs(const std::map<crypto::key_image, uint64_t>& key_image_seqs);
    
  public:
    BEGIN_SERIALIZE()
      // NOTE: serialization code must be updated both here and in boost_serialize() below
      // -- vanilla --
      VARINT_FIELD(version)
    
      if(version > CURRENT_TRANSACTION_VERSION)
      {
        LOG_ERROR("Can't serialize tx with version " << version << " > CURRENT_TRANSACTION_VERSION " << CURRENT_TRANSACTION_VERSION);
        return false;
      }
    
      VARINT_FIELD(unlock_time)
      FIELD(vin)
      FIELD(vout)
      FIELD(extra)
    
      // -- coin type fields --
      switch (version) {
        case 0:
        case VANILLA_TRANSACTION_VERSION:
        case DPOS_TRANSACTION_VERSION: {
          // no coin type fields at all
          if (typename Archive<W>::is_saving())
          {
          }
          else
          {
            vin_coin_types.clear();
            vout_coin_types.clear();
            for (const auto& inp : vin) {
              (void)inp;
              vin_coin_types.push_back(CP_XPB);
            }
            for (const auto& outp : vout) {
              (void)outp;
              vout_coin_types.push_back(CP_XPB);
            }
          }
        } break;
          
        case CURRENCY_TRANSACTION_VERSION: {
          // only the currency vectors
          std::vector<uint64_t> in_currencies, out_currencies;
          if (typename Archive<W>::is_saving())
          {
            for (const auto& ct : vin_coin_types)
              in_currencies.push_back(ct.currency);
            for (const auto& ct : vout_coin_types)
              out_currencies.push_back(ct.currency);
            FIELD(in_currencies);
            FIELD(out_currencies);
          }
          else
          {
            FIELD(in_currencies);
            FIELD(out_currencies);
            vin_coin_types.clear();
            vout_coin_types.clear();
            for (const auto& currency : in_currencies)
              vin_coin_types.push_back(coin_type(currency, NotContract, BACKED_BY_N_A));
            for (const auto& currency : out_currencies)
              vout_coin_types.push_back(coin_type(currency, NotContract, BACKED_BY_N_A));
          }
        } break;

        case CONTRACT_TRANSACTION_VERSION: {
          // save/load as coin type vector itself
          FIELD(vin_coin_types);
          FIELD(vout_coin_types);
        } break;
          
        default:
          LOG_ERROR("Error non-boost serializing tx, unknown version: " << version);
          return false;
      }
    
      // -- sanity checks --
      if (!has_valid_coin_types())
      {
        LOG_ERROR("Error non-boost serializing tx, saving=" << (typename Archive<W>::is_saving()) << ", invalid coin types");
        return false;
      }
      if (!has_valid_in_out_types())
      {
        LOG_ERROR("Error non-boost serializing tx, saving=" << (typename Archive<W>::is_saving()) << ", invalid in/out types");
        return false;
      }
    
    END_SERIALIZE()

  protected:
    transaction_prefix(){}
  };

  class transaction: public transaction_prefix
  {
    friend class tx_tester;
    
  public:
    std::vector<std::vector<crypto::signature> > signatures; //count signatures  always the same as inputs count

    transaction();
    virtual ~transaction();
    void set_null();

    BEGIN_SERIALIZE_OBJECT()
      FIELDS(*static_cast<transaction_prefix *>(this))

      ar.tag("signatures");
      ar.begin_array();
      PREPARE_CUSTOM_VECTOR_SERIALIZATION(ins().size(), signatures);
      bool signatures_not_expected = signatures.empty();
      if (!signatures_not_expected && ins().size() != signatures.size())
      {
        LOG_ERROR("Can't serialize tx, signatures were expected and size doesn't match");
        return false;
      }

      for (size_t i = 0; i < ins().size(); ++i)
      {
        size_t signature_size = get_signature_size(ins()[i]);
        if (signatures_not_expected)
        {
          if (0 == signature_size)
            continue;
          else
          {
            LOG_ERROR("Can't serialize tx, signatures not expected but signature_size is not 0");
            return false;
          }
        }

        PREPARE_CUSTOM_VECTOR_SERIALIZATION(signature_size, signatures[i]);
        if (signature_size != signatures[i].size())
        {
          LOG_ERROR("Can't serialize tx, signature_size " << signature_size << " does not match signatures[" << i << "].size " << signatures[i].size());
          return false;
        }

        FIELDS(signatures[i]);

        if (ins().size() - i > 1)
          ar.delimit_array();
      }
      ar.end_array();
    END_SERIALIZE()

    template <class Archive>
    bool boost_serialize(Archive& a)
    {
      // NOTE: serialization code must be updated both here and in BEGIN_SERIALIZE() above
      // -- vanilla --
      a & version;
      a & unlock_time;
      a & vin;
      a & vout;
      a & extra;
      a & signatures;
      
      // -- coin type fields --
      switch (version) {
        case 0:
        case VANILLA_TRANSACTION_VERSION:
        case DPOS_TRANSACTION_VERSION:
        {
          // no coin type fields at all
          if (typename Archive::is_saving())
          {
          }
          else
          {
            for (const auto& inp : vin) {
              (void)inp;
              vin_coin_types.push_back(CP_XPB);
            }
            for (const auto& outp : vout) {
              (void)outp;
              vout_coin_types.push_back(CP_XPB);
            }
          }
        } break;
          
        case CURRENCY_TRANSACTION_VERSION: {
          // only the currency vectors
          std::vector<uint64_t> in_currencies, out_currencies;
          if (typename Archive::is_saving())
          {
            for (const auto& ct : vin_coin_types)
              in_currencies.push_back(ct.currency);
            for (const auto& ct : vout_coin_types)
              out_currencies.push_back(ct.currency);
            a & in_currencies;
            a & out_currencies;
          }
          else
          {
            a & in_currencies;
            a & out_currencies;
            for (const auto& currency : in_currencies)
              vin_coin_types.push_back(coin_type(currency, NotContract, BACKED_BY_N_A));
            for (const auto& currency : out_currencies)
              vout_coin_types.push_back(coin_type(currency, NotContract, BACKED_BY_N_A));
          }
        } break;
          
        case CONTRACT_TRANSACTION_VERSION: {
          // save/load as coin type vector itself
          a & vin_coin_types;
          a & vout_coin_types;
        } break;
          
        default:
          LOG_ERROR("Error boost serializing tx, saving=" << (typename Archive::is_saving())
                    << ", unknown version: " << version);
          return false;
      }
      
      // -- sanity checks --
      if (!has_valid_coin_types())
      {
        LOG_ERROR("Error boost serializing tx, saving=" << (typename Archive::is_saving()) << ", invalid coin types");
        return false;
      }
      if (!has_valid_in_out_types())
      {
        LOG_ERROR("Error boost serializing tx, saving=" << (typename Archive::is_saving()) << ", invalid in/out types");
        return false;
      }

      return true;
    }

  private:
    static size_t get_signature_size(const txin_v& tx_in);
  };
  
  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  struct block_header
  {
    uint8_t major_version;
    uint8_t minor_version;
    uint64_t timestamp;
    crypto::hash  prev_id;
    uint32_t nonce;

    BEGIN_SERIALIZE()
      VARINT_FIELD(major_version)
      if(major_version > CURRENT_BLOCK_MAJOR_VERSION)
      {
        LOG_ERROR("Error non-boost serializing block: major_version " << major_version << " is too large (> " <<
                  CURRENT_BLOCK_MAJOR_VERSION << ")");
        return false;
      }
      VARINT_FIELD(minor_version)
      VARINT_FIELD(timestamp)
      FIELD(prev_id)
      // nonce is only relevant for pow blocks
      if (major_version <= POW_BLOCK_MAJOR_VERSION)
      {
        FIELD(nonce)
      }
    END_SERIALIZE()
  };

  struct block: public block_header
  {
    transaction miner_tx;
    std::vector<crypto::hash> tx_hashes;
    // dpos
    delegate_id_t signing_delegate_id;
    crypto::signature dpos_sig;

    BEGIN_SERIALIZE_OBJECT()
      FIELDS(*static_cast<block_header *>(this))
      FIELD(miner_tx)
      FIELD(tx_hashes)
      if (major_version >= DPOS_BLOCK_MAJOR_VERSION)
      {
        VARINT_FIELD(signing_delegate_id);
        FIELD(dpos_sig);
      }
    END_SERIALIZE()
    
    template <class Archive>
    bool boost_serialize(Archive& a)
    {
      a & major_version;
      if (major_version > CURRENT_BLOCK_MAJOR_VERSION)
      {
        LOG_ERROR("Error boost-serializing a block: major_version " << major_version << " is too large (> " << CURRENT_BLOCK_MAJOR_VERSION << ")");
        return false;
      }
      a & minor_version;
      a & timestamp;
      a & prev_id;
      // nonce is only relevant for pow blocks
      if (major_version <= POW_BLOCK_MAJOR_VERSION)
      {
        a & nonce;
      }
      //------------------
      a & miner_tx;
      a & tx_hashes;
      if (major_version >= DPOS_BLOCK_MAJOR_VERSION)
      {
        a & signing_delegate_id;
        a & dpos_sig;
      }
      
      return true;
    }
  };
  
  // for functional.h
  struct out_getter {
    const txout_target_v& operator()(const tx_out& the_out) {
      return the_out.target;
    }
    txout_target_v& operator()(tx_out& the_out) {
      return the_out.target;
    }
  };
}

BLOB_SERIALIZER(cryptonote::txout_to_key);
BLOB_SERIALIZER(cryptonote::txout_to_scripthash);

VARIANT_TAG(binary_archive, cryptonote::txin_gen, 0xff);
VARIANT_TAG(binary_archive, cryptonote::txin_to_script, 0x0);
VARIANT_TAG(binary_archive, cryptonote::txin_to_scripthash, 0x1);
VARIANT_TAG(binary_archive, cryptonote::txin_to_key, 0x2);
VARIANT_TAG(binary_archive, cryptonote::txin_mint, 0x3);
VARIANT_TAG(binary_archive, cryptonote::txin_remint, 0x4);
VARIANT_TAG(binary_archive, cryptonote::txin_create_contract, 0x5);
VARIANT_TAG(binary_archive, cryptonote::txin_mint_contract, 0x6);
VARIANT_TAG(binary_archive, cryptonote::txin_grade_contract, 0x7);
VARIANT_TAG(binary_archive, cryptonote::txin_resolve_bc_coins, 0x8);
VARIANT_TAG(binary_archive, cryptonote::txin_fuse_bc_coins, 0x9);
VARIANT_TAG(binary_archive, cryptonote::txin_register_delegate, 0xa);
VARIANT_TAG(binary_archive, cryptonote::txin_vote, 0xb);
VARIANT_TAG(binary_archive, cryptonote::txout_to_script, 0x0);
VARIANT_TAG(binary_archive, cryptonote::txout_to_scripthash, 0x1);
VARIANT_TAG(binary_archive, cryptonote::txout_to_key, 0x2);
VARIANT_TAG(binary_archive, cryptonote::transaction, 0xcc);
VARIANT_TAG(binary_archive, cryptonote::block, 0xbb);

VARIANT_TAG(json_archive, cryptonote::txin_gen, "gen");
VARIANT_TAG(json_archive, cryptonote::txin_to_script, "script");
VARIANT_TAG(json_archive, cryptonote::txin_to_scripthash, "scripthash");
VARIANT_TAG(json_archive, cryptonote::txin_to_key, "key");
VARIANT_TAG(json_archive, cryptonote::txin_mint, "mint");
VARIANT_TAG(json_archive, cryptonote::txin_remint, "remint");
VARIANT_TAG(json_archive, cryptonote::txin_create_contract, "create_contract");
VARIANT_TAG(json_archive, cryptonote::txin_mint_contract, "mint_contract");
VARIANT_TAG(json_archive, cryptonote::txin_grade_contract, "grade_contract");
VARIANT_TAG(json_archive, cryptonote::txin_resolve_bc_coins, "resolve_bc_coins");
VARIANT_TAG(json_archive, cryptonote::txin_fuse_bc_coins, "fuse_bc_coins");
VARIANT_TAG(json_archive, cryptonote::txin_register_delegate, "register_delegate");
VARIANT_TAG(json_archive, cryptonote::txin_vote, "vote");
VARIANT_TAG(json_archive, cryptonote::txout_to_script, "script");
VARIANT_TAG(json_archive, cryptonote::txout_to_scripthash, "scripthash");
VARIANT_TAG(json_archive, cryptonote::txout_to_key, "key");
VARIANT_TAG(json_archive, cryptonote::transaction, "tx");
VARIANT_TAG(json_archive, cryptonote::block, "block");

VARIANT_TAG(debug_archive, cryptonote::txin_gen, "gen");
VARIANT_TAG(debug_archive, cryptonote::txin_to_script, "script");
VARIANT_TAG(debug_archive, cryptonote::txin_to_scripthash, "scripthash");
VARIANT_TAG(debug_archive, cryptonote::txin_to_key, "key");
VARIANT_TAG(debug_archive, cryptonote::txin_mint, "mint");
VARIANT_TAG(debug_archive, cryptonote::txin_remint, "remint");
VARIANT_TAG(debug_archive, cryptonote::txin_create_contract, "create_contract");
VARIANT_TAG(debug_archive, cryptonote::txin_mint_contract, "mint_contract");
VARIANT_TAG(debug_archive, cryptonote::txin_grade_contract, "grade_contract");
VARIANT_TAG(debug_archive, cryptonote::txin_resolve_bc_coins, "resolve_bc_coins");
VARIANT_TAG(debug_archive, cryptonote::txin_fuse_bc_coins, "fuse_bc_coins");
VARIANT_TAG(debug_archive, cryptonote::txin_register_delegate, "register_delegate");
VARIANT_TAG(debug_archive, cryptonote::txin_vote, "vote");
VARIANT_TAG(debug_archive, cryptonote::txout_to_script, "script");
VARIANT_TAG(debug_archive, cryptonote::txout_to_scripthash, "scripthash");
VARIANT_TAG(debug_archive, cryptonote::txout_to_key, "key");
VARIANT_TAG(debug_archive, cryptonote::transaction, "tx");
VARIANT_TAG(debug_archive, cryptonote::block, "block");
