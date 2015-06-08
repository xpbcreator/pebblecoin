// Copyright (c) 2014-2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "common/stl-util.h"

#include "include_base_utils.h"

#include "cryptonote_basic.h"
#include "cryptonote_basic_impl.h"
#include "visitors.h"

using tools::const_get;

namespace cryptonote {
  namespace {
    struct input_type_version_visitor: tx_input_visitor_base_opt<size_t, 666>
    {
      using tx_input_visitor_base_opt<size_t, 666>::operator();
      
      size_t operator()(const txin_gen& inp) const { return VANILLA_TRANSACTION_VERSION; }
      size_t operator()(const txin_to_key& inp) const { return VANILLA_TRANSACTION_VERSION; }
      size_t operator()(const txin_mint& inp) const { return CURRENCY_TRANSACTION_VERSION; }
      size_t operator()(const txin_remint& inp) const { return CURRENCY_TRANSACTION_VERSION; }
      size_t operator()(const txin_create_contract& inp) const { return CONTRACT_TRANSACTION_VERSION; }
      size_t operator()(const txin_mint_contract& inp) const { return CONTRACT_TRANSACTION_VERSION; }
      size_t operator()(const txin_grade_contract& inp) const { return CONTRACT_TRANSACTION_VERSION; }
      size_t operator()(const txin_resolve_bc_coins& inp) const { return CONTRACT_TRANSACTION_VERSION; }
      size_t operator()(const txin_fuse_bc_coins& inp) const { return CONTRACT_TRANSACTION_VERSION; }
      size_t operator()(const txin_register_delegate& inp) const { return DPOS_TRANSACTION_VERSION; }
      size_t operator()(const txin_vote& inp) const { return DPOS_TRANSACTION_VERSION; }
    };
    
    struct output_type_version_visitor: tx_output_visitor_base_opt<size_t, 666>
    {
      using tx_output_visitor_base_opt<size_t, 666>::operator();
      
      size_t operator()(const txout_to_key& inp) const { return VANILLA_TRANSACTION_VERSION; }
    };
  }

  size_t inp_minimum_tx_version(const txin_v& inp)
  {
    return boost::apply_visitor(input_type_version_visitor(), inp);
  }

  size_t outp_minimum_tx_version(const tx_out& outp)
  {
    return boost::apply_visitor(output_type_version_visitor(), outp.target);
  }

  bool transaction_prefix::has_valid_in_out_types() const
  {
    BOOST_FOREACH(const auto& inp, vin) {
      if (inp_minimum_tx_version(inp) > version)
      {
        LOG_ERROR("Invalid tx version " << version << " for input " << inp.which()
                  << " requires min version " << inp_minimum_tx_version(inp));
        return false;
      }
    }
    BOOST_FOREACH(const auto& outp, vout) {
      if (outp_minimum_tx_version(outp) > version)
      {
        LOG_ERROR("Invalid tx version " << version << " for output " << outp.target.which()
                  << " requires min version " << outp_minimum_tx_version(outp));
        return false;
      }
    }
    
    return true;
  }
  
  bool transaction_prefix::has_valid_coin_types() const
  {
    if (vin.size() != vin_coin_types.size() || vout.size() != vout_coin_types.size())
    {
      LOG_ERROR("vin.size()=" << vin.size() << ", vin_coin_types.size()=" << vin_coin_types.size() <<
                ", vout.size()=" << vout.size() << ", vout_coin_types.size()=" << vout_coin_types.size());
      return false;
    }
    
    BOOST_FOREACH(const auto& ct, vin_coin_types)
    {
      if (!ct.is_valid_tx_version(version))
      {
        LOG_ERROR("invalid version " << version << " for coin type " << ct);
        return false;
      }
    }
    BOOST_FOREACH(const auto& ct, vout_coin_types)
    {
      if (!ct.is_valid_tx_version(version))
      {
        LOG_ERROR("invalid version " << version << " for coin type " << ct);
        return false;
      }
    }
    
    return true;
  }

  void transaction_prefix::clear_ins()
  {
    vin.clear();
    vin_coin_types.clear();
  }
  
  void transaction_prefix::clear_outs()
  {
    vout.clear();
    vout_coin_types.clear();
  }

  void transaction_prefix::replace_vote_seqs(const std::map<crypto::key_image, uint64_t> &key_image_seqs)
  {
    for (auto& inp : vin)
    {
      if (inp.type() == typeid(txin_vote))
      {
        auto& inv = boost::get<txin_vote>(inp);
        uint64_t new_seq = const_get(key_image_seqs, inv.ink.k_image);
        if (inv.seq != new_seq)
        {
          LOG_PRINT_YELLOW("WARNING: Wallet generated wrong vote sequence number", LOG_LEVEL_0);
        }
        inv.seq = new_seq;
      }
    }
  }
  
  transaction::transaction()
  {
    set_null();
  }

  transaction::~transaction()
  {
    //set_null();
  }

  void transaction::set_null()
  {
    version = 0;
    unlock_time = 0;
    clear_ins();
    clear_outs();
    extra.clear();
    signatures.clear();
  }

  size_t transaction::get_signature_size(const txin_v& tx_in)
  {
    struct txin_signature_size_visitor : public boost::static_visitor<size_t>
    {
      size_t operator()(const txin_gen& txin) const{return 0;}
      size_t operator()(const txin_to_script& txin) const{return 0;}
      size_t operator()(const txin_to_scripthash& txin) const{return 0;}
      size_t operator()(const txin_to_key& txin) const {return txin.key_offsets.size();}
      size_t operator()(const txin_mint& txin) const{return 0;}
      size_t operator()(const txin_remint& txin) const{return 0;}
      size_t operator()(const txin_create_contract& txin) const{return 0;}
      size_t operator()(const txin_mint_contract& txin) const{return 0;}
      size_t operator()(const txin_grade_contract& txin) const{return 0;}
      size_t operator()(const txin_resolve_bc_coins& txin) const{return 0;}
      size_t operator()(const txin_fuse_bc_coins& txin) const{return 0;}
      size_t operator()(const txin_register_delegate& txin) const{return 0;}
      size_t operator()(const txin_vote& txin) const{return txin.ink.key_offsets.size();}
    };
    
    return boost::apply_visitor(txin_signature_size_visitor(), tx_in);
  }
} // namespace cryptonote
