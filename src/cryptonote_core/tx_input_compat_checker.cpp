// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypto/crypto_basic_impl.h"

#include "cryptonote_basic_impl.h"
#include "account.h"
#include "tx_input_compat_checker.h"

namespace cryptonote
{
  namespace detail
  {
    template <class map, class key>
    bool gt_0(const map& m, const key& k)
    {
      auto it = m.find(k);
      if (it == m.end())
        return false;
      
      return it->second > 0;
    }
    
    // --------------------------------------------------------------------------------
    check_can_add_inp_visitor::check_can_add_inp_visitor(const tx_input_compat_checker& icc) : m_icc(icc) { }
      
    bool check_can_add_inp_visitor::operator()(const txin_to_key& inp) const
    {
      CHECK_AND_ASSERT_MES(m_icc.k_images.count(inp.k_image) == 0, false, "Already spent key image");
      return true;
    }
    
    bool check_can_add_inp_visitor::operator()(const txin_mint& inp) const
    {
      CHECK_AND_ASSERT_MES(m_icc.minted_currencies.count(inp.currency) > 0, false, "Already minted currency/contract");
      
      CHECK_AND_ASSERT_MES(!inp.description.empty() && m_icc.used_descriptions.count(inp.description) > 0,
                           false, "Already used currency/contract description");
      return true;
    }
    
    bool check_can_add_inp_visitor::operator()(const txin_remint& inp) const
    {
      CHECK_AND_ASSERT_MES(m_icc.reminted_currencies.count(inp.currency) == 0, false, "Already reminted currency");
      return true;
    }
    
    bool check_can_add_inp_visitor::operator()(const txin_create_contract& inp) const
    {
      CHECK_AND_ASSERT_MES(m_icc.minted_currencies.count(inp.contract) > 0, false, "Already minted currency/contract");
      
      CHECK_AND_ASSERT_MES(!inp.description.empty() && m_icc.used_descriptions.count(inp.description) > 0,
                           false, "Already used currency/contract description");
      return true;
    }
    
    bool check_can_add_inp_visitor::operator()(const txin_mint_contract& inp) const
    {
      // can mint twice; don't mint if graded
      CHECK_AND_ASSERT_MES(m_icc.graded_contracts.count(inp.contract) == 0, false, "Graded contract, can't mint");
      return true;
    }
    
    bool check_can_add_inp_visitor::operator()(const txin_grade_contract& inp) const
    {
      // don't grade if graded
      CHECK_AND_ASSERT_MES(m_icc.graded_contracts.count(inp.contract) > 0, false, "Already graded contract");
      
      // don't grade if minted or fused
      CHECK_AND_ASSERT_MES(gt_0(m_icc.minted_contracts, inp.contract), false, "Minted contract, can't grade");
      CHECK_AND_ASSERT_MES(gt_0(m_icc.fused_contracts, inp.contract), false, "Fused contract, can't grade");
      
      return true;
    }
    
    bool check_can_add_inp_visitor::operator()(const txin_resolve_bc_coins& inp) const
    {
      return true;
    }
    
    bool check_can_add_inp_visitor::operator()(const txin_fuse_bc_coins& inp) const
    {
      // can fuse twice; don't fuse if graded
      CHECK_AND_ASSERT_MES(m_icc.graded_contracts.count(inp.contract) == 0, false, "Graded contract, can't fuse");
      return true;
    }

    bool check_can_add_inp_visitor::operator()(const txin_register_delegate& inp) const
    {
      CHECK_AND_ASSERT_MES(m_icc.registered_delegate_ids.count(inp.delegate_id) == 0,
                           false, "already registering delegate " << inp.delegate_id);
      CHECK_AND_ASSERT_MES(m_icc.registered_addresses.count(inp.delegate_address) == 0,
                           false, "already registering delegate " << inp.delegate_address);
      return true;
    }

    bool check_can_add_inp_visitor::operator()(const txin_vote& inp) const
    {
      CHECK_AND_ASSERT_MES((*this)(inp.ink), false, "Already voted/spent key image");
      return true;
    }

    // --------------------------------------------------------------------------------
    add_inp_visitor::add_inp_visitor(tx_input_compat_checker& icc) : m_icc(icc) { }
      
    bool add_inp_visitor::operator()(const txin_to_key& inp) const
    {
      auto i_res = m_icc.k_images.insert(inp.k_image);
      CHECK_AND_ASSERT_MES(i_res.second, false, "internal error: inserted duplicate image in key image set: "
                           << inp.k_image);
      return true;
    }
    
    bool add_inp_visitor::operator()(const txin_mint& inp) const
    {
      auto i_res1 = m_icc.minted_currencies.insert(inp.currency);
      CHECK_AND_ASSERT_MES(i_res1.second, false, "internal error: inserted duplicate currency in mint set: "
                           << inp.currency);
      if (!inp.description.empty())
      {
        auto i_res2 = m_icc.used_descriptions.insert(inp.description);
        CHECK_AND_ASSERT_MES(i_res2.second, false, "internal error: inserted duplicate description in set: "
                             << inp.description);
      }
      return true;
    }
    
    bool add_inp_visitor::operator()(const txin_remint& inp) const
    {
      auto i_res = m_icc.reminted_currencies.insert(inp.currency);
      CHECK_AND_ASSERT_MES(i_res.second, false, "internal error: inserted duplicate currency in remint set: "
                           << inp.currency);
      return true;
    }
    
    bool add_inp_visitor::operator()(const txin_create_contract& inp) const
    {
      auto i_res1 = m_icc.minted_currencies.insert(inp.contract);
      CHECK_AND_ASSERT_MES(i_res1.second, false, "internal error: inserted duplicate contract in create set: "
                           << inp.contract);
      if (!inp.description.empty())
      {
        auto i_res2 = m_icc.used_descriptions.insert(inp.description);
        CHECK_AND_ASSERT_MES(i_res2.second, false, "internal error: inserted duplicate description in set: "
                             << inp.description);
      }
      return true;
    }
    
    bool add_inp_visitor::operator()(const txin_mint_contract& inp) const
    {
      m_icc.minted_contracts[inp.contract] += 1;
      return true;
    }
    
    bool add_inp_visitor::operator()(const txin_grade_contract& inp) const
    {
      auto i_res = m_icc.graded_contracts.insert(inp.contract);
      CHECK_AND_ASSERT_MES(i_res.second, false, "internal error: inserted duplicate contract in grade set: "
                           << inp.contract);
      return true;
    }
    
    bool add_inp_visitor::operator()(const txin_resolve_bc_coins& inp) const
    {
      return true;
    }
    
    bool add_inp_visitor::operator()(const txin_fuse_bc_coins& inp) const
    {
      m_icc.fused_contracts[inp.contract] += 1;
      return true;
    }
    
    bool add_inp_visitor::operator()(const txin_register_delegate& inp) const
    {
      auto res1 = m_icc.registered_delegate_ids.insert(inp.delegate_id);
      auto res2 = m_icc.registered_addresses.insert(inp.delegate_address);
      CHECK_AND_ASSERT_MES(res1.second && res2.second, false, "internal error: inserted duplicate delegate id/address");
      return true;
    }
    
    bool add_inp_visitor::operator()(const txin_vote& inp) const
    {
      return (*this)(inp.ink);
    }
    
    // --------------------------------------------------------------------------------
    remove_inp_visitor::remove_inp_visitor(tx_input_compat_checker& icc) : m_icc(icc) { }
      
    bool remove_inp_visitor::operator()(const txin_to_key& inp) const
    {
      CHECK_AND_ASSERT_MES(m_icc.k_images.erase(inp.k_image) == 1,
                           false, "internal error: did not remove k_image");
      return true;
    }
    
    bool remove_inp_visitor::operator()(const txin_mint& inp) const
    {
      CHECK_AND_ASSERT_MES(m_icc.minted_currencies.erase(inp.currency) == 1,
                           false, "internal error: did not remove currency");
      
      if (!inp.description.empty())
      {
        CHECK_AND_ASSERT_MES(m_icc.used_descriptions.erase(inp.description) == 1,
                             false, "internal error: did not remove description");
      }
      return true;
    }
    
    bool remove_inp_visitor::operator()(const txin_remint& inp) const
    {
      CHECK_AND_ASSERT_MES(m_icc.reminted_currencies.erase(inp.currency) == 1,
                           false, "internal error: did not remove currency");
      return true;
    }
    
    bool remove_inp_visitor::operator()(const txin_create_contract& inp) const
    {
      CHECK_AND_ASSERT_MES(m_icc.minted_currencies.erase(inp.contract) == 1,
                           false, "internal error: did not remove contract");
      
      if (!inp.description.empty())
      {
        CHECK_AND_ASSERT_MES(m_icc.used_descriptions.erase(inp.description) == 1,
                             false, "internal error: did not remove description");
      }
      return true;
    }
    
    bool remove_inp_visitor::operator()(const txin_mint_contract& inp) const
    {
      CHECK_AND_ASSERT_MES(m_icc.minted_contracts[inp.contract] > 0, false, "internal error: no minted contract to remove");
      m_icc.minted_contracts[inp.contract] -= 1;
      return true;
    }
    
    bool remove_inp_visitor::operator()(const txin_grade_contract& inp) const
    {
      CHECK_AND_ASSERT_MES(m_icc.graded_contracts.erase(inp.contract) == 1,
                           false, "internal error: did not remove contract from grade set");
      return true;
    }
    
    bool remove_inp_visitor::operator()(const txin_resolve_bc_coins& inp) const
    {
      return true;
    }
    
    bool remove_inp_visitor::operator()(const txin_fuse_bc_coins& inp) const
    {
      CHECK_AND_ASSERT_MES(m_icc.fused_contracts[inp.contract] > 0, false, "internal error: no fused contract to remove");
      m_icc.fused_contracts[inp.contract] -= 1;
      return true;
    }
    
    bool remove_inp_visitor::operator()(const txin_register_delegate& inp) const
    {
      auto res1 = m_icc.registered_delegate_ids.erase(inp.delegate_id);
      auto res2 = m_icc.registered_addresses.erase(inp.delegate_address);
      
      CHECK_AND_ASSERT_MES(res1 == 1 && res2 == 1, false, "internal error: did not remove delegate id or address");
      return true;
    }
    
    bool remove_inp_visitor::operator()(const txin_vote& inp) const
    {
      return (*this)(inp.ink);
    }
  }
  
  
  tx_input_compat_checker::tx_input_compat_checker()
      : check_inp_visitor(*this)
      , add_inp_visitor(*this)
      , remove_inp_visitor(*this) { }

  tx_input_compat_checker::tx_input_compat_checker(const tx_input_compat_checker& other)
      : check_inp_visitor(*this)
      , add_inp_visitor(*this)
      , remove_inp_visitor(*this)
  {
    k_images = other.k_images;
    minted_currencies = other.minted_currencies;
    used_descriptions = other.used_descriptions;
    reminted_currencies = other.reminted_currencies;
    graded_contracts = other.graded_contracts;
    minted_contracts = other.minted_contracts;
    fused_contracts  = other.fused_contracts;
    registered_delegate_ids = other.registered_delegate_ids;
    registered_addresses = other.registered_addresses;
  }
  
  
  bool tx_input_compat_checker::can_add_inp(const txin_v &in_v) const
  {
    return boost::apply_visitor(check_inp_visitor, in_v);
  }

  bool tx_input_compat_checker::add_inp(const txin_v &in_v)
  {
    return boost::apply_visitor(add_inp_visitor, in_v);
  }

  bool tx_input_compat_checker::remove_inp(const txin_v &in_v)
  {
    return boost::apply_visitor(remove_inp_visitor, in_v);
  }
  

  bool tx_input_compat_checker::add_tx(const transaction &tx)
  {
    BOOST_FOREACH(const auto& in_v, tx.ins())
    {
      CHECK_AND_ASSERT_MES(can_add_inp(in_v), false, "Detected invalid input");
      CHECK_AND_ASSERT_MES(add_inp(in_v), false, "Failed to add an apparently valid input");
    }
    
    return true;
  }

  bool tx_input_compat_checker::can_add_tx(const transaction &tx) const
  {
    // Lazy approach: Copy the state and see if everything can be added successfully
    tx_input_compat_checker other(*this);
    
    return other.add_tx(tx);
  }

  bool tx_input_compat_checker::remove_tx(const transaction &tx)
  {
    bool failed = false;
    BOOST_FOREACH(const auto& in_v, tx.ins())
    {
      if (!remove_inp(in_v))
      {
        LOG_ERROR("Failed to remove inp from icc");
        failed = true;
      }
    }
    
    return failed;
  }
  

  bool tx_input_compat_checker::is_tx_valid(const transaction& tx)
  {
    tx_input_compat_checker icc;
    
    return icc.can_add_tx(tx);
  }
}
