// Copyright (c) 2014-2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <vector>
#include <algorithm>

#include "string_tools.h"

#include "cryptonote_config.h"
#include "crypto/hash.h"
#include "crypto/crypto_basic_impl.h"

#include "cryptonote_core/nulls.h"
#include "cryptonote_core/contract_grading.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/cryptonote_basic_impl.h"
#include "cryptonote_core/visitors.h"
#include "tx_builder.h"

namespace cryptonote
{
  namespace
  {
    std::vector<tx_destination_entry> shuffle_outs(const std::vector<tx_destination_entry>& destinations)
    {
      std::vector<tx_destination_entry> shuffled_dsts(destinations);
      std::sort(shuffled_dsts.begin(), shuffled_dsts.end(), [](const tx_destination_entry& de1, const tx_destination_entry& de2) { return de1.amount < de2.amount; } );
      return shuffled_dsts;
    }
    
    bool dst_to_out(const tx_destination_entry& dst_entr, size_t output_index, const keypair& txkey, tx_out& out)
    {
      CHECK_AND_ASSERT_MES(dst_entr.amount > 0, false, "Destination with wrong amount: " << dst_entr.amount);
      crypto::key_derivation derivation;
      crypto::public_key out_eph_public_key;
      bool r = crypto::generate_key_derivation(dst_entr.addr.m_view_public_key, txkey.sec, derivation);
      CHECK_AND_ASSERT_MES(r, false, "at creation outs: failed to generate_key_derivation(" << dst_entr.addr.m_view_public_key << ", " << txkey.sec << ")");
      
      r = crypto::derive_public_key(derivation, output_index, dst_entr.addr.m_spend_public_key, out_eph_public_key);
      CHECK_AND_ASSERT_MES(r, false, "at creation outs: failed to derive_public_key(" << derivation << ", " << output_index << ", "<< dst_entr.addr.m_spend_public_key << ")");
      
      out.amount = dst_entr.amount;
      txout_to_key tk;
      tk.key = out_eph_public_key;
      out.target = tk;
      return true;
    }
    
    struct get_k_image_visitor: tx_input_visitor_base
    {
      using tx_input_visitor_base::operator();
      
      crypto::key_image &dst;
      get_k_image_visitor(crypto::key_image& dst_in) : dst(dst_in) { }
      
      bool operator()(const txin_to_key& inp) const { dst = inp.k_image; return true; }
      bool operator()(const txin_vote& inp) const { dst = inp.ink.k_image; return true; }
    };
    bool get_txin_k_image(const txin_v& inv, crypto::key_image& k_image)
    {
      return boost::apply_visitor(get_k_image_visitor(k_image), inv);
    }
    crypto::key_image get_txin_k_image(const txin_v& inv)
    {
      crypto::key_image k_image;
      if (!get_txin_k_image(inv, k_image)) {
        throw std::runtime_error("Inp had no k_image");
      }
      return k_image;
    }
  }

  tx_builder::tx_builder() : m_state(Uninitialized), m_txkey(null_keypair), m_ignore_checks(false) { }
  
  bool tx_builder::init(uint64_t unlock_time, std::vector<uint8_t> extra, bool ignore_checks)
  {
    CHECK_AND_ASSERT_MES(m_state == Uninitialized, false, "Cannot init state, tx is not Uninitialized");
    
    m_ignore_checks = ignore_checks;
    
    m_tx.set_null();
    
    m_tx.version = VANILLA_TRANSACTION_VERSION;
    m_tx.unlock_time = unlock_time;
    
    m_tx.extra = extra;
    m_txkey = keypair::generate();
    
    CHECK_AND_ASSERT_MES(add_tx_pub_key_to_extra(m_tx, m_txkey.pub), false, "failed to add_tx_pub_key_to_extra");
    
    m_state = InProgress;
    
    return true;
  }
  bool tx_builder::init(uint64_t unlock_time)
  {
    return init(0, std::vector<uint8_t>());
  }
  bool tx_builder::init()
  {
    return init(0);
  }
  
  void tx_builder::update_version_to(size_t min_version)
  {
    if (m_tx.version < min_version) m_tx.version = min_version;
  }
  
  void tx_builder::update_version_to(const txin_v& inp, const coin_type& cp)
  {
    update_version_to(cp.minimum_tx_version());
    update_version_to(inp_minimum_tx_version(inp));
  }
  
  void tx_builder::update_version_to(const tx_out& outp, const coin_type& cp)
  {
    update_version_to(cp.minimum_tx_version());
    update_version_to(outp_minimum_tx_version(outp));
  }
  
  void tx_builder::add_in(const txin_v& inp, const coin_type& cp)
  {
    update_version_to(inp, cp);
    m_tx.add_in(inp, cp);
  }
  
  void tx_builder::add_out(const tx_out& outp, const coin_type& cp)
  {
    update_version_to(outp, cp);
    m_tx.add_out(outp, cp);
  }
  
  bool tx_builder::add_sources(const account_keys& sender_account_keys, const std::vector<tx_source_entry>& sources,
                               currency_map& input_amounts,
                               bool vote, uint16_t vote_seq, const delegate_votes& votes)
  {
    //fill inputs
    BOOST_FOREACH(const tx_source_entry& src_entr, sources)
    {
      if (src_entr.real_output >= src_entr.outputs.size())
      {
        LOG_ERROR("real_output index (" << src_entr.real_output << ") bigger than output_keys.size()="
                  << src_entr.outputs.size());
        return false;
      }

      if (m_ignore_checks)
        input_amounts[src_entr.cp] += src_entr.amount_out;
      else
        CHECK_AND_ASSERT(add_amount(input_amounts[src_entr.cp], src_entr.amount_out), false);
      
      m_in_contexts.push_back(input_generation_context_data());
      keypair& in_ephemeral = m_in_contexts.back().in_ephemeral;
      crypto::key_image img;
      if (!generate_key_image_helper(sender_account_keys, src_entr.real_out_tx_key, src_entr.real_output_in_tx_index,
                                     in_ephemeral, img))
      {
        LOG_ERROR("Could not generate key image");
        return false;
      }
      
      //check that derivated key is equal with real output key
      if (!(in_ephemeral.pub == src_entr.outputs[src_entr.real_output].second))
      {
        LOG_ERROR("derived public key missmatch with output public key! "<< ENDL << "derived_key:"
                  << epee::string_tools::pod_to_hex(in_ephemeral.pub) << ENDL << "real output_public_key:"
                  << epee::string_tools::pod_to_hex(src_entr.outputs[src_entr.real_output].second) );
        return false;
      }
      
      //put key image into tx input
      txin_to_key input_to_key;
      input_to_key.amount = src_entr.amount_in;
      input_to_key.k_image = img;
      
      //fill outputs array and use relative offsets
      BOOST_FOREACH(const tx_source_entry::output_entry& out_entry, src_entr.outputs)
      {
        input_to_key.key_offsets.push_back(out_entry.first);
      }
      
      input_to_key.key_offsets = absolute_output_offsets_to_relative(input_to_key.key_offsets);
      
      // add the in spending the cp, or the backing/contract coins
      coin_type in_cp;
      switch (src_entr.type) {
        case tx_source_entry::InToKey:
          in_cp = src_entr.cp;
          break;
          
        case tx_source_entry::ResolveBacking:
          in_cp = coin_type(src_entr.contract_resolving, BackingCoin, src_entr.cp.currency);
          break;
          
        case tx_source_entry::ResolveContract:
          in_cp = coin_type(src_entr.contract_resolving, ContractCoin, src_entr.cp.currency);
          break;
          
        default:
          LOG_ERROR("Unknown src_entr.type: " << src_entr.type);
          return false;
      }
      
      if (!m_ignore_checks) {
        if (vote && in_cp != CP_XPB) {
          LOG_ERROR("Can only vote with XPBs");
          return false;
        }
      }
      
      if (vote) {
        txin_vote input_vote;
        
        input_vote.ink = input_to_key;
        input_vote.seq = vote_seq;
        input_vote.votes = votes;
        this->add_in(input_vote, in_cp);
      }
      else {
        this->add_in(input_to_key, in_cp);
      }
      
      m_sources_used.push_back(src_entr);
      m_source_to_vin_index[m_sources_used.size() - 1] = m_tx.ins().size() - 1;
      
      // add the in converting the backing/contract to the currency
      if (src_entr.type == tx_source_entry::ResolveBacking ||
          src_entr.type == tx_source_entry::ResolveContract)
      {
        txin_resolve_bc_coins resolve;
        
        resolve.contract = src_entr.contract_resolving;
        resolve.is_backing_coins = src_entr.type == tx_source_entry::ResolveBacking;
        resolve.backing_currency = src_entr.cp.currency;
        resolve.source_amount = src_entr.amount_in;
        resolve.graded_amount = src_entr.amount_out;
        
        this->add_in(resolve, coin_type(src_entr.cp.currency, NotContract, BACKED_BY_N_A));
      }
    }
    
    return true;
  }
  
  bool tx_builder::add_dests_unchecked(const std::vector<tx_destination_entry>& destinations,
                                       currency_map& output_amounts)
  {
    auto shuffled_dsts = shuffle_outs(destinations);
    
    BOOST_FOREACH(const tx_destination_entry& dst_entr, shuffled_dsts)
    {
      tx_out out;
      CHECK_AND_ASSERT(dst_to_out(dst_entr, m_tx.outs().size(), m_txkey, out), false);
      this->add_out(out, dst_entr.cp);
      output_amounts[dst_entr.cp] += dst_entr.amount;
    }
    
    return true;
  }
  
  bool tx_builder::add_dests_unchecked(const std::vector<tx_destination_entry>& destinations)
  {
    currency_map output_amounts;
    return add_dests_unchecked(destinations, output_amounts);
  }

  bool tx_builder::check_inputs_outputs(currency_map& input_amounts, currency_map& output_amounts)
  {
    BOOST_FOREACH(const auto& item, output_amounts)
    {
      CHECK_AND_ASSERT_MES(output_amounts[item.first] <= input_amounts[item.first], false,
                           "Transaction inputs money (" << item.first << ": " << input_amounts[item.first]
                           << ") less than outputs money (" << output_amounts[item.first] << ")");
    }
    
    return true;
  }
  
  bool tx_builder::add_dests(currency_map& input_amounts, const std::vector<tx_destination_entry>& destinations)
  {
    currency_map output_amounts;
    CHECK_AND_ASSERT(add_dests_unchecked(destinations, output_amounts), false);
    return check_inputs_outputs(input_amounts, output_amounts);
  }
  
  bool tx_builder::add_send(const account_keys& sender_account_keys,
                            const std::vector<tx_source_entry>& sources,
                            const std::vector<tx_destination_entry>& destinations)
  {
    CHECK_AND_ASSERT_MES(m_state == InProgress, false, "Cannot add send ins/outs, tx is not InProgress");
    
    // state will remain broken unless everything completes successfully
    m_state = Broken;
    
    currency_map input_amounts;
    CHECK_AND_ASSERT(add_sources(sender_account_keys, sources, input_amounts), false);
    CHECK_AND_ASSERT(add_dests(input_amounts, destinations), false);

    m_state = InProgress;
    
    return true;
  }
  
  bool tx_builder::add_mint(uint64_t currency, const std::string& description, uint64_t amount, uint64_t decimals,
                            const crypto::public_key& remint_key,
                            const std::vector<tx_destination_entry>& destinations)
  {
    CHECK_AND_ASSERT_MES(m_state == InProgress, false, "Cannot add mint, tx is not InProgress");

    m_state = Broken;
    
    CHECK_AND_ASSERT_MES(description.empty() || description.size() <= CURRENCY_DESCRIPTION_MAX_SIZE, false,
                         "Cannot add mint, description is too large");
    
    txin_mint txin;
    txin.currency = currency;
    txin.description = description;
    txin.decimals = decimals;
    txin.amount = amount;
    txin.remint_key = remint_key;
    
    this->add_in(txin, coin_type(currency, NotContract, BACKED_BY_N_A));
    
    currency_map input_amounts;
    input_amounts[m_tx.in_cp(m_tx.ins().size() - 1)] = txin.amount;
    CHECK_AND_ASSERT(add_dests(input_amounts, destinations), false);
    
    m_state = InProgress;
    
    return true;
  }

  bool tx_builder::add_remint(uint64_t currency, uint64_t amount,
                              const crypto::secret_key& remint_skey,
                              const crypto::public_key& new_remint_key,
                              const std::vector<tx_destination_entry>& destinations)
  {
    CHECK_AND_ASSERT_MES(m_state == InProgress, false, "Cannot add remint, tx is not InProgress");

    m_state = Broken;
    
    txin_remint txin;
    txin.currency = currency;
    txin.amount = amount;
    txin.new_remint_key = new_remint_key;
    
    crypto::hash prefix_hash = txin.get_prefix_hash();
    crypto::public_key remint_pkey;
    CHECK_AND_ASSERT_MES(secret_key_to_public_key(remint_skey, remint_pkey), false,
                         "Couldn't derive remint public key from remint secret key");
    generate_signature(prefix_hash, remint_pkey, remint_skey, txin.sig);
    
    this->add_in(txin, coin_type(currency, NotContract, BACKED_BY_N_A));
    
    currency_map input_amounts;
    input_amounts[m_tx.in_cp(m_tx.ins().size() - 1)] = txin.amount;
    CHECK_AND_ASSERT(add_dests(input_amounts, destinations), false);
    
    m_state = InProgress;
    
    return true;
  }
  
  bool tx_builder::add_contract(uint64_t contract, const std::string& description,
                                const crypto::public_key& grading_key,
                                uint32_t fee_scale, uint64_t expiry_block, uint32_t default_grade)
  {
    CHECK_AND_ASSERT_MES(m_state == InProgress, false, "Cannot add contract, tx is not InProgress");
    
    m_state = Broken;
    
    if (!m_ignore_checks)
    {
      CHECK_AND_ASSERT_MES(description.empty() || description.size() <= CONTRACT_DESCRIPTION_MAX_SIZE, false,
                           "Cannot add contract, description is too large");
      CHECK_AND_ASSERT_MES(grading_key != null_pkey, false, "Cannot add contract, grading key can't be null");
      CHECK_AND_ASSERT_MES(crypto::check_key(grading_key), false, "Cannot add contract, grading key is invalid");
      CHECK_AND_ASSERT_MES(expiry_block > 0, false, "Cannot add contract, expiry block must be provided > 0");
      CHECK_AND_ASSERT_MES(expiry_block < CRYPTONOTE_MAX_BLOCK_NUMBER, false,
                           "Expiry block must be < CRYPTONOTE_MAX_BLOCK_NUMBER");
      CHECK_AND_ASSERT_MES(fee_scale <= GRADE_SCALE_MAX, false, "Cannot add contract, fee_scale must be <= GRADE_SCALE_MAX");
      CHECK_AND_ASSERT_MES(default_grade <= GRADE_SCALE_MAX, false,
                           "Cannot add contract, default_grade must be <= GRADE_SCALE_MAX");
    }
    
    txin_create_contract txin;
    txin.contract = contract;
    txin.description = description;
    txin.grading_key = grading_key;
    txin.fee_scale = fee_scale;
    txin.expiry_block = expiry_block;
    txin.default_grade = default_grade;
    
    this->add_in(txin, CP_N_A);
    
    // No destinations are added
    
    m_state = InProgress;
    
    return true;
  }
  
  bool tx_builder::add_grade_contract(uint64_t contract, uint32_t grade,
                                      const crypto::secret_key& grading_secret_key,
                                      const std::vector<tx_destination_entry>& grading_fee_destinations)
  {
    CHECK_AND_ASSERT_MES(m_state == InProgress, false, "Cannot add grade contract, tx is not InProgress");
    
    m_state = Broken;
    
    if (!m_ignore_checks)
    {
      CHECK_AND_ASSERT_MES(grade <= GRADE_SCALE_MAX, false, "Cannot add grade contract, grade is not <= GRADE_SCALE_MAX");
    }

    txin_grade_contract txin;
    txin.contract = contract;
    txin.grade = grade;
    
    // need entire blockchain to verify the proper fees are being sent, so don't do it here
    currency_map output_amounts;
    CHECK_AND_ASSERT(add_dests_unchecked(grading_fee_destinations, output_amounts), false);
    
    std::map<uint64_t, uint64_t> fee_amounts;
    BOOST_FOREACH(const auto& item, output_amounts) {
      if (!m_ignore_checks)
      {
        CHECK_AND_ASSERT_MES(item.first.backed_by_currency == BACKED_BY_N_A, false, "A fee for grading can't be a contract");
      }
      fee_amounts[item.first.currency] = item.second;
    }
    
    txin.fee_amounts = fee_amounts;
    
    crypto::hash prefix_hash = txin.get_prefix_hash();
    crypto::public_key grading_pub_key;
    CHECK_AND_ASSERT_MES(secret_key_to_public_key(grading_secret_key, grading_pub_key), false,
                         "Couldn't derive grading public key from grading secret key");
    generate_signature(prefix_hash, grading_pub_key, grading_secret_key, txin.sig);
    
    this->add_in(txin, CP_N_A);
    
    m_state = InProgress;
    
    return true;
  }
  
  bool tx_builder::add_mint_contract(const account_keys& sender_account_keys,
                                     uint64_t for_contract, uint64_t amount_to_mint,
                                     const std::vector<tx_source_entry>& backing_sources,
                                     const std::vector<tx_destination_entry>& all_destinations)
  {
    CHECK_AND_ASSERT_MES(m_state == InProgress, false, "Cannot add contract, tx is not InProgress");
    CHECK_AND_ASSERT_MES(backing_sources.size() > 0 && all_destinations.size() > 0 && amount_to_mint > 0,
                         false, "Given no sources or no destinations or 0 to mint");

    m_state = Broken;
    
    if (!m_ignore_checks)
    {
      BOOST_FOREACH(const auto& src, backing_sources)
      {
        CHECK_AND_ASSERT_MES(src.cp == backing_sources[0].cp, false, "All backing sources must have the same cp");
        CHECK_AND_ASSERT_MES(src.cp.contract_type == NotContract &&
                             src.cp.backed_by_currency == BACKED_BY_N_A, false,
                             "All backing sources must be NotContracts");
      }
    }
    
    // WARNING: changing the order things are added will cause the unit tests to fail
    
    currency_map input_amounts, output_amounts;
    CHECK_AND_ASSERT(add_sources(sender_account_keys, backing_sources, input_amounts), false);
    
    if (!m_ignore_checks)
    {
      CHECK_AND_ASSERT_MES(input_amounts[backing_sources[0].cp] >= amount_to_mint, false,
                           "Want to mint more than given as input");
    }
    
    txin_mint_contract txin;
    txin.contract = for_contract;
    txin.backed_by_currency = backing_sources[0].cp.currency;
    txin.amount = amount_to_mint;
    this->add_in(txin, CP_N_A);
    
    // update the amounts
    if (!m_ignore_checks)
    {
      CHECK_AND_ASSERT_MES(sub_amount(input_amounts[backing_sources[0].cp], amount_to_mint), false,
                           "Failed to subtract backing currency amount in add_mint_contract");
      CHECK_AND_ASSERT_MES(add_amount(input_amounts[coin_type(for_contract, BackingCoin, txin.backed_by_currency)],
                                      amount_to_mint),
                           false, "Failed to add BackingCoin amount in add_mint_contract");
      CHECK_AND_ASSERT_MES(add_amount(input_amounts[coin_type(for_contract, ContractCoin, txin.backed_by_currency)],
                                      amount_to_mint),
                           false, "Failed to add ContractCoin amount in add_mint_contract");
    }
    
    // add the destinations, should include backing coins & contract coins & whatever change
    CHECK_AND_ASSERT(add_dests_unchecked(all_destinations, output_amounts), false);
    
    if (!m_ignore_checks)
    {
      // check amounts are valid
      CHECK_AND_ASSERT(check_inputs_outputs(input_amounts, output_amounts), false);
    }
    
    m_state = InProgress;
    
    return true;
  }
  
  bool tx_builder::add_fuse_contract(uint64_t contract, uint64_t backing_currency, uint64_t amount,
                                     const account_keys& backing_account_keys,
                                     const std::vector<tx_source_entry> backing_sources,
                                     const account_keys& contract_account_keys,
                                     const std::vector<tx_source_entry> contract_sources,
                                     const std::vector<tx_destination_entry>& all_destinations)
  {
    CHECK_AND_ASSERT_MES(m_state == InProgress, false, "Cannot add contract, tx is not InProgress");
    CHECK_AND_ASSERT_MES(backing_sources.size() > 0 && contract_sources.size() > 0
                         && all_destinations.size() > 0 && amount > 0,
                         false, "Given no sources or no destinations or 0 to mint");
    
    m_state = Broken;
    
    if (!m_ignore_checks)
    {
      BOOST_FOREACH(const auto& src, backing_sources)
      {
        CHECK_AND_ASSERT_MES(src.cp.currency == backing_sources[0].cp.currency, false,
                             "All backing sources must have the same contract");
        CHECK_AND_ASSERT_MES(src.cp.backed_by_currency == backing_sources[0].cp.backed_by_currency, false,
                             "All backing sources must be backed by the same currency");
        CHECK_AND_ASSERT_MES(src.cp.contract_type == BackingCoin,
                             false, "All backing sources must be backing coins");
      }
      BOOST_FOREACH(const auto& src, contract_sources)
      {
        CHECK_AND_ASSERT_MES(src.cp.currency == backing_sources[0].cp.currency, false,
                             "All contract sources must have the same contract");
        CHECK_AND_ASSERT_MES(src.cp.backed_by_currency == backing_sources[0].cp.backed_by_currency, false,
                             "All contract sources must be backed by the same currency");
        CHECK_AND_ASSERT_MES(src.cp.contract_type == ContractCoin,
                             false, "All contract sources must be contract coins");
      }
    }
    
    currency_map input_amounts, output_amounts;
    CHECK_AND_ASSERT(add_sources(backing_account_keys, backing_sources, input_amounts), false);
    CHECK_AND_ASSERT(add_sources(contract_account_keys, contract_sources, input_amounts), false);
    
    if (!m_ignore_checks)
    {
      CHECK_AND_ASSERT_MES(input_amounts[coin_type(contract, BackingCoin, backing_currency)] >= amount, false,
                           "Want to fuse more than # of backing coins given as input");
      CHECK_AND_ASSERT_MES(input_amounts[coin_type(contract, ContractCoin, backing_currency)] >= amount, false,
                           "Want to fuse more than # of contract coins given as input");
    }
    
    txin_fuse_bc_coins txin;
    txin.contract = contract;
    txin.backing_currency = backing_currency;
    txin.amount = amount;
    this->add_in(txin, coin_type(backing_currency, NotContract, BACKED_BY_N_A));
    
    // update the amounts
    if (!m_ignore_checks)
    {
      CHECK_AND_ASSERT_MES(sub_amount(input_amounts[coin_type(contract, BackingCoin, backing_currency)], amount), false,
                           "Failed to subtract backing coins amount in add_fuse_contract");
      CHECK_AND_ASSERT_MES(sub_amount(input_amounts[coin_type(contract, ContractCoin, backing_currency)], amount), false,
                           "Failed to subtract contract coins amount in add_fuse_contract");
      CHECK_AND_ASSERT_MES(add_amount(input_amounts[coin_type(backing_currency, NotContract, BACKED_BY_N_A)], amount),
                           false, "Failed to add fused amount in add_fuse_contract");
    }
    
    // add the destinations, should include the fused coins + whatever backing coins & contract coins change
    CHECK_AND_ASSERT(add_dests_unchecked(all_destinations, output_amounts), false);
    
    if (!m_ignore_checks)
    {
      // check amounts are valid
      CHECK_AND_ASSERT(check_inputs_outputs(input_amounts, output_amounts), false);
    }
    
    m_state = InProgress;
    
    return true;
  }
  
  bool tx_builder::add_register_delegate(delegate_id_t delegate_id, const account_public_address delegate_address,
                                         uint64_t registration_fee,
                                         const account_keys& fee_account_keys,
                                         const std::vector<tx_source_entry>& fee_sources,
                                         const std::vector<tx_destination_entry>& change_dests)
  {
    CHECK_AND_ASSERT_MES(m_state == InProgress, false, "Cannot add register delegate, tx is not InProgress");
    CHECK_AND_ASSERT_MES(registration_fee > 0 && fee_sources.size() > 0,
                         false, "No registration fee or no registration fee sources");
    
    m_state = Broken;
    
    currency_map input_amounts, output_amounts;
    CHECK_AND_ASSERT(add_sources(fee_account_keys, fee_sources, input_amounts), false);
    
    txin_register_delegate txin;
    txin.delegate_id = delegate_id;
    txin.delegate_address = delegate_address;
    txin.registration_fee = registration_fee;
    this->add_in(txin, CP_XPB);
    
    CHECK_AND_ASSERT(add_dests_unchecked(change_dests, output_amounts), false);
    
    if (!m_ignore_checks)
    {
      CHECK_AND_ASSERT_MES(input_amounts[CP_XPB] >= registration_fee, false,
                           "Not enough sources to cover registration fee");
      CHECK_AND_ASSERT_MES(sub_amount(input_amounts[CP_XPB], registration_fee), false,
                           "Failed to subtract fee amount in add_register_delegate");
      CHECK_AND_ASSERT(check_inputs_outputs(input_amounts, output_amounts), false);
    }
    
    m_state = InProgress;
    
    return true;
  }
  
  bool tx_builder::add_register_delegate(delegate_id_t delegate_id, const account_public_address delegate_address,
                                         uint64_t registration_fee)
  {
    CHECK_AND_ASSERT_MES(m_state == InProgress, false, "Cannot add register delegate, tx is not InProgress");
    
    m_state = Broken;
    
    txin_register_delegate txin;
    txin.delegate_id = delegate_id;
    txin.delegate_address = delegate_address;
    txin.registration_fee = registration_fee;
    this->add_in(txin, CP_XPB);
    
    m_state = InProgress;
    
    return true;
  }
  
  bool tx_builder::add_vote(const account_keys& sender_account_keys,
                            const std::vector<tx_source_entry>& sources,
                            uint16_t seq, const delegate_votes& votes)
  {
    CHECK_AND_ASSERT_MES(m_state == InProgress, false, "Cannot add vote, tx is not InProgress");
    
    m_state = Broken;
    
    currency_map input_amounts;
    CHECK_AND_ASSERT(add_sources(sender_account_keys, sources, input_amounts,
                                 true, seq, votes), false);

    m_state = InProgress;
    
    return true;
  }
  
  bool tx_builder::finalize()
  {
    CHECK_AND_ASSERT_MES(m_state == InProgress, false, "Cannot finalize transaction, tx is not InProgress");
    
    m_state = Broken;
    
    // make sure version is correct, might need an update due to having called finalize with a modifier
    for (size_t i = 0; i < m_tx.ins().size(); i++) {
      update_version_to(m_tx.ins()[i], m_tx.in_cp(i));
    }
    for (size_t i = 0; i < m_tx.outs().size(); i++) {
      update_version_to(m_tx.outs()[i], m_tx.out_cp(i));
    }
    
    // Generate ring signatures
    // Initialize with empty signatures as placeholders - mint and remint will remain empty
    assert(m_tx.signatures.empty());
    m_tx.signatures.resize(m_tx.ins().size(), std::vector<crypto::signature>());
 
    crypto::hash tx_prefix_hash;
    CHECK_AND_ASSERT(get_transaction_prefix_hash(m_tx, tx_prefix_hash), false);

    std::stringstream ss_ring_s;
    size_t source_index = 0;
    BOOST_FOREACH(const tx_source_entry& src_entr, m_sources_used)
    {
      ss_ring_s << "pub_keys:" << ENDL;
      std::vector<const crypto::public_key*> keys_ptrs;
      BOOST_FOREACH(const tx_source_entry::output_entry& o, src_entr.outputs)
      {
        keys_ptrs.push_back(&o.second);
        ss_ring_s << o.second << ENDL;
      }
      
      size_t vin_index = m_source_to_vin_index[source_index];

      std::vector<crypto::signature>& sigs = m_tx.signatures[vin_index];
      sigs.resize(src_entr.outputs.size());
      crypto::generate_ring_signature(tx_prefix_hash, get_txin_k_image(m_tx.ins()[vin_index]), keys_ptrs,
                                      m_in_contexts[source_index].in_ephemeral.sec, src_entr.real_output, sigs.data());
      ss_ring_s << "signatures:" << ENDL;
      std::for_each(sigs.begin(), sigs.end(), [&](const crypto::signature& s){ss_ring_s << s << ENDL;});
      ss_ring_s << "prefix_hash:" << tx_prefix_hash << ENDL << "in_ephemeral_key: " << m_in_contexts[source_index].in_ephemeral.sec << ENDL << "real_output: " << src_entr.real_output;
      source_index++;
    }

    crypto::hash tx_hash;
    CHECK_AND_ASSERT(get_transaction_hash(m_tx, tx_hash), false);

    LOG_PRINT2("construct_tx.log", "transaction_created: " << tx_hash << ENDL
               << obj_to_json_str(m_tx) << ENDL
               << ss_ring_s.str(), LOG_LEVEL_3);
    
    m_state = Finalized;

    return true;
  }
  
  bool tx_builder::get_finalized_tx(transaction& tx, keypair& txkey) const
  {
    CHECK_AND_ASSERT_MES(m_state == Finalized, false, "Cannot get transaction, tx is not finalized");
    
    tx = m_tx;
    txkey = m_txkey;
    
    return true;
  }
  
  bool tx_builder::get_finalized_tx(transaction& tx) const
  {
    keypair txkey;
    return get_finalized_tx(tx, txkey);
  }
  
  bool tx_builder::get_unlock_time(uint64_t& unlock_time) const
  {
    CHECK_AND_ASSERT_MES(m_state != Uninitialized, false, "Cannot get unlock time, tx not initialized yet");
    
    return m_tx.unlock_time;
  }
}
