// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "cryptonote_basic.h"
#include "account.h"
#include "keypair.h"

#include <map>

namespace cryptonote {
  struct tx_source_entry; // defined in cryptonote_format_utils.h
  struct tx_destination_entry;
  
  class tx_builder {
  public:
    enum tx_builder_state {
      Uninitialized,
      InProgress,
      Finalized,
      Broken
    };
    
    tx_builder();
    
    bool init(uint64_t unlock_time, std::vector<uint8_t> extra, bool ignore_overflow_check=false);
    bool init(uint64_t unlock_time);
    bool init();
    
    bool add_send(const account_keys& sender_account_keys,
                  const std::vector<tx_source_entry>& sources,
                  const std::vector<tx_destination_entry>& destinations);
    
    bool add_mint(uint64_t currency, const std::string& description, uint64_t amount, uint64_t decimals,
                  const crypto::public_key& remint_key,
                  const std::vector<tx_destination_entry>& destinations);
    bool add_remint(uint64_t currency, uint64_t amount,
                    const crypto::secret_key& remint_skey,
                    const crypto::public_key& new_remint_key,
                    const std::vector<tx_destination_entry>& destinations);
    
    bool add_contract(uint64_t contract, const std::string& description,
                      const crypto::public_key& grading_key,
                      uint32_t fee_scale, uint64_t expiry_block, uint32_t default_grade);
    bool add_grade_contract(uint64_t contract, uint32_t grade,
                            const crypto::secret_key& grading_secret_key,
                            const std::vector<tx_destination_entry>& grading_fee_destinations);
    bool add_mint_contract(const account_keys& sender_account_keys,
                           uint64_t for_contract, uint64_t amount_to_mint,
                           const std::vector<tx_source_entry>& backing_sources,
                           const std::vector<tx_destination_entry>& all_destinations);
    bool add_fuse_contract(uint64_t contract, uint64_t backing_currency, uint64_t amount,
                           const account_keys& backing_account_keys,
                           const std::vector<tx_source_entry> backing_sources,
                           const account_keys& contract_account_keys,
                           const std::vector<tx_source_entry> contract_sources,
                           const std::vector<tx_destination_entry>& all_destinations);

    bool add_register_delegate(delegate_id_t delegate_id, const account_public_address delegate_address,
                               uint64_t registration_fee,
                               const account_keys& fee_account_keys,
                               const std::vector<tx_source_entry>& fee_sources,
                               const std::vector<tx_destination_entry>& change_dests);
    bool add_register_delegate(delegate_id_t delegate_id, const account_public_address delegate_address,
                               uint64_t registration_fee);
    
    bool add_vote(const account_keys& sender_account_keys,
                  const std::vector<tx_source_entry>& sources,
                  uint16_t seq, const delegate_votes& votes);
    
    bool finalize();
    
    template <class tx_modifier_t>
    bool finalize(const tx_modifier_t& f) {
      f(m_tx);
      return finalize();
    }
    
    template <class tx_modifier_t>
    bool finalize(tx_modifier_t& f) {
      f(m_tx);
      return finalize();
    }

    bool get_finalized_tx(transaction& tx, keypair& txkey) const;
    bool get_finalized_tx(transaction& tx) const;
    
    bool get_unlock_time(uint64_t& unlock_time) const;
    
  private:
    bool add_sources(const account_keys& sender_account_keys,
                     const std::vector<tx_source_entry>& sources, currency_map& input_amounts,
                     bool vote=false, uint16_t vote_seq=0, const delegate_votes& votes=DPOS_NO_VOTES);
    bool add_dests_unchecked(const std::vector<tx_destination_entry>& destinations, currency_map& output_amounts);
    bool add_dests_unchecked(const std::vector<tx_destination_entry>& destinations);
    bool check_inputs_outputs(currency_map& input_amounts, currency_map& output_amounts);
    bool add_dests(currency_map& input_amounts, const std::vector<tx_destination_entry>& destinations);
    
    void add_in(const txin_v& inp, const coin_type& cp);
    void add_out(const tx_out& outp, const coin_type& cp);
    
    void update_version_to(size_t min_version);
    void update_version_to(const txin_v& inp, const coin_type& cp);
    void update_version_to(const tx_out& outp, const coin_type& cp);
    
    struct input_generation_context_data
    {
      keypair in_ephemeral;
    };
    
    bool m_ignore_checks;
    tx_builder_state m_state;
    transaction m_tx;
    keypair m_txkey;
    std::vector<tx_source_entry> m_sources_used;
    std::vector<input_generation_context_data> m_in_contexts;
    std::map<size_t, size_t> m_source_to_vin_index;
  };
}
