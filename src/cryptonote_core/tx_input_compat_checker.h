// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "crypto/crypto.h"

#include "cryptonote_basic.h"
#include "account_boost_serialization.h"
#include "visitors.h"

namespace cryptonote
{
  // check whether all inputs within a transaction are compatible with each other
  // (semantic check, e.g. don't spend same key image)
  // and also whether multiple transactions are compatible with each other
  // (same checks but across txs, needed for tx pool)
  
  class tx_input_compat_checker;
  
  namespace detail
  {
    struct check_can_add_inp_visitor : public tx_input_visitor_base
    {
      using tx_input_visitor_base::operator();
      
      const tx_input_compat_checker& m_icc;
      check_can_add_inp_visitor(const tx_input_compat_checker& icc);
      
      bool operator()(const txin_to_key& inp) const;
      bool operator()(const txin_mint& inp) const;
      bool operator()(const txin_remint& inp) const;
      bool operator()(const txin_create_contract& inp) const;
      bool operator()(const txin_mint_contract& inp) const;
      bool operator()(const txin_grade_contract& inp) const;
      bool operator()(const txin_resolve_bc_coins& inp) const;
      bool operator()(const txin_fuse_bc_coins& inp) const;
      bool operator()(const txin_register_delegate& inp) const;
      bool operator()(const txin_vote& inp) const;
    };
    
    struct add_inp_visitor : public tx_input_visitor_base
    {
      using tx_input_visitor_base::operator();
      
      tx_input_compat_checker& m_icc;
      add_inp_visitor(tx_input_compat_checker& icc);
      
      bool operator()(const txin_to_key& inp) const;
      bool operator()(const txin_mint& inp) const;
      bool operator()(const txin_remint& inp) const;
      bool operator()(const txin_create_contract& inp) const;
      bool operator()(const txin_mint_contract& inp) const;
      bool operator()(const txin_grade_contract& inp) const;
      bool operator()(const txin_resolve_bc_coins& inp) const;
      bool operator()(const txin_fuse_bc_coins& inp) const;
      bool operator()(const txin_register_delegate& inp) const;
      bool operator()(const txin_vote& inp) const;
    };
  
    struct remove_inp_visitor : public tx_input_visitor_base
    {
      using tx_input_visitor_base::operator();
      
      tx_input_compat_checker& m_icc;
      remove_inp_visitor(tx_input_compat_checker& icc);
      
      bool operator()(const txin_to_key& inp) const;
      bool operator()(const txin_mint& inp) const;
      bool operator()(const txin_remint& inp) const;
      bool operator()(const txin_create_contract& inp) const;
      bool operator()(const txin_mint_contract& inp) const;
      bool operator()(const txin_grade_contract& inp) const;
      bool operator()(const txin_resolve_bc_coins& inp) const;
      bool operator()(const txin_fuse_bc_coins& inp) const;
      bool operator()(const txin_register_delegate& inp) const;
      bool operator()(const txin_vote& inp) const;
    };
  }

  class tx_input_compat_checker
  {
    friend struct detail::check_can_add_inp_visitor;
    friend struct detail::add_inp_visitor;
    friend struct detail::remove_inp_visitor;
    
  public:
    tx_input_compat_checker();
    tx_input_compat_checker(const tx_input_compat_checker& other);
    
    static bool is_tx_valid(const transaction& tx);
    
    bool can_add_inp(const txin_v& inp) const;
    bool add_inp(const txin_v& inp);
    bool remove_inp(const txin_v& inp);
    
    bool can_add_tx(const transaction& tx) const;
    bool add_tx(const transaction& tx);
    bool remove_tx(const transaction& tx);
    
  private:
    std::unordered_set<crypto::key_image> k_images;
    std::unordered_set<uint64_t> minted_currencies;
    std::unordered_set<std::string> used_descriptions;
    std::unordered_set<uint64_t> reminted_currencies;
    std::unordered_set<uint64_t> graded_contracts;
    std::unordered_map<uint64_t, size_t> minted_contracts; // count # of minted contracts (can be multiple)
    std::unordered_map<uint64_t, size_t> fused_contracts; // count # of fused contracts (can be multiple)
    std::unordered_set<delegate_id_t> registered_delegate_ids;
    std::unordered_set<account_public_address> registered_addresses;
    
    detail::check_can_add_inp_visitor check_inp_visitor;
    detail::add_inp_visitor add_inp_visitor;
    detail::remove_inp_visitor remove_inp_visitor;
  };
}
