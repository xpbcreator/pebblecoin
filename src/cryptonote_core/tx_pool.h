// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <set>
#include <unordered_map>
#include <unordered_set>
#include <functional>

#include <boost/serialization/version.hpp>
#include <boost/utility.hpp>
#include <boost/functional/hash.hpp>
#include <boost/variant.hpp>
#include <boost/serialization/variant.hpp>

#include "include_base_utils.h"
#include "string_tools.h"
#include "syncobj.h"

#include "crypto/hash.h"
#include "crypto/crypto.h"
#include "crypto/crypto_basic_impl.h"

#include "cryptonote_basic.h"
#include "cryptonote_basic_impl.h"
#include "verification_context.h"
#include "i_tx_pool_callback.h"

namespace cryptonote
{
  namespace detail
  {
    // these determine whether a transaction can be added to the pool
    // they are designed to disallow transactions that can't possibly succeed because they conflict with other
    // transactions in the memory pool.  At the same time they should never ban transactions that can possibly
    // succeed, especially in the case of blockchain reorgs.
    struct txin_invalid_info
    {
      txin_invalid_info() { }
    };
    
    // in_to_key is only valid if there's no other in_to_key spending that key image.
    struct txin_to_key_info
    {
      crypto::key_image k_image;
      
      txin_to_key_info() { }
      txin_to_key_info(const txin_to_key& inp) : k_image(inp.k_image) { }
    };
    
    // only way mint could not succeed is if there's another one with the exact (curreny, description) pair.
    // if there is (0xbob, "Bob"), then (0xbob, "Foo") could possibly succeed, because if a different currency with
    // "Bob" is minted, the first becomes impossible, paving the way for the 2nd one.
    struct txin_mint_info
    {
      uint64_t currency;
      std::string description;
      
      txin_mint_info() { }
      // overlap between minting subcurrencies and creating contracts
      txin_mint_info(const txin_mint& inp) : currency(inp.currency), description(inp.description) { }
      txin_mint_info(const txin_create_contract& inp) : currency(inp.contract), description(inp.description) { }
    };
    
    struct txin_remint_info
    {
      uint64_t currency;
      crypto::signature sig;
      
      txin_remint_info() { }
      txin_remint_info(const txin_remint& inp) : currency(inp.currency), sig(inp.sig) { }
    };
    
    // for transactions that never conflict with anything
    struct txin_no_conflict_info
    {
      txin_no_conflict_info() { }
      txin_no_conflict_info(const txin_mint_contract& inp) { }
      txin_no_conflict_info(const txin_resolve_bc_coins& inp) { }
      txin_no_conflict_info(const txin_fuse_bc_coins& inp) { }
    };
    
    struct txin_grade_contract_info
    {
      uint64_t contract;
      std::map<uint64_t, uint64_t> fee_amounts; //fees may change after reorg
      
      txin_grade_contract_info() { }
      txin_grade_contract_info(const txin_grade_contract& inp) : contract(inp.contract), fee_amounts(inp.fee_amounts) { }
    };
    
    struct txin_register_delegate_info
    {
      delegate_id_t delegate_id;
      account_public_address addr;
      
      txin_register_delegate_info() { }
      txin_register_delegate_info(const txin_register_delegate& inp)
          : delegate_id(inp.delegate_id), addr(inp.delegate_address) { }
    };
    
    struct txin_vote_info
    {
      crypto::key_image k_image;
      uint16_t seq;
      
      txin_vote_info() { }
      txin_vote_info(const txin_vote& inp) : k_image(inp.ink.k_image), seq(inp.seq) { }
    };
    
    inline bool operator==(const txin_invalid_info &a, const txin_invalid_info &b) {
      return true;
    }
    inline bool operator==(const txin_to_key_info &a, const txin_to_key_info &b) {
      return a.k_image == b.k_image;
    }
    inline bool operator==(const txin_mint_info &a, const txin_mint_info &b) {
      return a.currency == b.currency && a.description == b.description;
    }
    inline bool operator==(const txin_remint_info &a, const txin_remint_info &b) {
      return a.currency == b.currency && a.sig == b.sig;
    }
    inline bool operator==(const txin_no_conflict_info &a, const txin_no_conflict_info &b) {
      return false;
    }
    inline bool operator==(const txin_grade_contract_info &a, const txin_grade_contract_info &b) {
      return a.contract == b.contract && a.fee_amounts == b.fee_amounts;
    }
    inline bool operator==(const txin_register_delegate_info &a, const txin_register_delegate_info &b) {
      return a.delegate_id == b.delegate_id && a.addr == b.addr;
    }
    inline bool operator==(const txin_vote_info &a, const txin_vote_info &b) {
      return a.k_image == b.k_image && a.seq == b.seq;
    }
    
    inline size_t hash_value(const txin_invalid_info& a) {
      throw std::runtime_error("txin_invalid_info should never be hashed");
    }
    inline size_t hash_value(const txin_to_key_info& a) {
      return std::hash<crypto::key_image>()(a.k_image);
    }
    inline size_t hash_value(const txin_mint_info& a) {
      return boost::hash<std::pair<uint64_t, std::string> >()(std::make_pair(a.currency, a.description));
    }
    inline size_t hash_value(const txin_remint_info& a) {
      return boost::hash<std::pair<uint64_t, crypto::signature> >()(std::make_pair(a.currency, a.sig));
    }
    inline size_t hash_value(const txin_no_conflict_info& a) {
      return (size_t)&a;
    }
    inline size_t hash_value(const txin_grade_contract_info& a) {
      return boost::hash<std::pair<uint64_t, std::map<uint64_t, uint64_t> > >()(std::make_pair(a.contract, a.fee_amounts));
    }
    inline size_t hash_value(const txin_register_delegate_info& a) {
      return boost::hash<std::pair<delegate_id_t, account_public_address> >()(std::make_pair(a.delegate_id, a.addr));
    }
    inline size_t hash_value(const txin_vote_info& a) {
      return boost::hash<std::pair<crypto::key_image, uint16_t> >()(std::make_pair(a.k_image, a.seq));
    }
    
    typedef boost::variant<txin_invalid_info, txin_to_key_info, txin_mint_info, txin_remint_info,
                           txin_no_conflict_info, txin_grade_contract_info,
                           txin_register_delegate_info, txin_vote_info> txin_info;
    
    inline std::ostream &operator <<(std::ostream &o, const txin_info &v) {
      struct add_txin_info_descr_visitor: public boost::static_visitor<void>
      {
        std::ostream& o;
        add_txin_info_descr_visitor(std::ostream& o_in) : o(o_in) { }
        
        void operator()(const txin_invalid_info& inf) const { o << "<invalid>"; }
        void operator()(const txin_to_key_info& inf) const { o << "<key, " << inf.k_image << ">"; }
        void operator()(const txin_mint_info& inf) const { o << "<mint, " << inf.currency << ", " << inf.description << ">"; }
        void operator()(const txin_remint_info& inf) const { o << "<remint, " << inf.currency << ", " << inf.sig << ">"; }
        void operator()(const txin_no_conflict_info& inf) const { o << "<noconflict " << (&inf) << ">"; }
        void operator()(const txin_grade_contract_info& inf) const { o << "<grade, " << inf.contract << ">"; }
        void operator()(const txin_register_delegate_info& inf) const {
          o << "<delegate, " << inf.delegate_id << ", " << inf.addr << ">";
        }
        void operator()(const txin_vote_info& inf) const {
          o << "<vote, " << inf.k_image << ", " << inf.seq << ">";
        }
      };
      
      boost::apply_visitor(add_txin_info_descr_visitor(o), v);
      return o;
    }
  }
    
}

namespace cryptonote
{
  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  class blockchain_storage;

  class tx_memory_pool: boost::noncopyable
  {
  public:
    tx_memory_pool(blockchain_storage& bchs);
    bool add_tx(const transaction &tx, const crypto::hash &id, size_t blob_size, tx_verification_context& tvc, bool keeped_by_block);
    bool add_tx(const transaction &tx, tx_verification_context& tvc, bool keeped_by_block);
    //gets tx and remove it from pool
    bool take_tx(const crypto::hash &id, transaction &tx, size_t& blob_size, uint64_t& fee);

    bool have_tx(const crypto::hash &id) const;
    bool check_can_add_tx(const transaction& tx) const;

    bool on_blockchain_inc(uint64_t new_block_height, const crypto::hash& top_block_id);
    bool on_blockchain_dec(uint64_t new_block_height, const crypto::hash& top_block_id);

    void lock() const;
    void unlock() const;

    // load/store operations
    bool init(const std::string& config_folder);
    bool deinit();
    bool fill_block_template(block &bl, size_t median_size, uint64_t already_generated_coins,
                             size_t &total_size, uint64_t &fee);
    bool get_transactions(std::list<transaction>& txs) const;
    bool get_transaction(const crypto::hash& h, transaction& tx) const;
    size_t get_transactions_count() const;
    bool remove_transaction_data(const transaction& tx);
    bool have_key_images(const std::unordered_set<crypto::key_image>& kic, const transaction& tx);
    std::string print_pool(bool short_format);

#define CURRENT_MEMPOOL_ARCHIVE_VER    9
    
    template<class archive_t>
    void serialize(archive_t & a, const unsigned int version)
    {
      if (version < CURRENT_MEMPOOL_ARCHIVE_VER)
        return;
      CRITICAL_REGION_LOCAL(m_transactions_lock);
      a & m_transactions;
      a & m_txin_infos;
    }

    struct tx_details
    {
      transaction tx;
      size_t blob_size;
      uint64_t fee;
      crypto::hash max_used_block_id;
      uint64_t max_used_block_height;
      bool kept_by_block;
      //
      uint64_t last_failed_height;
      crypto::hash last_failed_id;
    };
    
    i_tx_pool_callback* callback() const { return m_callback; }
    void callback(i_tx_pool_callback* callback) { m_callback = callback; }
    
  private:
    bool add_inp(const crypto::hash& tx_id, const txin_v& inp, bool kept_by_block);
    bool remove_inp(const crypto::hash& tx_id, const txin_v& inp);
    bool check_can_add_inp(const txin_v& inp) const;
    
    bool is_transaction_ready_to_go(tx_details& txd) const;
    typedef std::unordered_map<crypto::hash, tx_details > transactions_container;
    typedef std::unordered_map<detail::txin_info, std::unordered_set<crypto::hash>,
                               boost::hash<detail::txin_info> > txin_info_container;
    
    mutable epee::critical_section m_transactions_lock;
    transactions_container m_transactions;
    txin_info_container m_txin_infos;

    //transactions_container m_alternative_transactions;

    std::string m_config_folder;
    blockchain_storage& m_blockchain;
    i_tx_pool_callback* m_callback;
    /************************************************************************/
    /*                                                                      */
    /************************************************************************/

#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
    friend class blockchain_storage;
#endif
  };
}

namespace boost
{
  namespace serialization
  {
    template<class archive_t>
    void serialize(archive_t & ar, cryptonote::tx_memory_pool::tx_details& td, const unsigned int version)
    {
      ar & td.blob_size;
      ar & td.fee;
      ar & td.tx;
      ar & td.max_used_block_height;
      ar & td.max_used_block_id;
      ar & td.last_failed_height;
      ar & td.last_failed_id;
    }

    template<class archive_t>
    void serialize(archive_t & ar, cryptonote::detail::txin_invalid_info& x, const unsigned int version)
    {
      throw std::runtime_error("txin_invalid_info should never be serialized");
    }
    
    template<class archive_t>
    void serialize(archive_t & ar, cryptonote::detail::txin_to_key_info& x, const unsigned int version)
    {
      ar & x.k_image;
    }
    
    template<class archive_t>
    void serialize(archive_t & ar, cryptonote::detail::txin_mint_info& x, const unsigned int version)
    {
      ar & x.currency;
      ar & x.description;
    }
    
    template<class archive_t>
    void serialize(archive_t & ar, cryptonote::detail::txin_remint_info& x, const unsigned int version)
    {
      ar & x.currency;
      ar & x.sig;
    }

    template<class archive_t>
    void serialize(archive_t & ar, cryptonote::detail::txin_no_conflict_info& x, const unsigned int version)
    {
    }

    template<class archive_t>
    void serialize(archive_t & ar, cryptonote::detail::txin_grade_contract_info& x, const unsigned int version)
    {
      ar & x.contract;
      ar & x.fee_amounts;
    }
    
    template<class archive_t>
    void serialize(archive_t & ar, cryptonote::detail::txin_register_delegate_info& x, const unsigned int version)
    {
      ar & x.delegate_id;
      ar & x.addr;
    }
    
    template<class archive_t>
    void serialize(archive_t & ar, cryptonote::detail::txin_vote_info& x, const unsigned int version)
    {
      ar & x.k_image;
      ar & x.seq;
    }
  }
}
BOOST_CLASS_VERSION(cryptonote::tx_memory_pool, CURRENT_MEMPOOL_ARCHIVE_VER)
