// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "include_base_utils.h"

#include "cryptonote_basic.h"

#include <boost/variant/static_visitor.hpp>

namespace cryptonote {
  template <typename t_retval=bool, t_retval t_default_return=false, bool t_print_error=true>
  struct tx_input_visitor_base_opt: public boost::static_visitor<t_retval>
  {
    size_t visitor_index;
    
    void set_index(size_t i)
    {
      visitor_index = i;
    }
    
    virtual t_retval operator()(const txin_to_key& inp) const
    {
      if (t_print_error) LOG_ERROR("tx_input_visitor_base: txin_to_key not supported");
      return t_default_return;
    }
    virtual t_retval operator()(const txin_gen& inp) const
    {
      if (t_print_error) LOG_ERROR("tx_input_visitor_base: txin_gen not supported");
      return t_default_return;
    }
    virtual t_retval operator()(const txin_to_script& inp) const
    {
      if (t_print_error) LOG_ERROR("tx_input_visitor_base: txin_to_script not supported");
      return t_default_return;
    }
    virtual t_retval operator()(const txin_to_scripthash& inp) const
    {
      if (t_print_error) LOG_ERROR("tx_input_visitor_base: txin_to_scripthash not supported");
      return t_default_return;
    }
    virtual t_retval operator()(const txin_mint& inp) const
    {
      if (t_print_error) LOG_ERROR("tx_input_visitor_base: txin_mint not supported");
      return t_default_return;
    }
    virtual t_retval operator()(const txin_remint& inp) const
    {
      if (t_print_error) LOG_ERROR("tx_input_visitor_base: txin_remint not supported");
      return t_default_return;
    }
    virtual t_retval operator()(const txin_create_contract& inp) const
    {
      if (t_print_error) LOG_ERROR("tx_input_visitor_base: txin_create_contract not supported");
      return t_default_return;
    }
    virtual t_retval operator()(const txin_grade_contract& inp) const
    {
      if (t_print_error) LOG_ERROR("tx_input_visitor_base: txin_grade_contract not supported");
      return t_default_return;
    }
    virtual t_retval operator()(const txin_mint_contract& inp) const
    {
      if (t_print_error) LOG_ERROR("tx_input_visitor_base: txin_mint_contract not supported");
      return t_default_return;
    }
    virtual t_retval operator()(const txin_resolve_bc_coins& inp) const
    {
      if (t_print_error) LOG_ERROR("tx_input_visitor_base: txin_resolve_bc_coins not supported");
      return t_default_return;
    }
    virtual t_retval operator()(const txin_fuse_bc_coins& inp) const
    {
      if (t_print_error) LOG_ERROR("tx_input_visitor_base: txin_fuse_bc_coins not supported");
      return t_default_return;
    }
    virtual t_retval operator()(const txin_register_delegate& inp) const
    {
      if (t_print_error) LOG_ERROR("tx_input_visitor_base: txin_register_delegate not supported");
      return t_default_return;
    }
    virtual t_retval operator()(const txin_vote& inp) const
    {
      if (t_print_error) LOG_ERROR("tx_input_visitor_base: txin_vote not supported");
      return t_default_return;
    }
  };
  
  template <typename t_retval=bool, t_retval t_default_return=false, bool t_print_error=true>
  struct tx_output_visitor_base_opt: public boost::static_visitor<t_retval>
  {
    size_t visitor_index;
    
    void set_index(size_t i)
    {
      visitor_index = i;
    }
    
    virtual t_retval operator()(const txout_to_key& inp) const
    {
      if (t_print_error) LOG_ERROR("tx_output_visitor_base: txout_to_key not supported");
      return t_default_return;
    }
    virtual t_retval operator()(const txout_to_script& inp) const
    {
      if (t_print_error) LOG_ERROR("tx_output_visitor_base: txout_to_script not supported");
      return t_default_return;
    }
    virtual t_retval operator()(const txout_to_scripthash& inp) const
    {
      if (t_print_error) LOG_ERROR("tx_output_visitor_base: txout_to_scripthash not supported");
      return t_default_return;
    }
  };
  
  typedef tx_input_visitor_base_opt<bool, false, true> tx_input_visitor_base;
  typedef tx_output_visitor_base_opt<bool, false, true> tx_output_visitor_base;
}
