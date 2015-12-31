// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <vector>
#include <utility>
#include <list>
#include <functional>

#include "include_base_utils.h"
#include "misc_language.h"

#include "common/functional.h"
#include "common/stl-util.h"
#include "common/types.h"
#include "crypto/crypto.h"
#include "crypto/crypto_basic_impl.h"
#include "cryptonote_core/tx_builder.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/coin_type.h"

#include "wallet2.h"
#include "wallet_errors.h"
#include "wallet_tx_builder.h"

using tools::map_filter;

namespace {
//----------------------------------------------------------------------------------------------------
cryptonote::currency_map calculate_needed_money(std::vector<cryptonote::tx_destination_entry> dests, uint64_t fee)
{
  cryptonote::currency_map needed_money;
  needed_money[cryptonote::CP_XPB] = fee;
  BOOST_FOREACH(auto& dt, dests)
  {
    THROW_WALLET_EXCEPTION_IF(0 == dt.amount, tools::error::zero_destination);
    needed_money[dt.cp] += dt.amount;
    THROW_WALLET_EXCEPTION_IF(needed_money[dt.cp] < dt.amount, tools::error::tx_sum_overflow, dests, fee);
  }
  return needed_money;
}
//----------------------------------------------------------------------------------------------------
template<typename T>
T pop_random_value(std::vector<T>& vec)
{
  CHECK_AND_ASSERT_MES(!vec.empty(), T(), "Vector must be non-empty");

  size_t idx = crypto::rand<size_t>() % vec.size();
  T res = vec[idx];
  if (idx + 1 != vec.size())
  {
    vec[idx] = vec.back();
  }
  vec.resize(vec.size() - 1);

  return res;
}
//----------------------------------------------------------------------------------------------------
void print_source_entry(const cryptonote::tx_source_entry& src)
{
  std::string indexes;
  std::for_each(src.outputs.begin(), src.outputs.end(), [&](const cryptonote::tx_source_entry::output_entry& s_e) {
    indexes += std::to_string(s_e.first) + " ";
  });
  LOG_PRINT_L0("amount_in=" << cryptonote::print_money(src.amount_in)
               << ", amount_out=" << cryptonote::print_money(src.amount_out)
               << ", real_output=" << src.real_output
               << ", real_output_in_tx_index=" << src.real_output_in_tx_index
               << ", indexes: " << indexes);
}

}

namespace tools {
  
class wallet_tx_builder::impl
{
private:
  enum state {
    Uninitialized,
    InProgress,
    Finalized,
    ProcessedSent,
    Broken
  };
  
public:
  impl(wallet2& wallet)
      : m_wallet(wallet)
      , m_state(Uninitialized)
      , m_kd(AUTO_VAL_INIT(m_kd))
      , m_scanty_outs_amount(0)
  {
  }
  
  void init_tx(uint64_t unlock_time, const std::vector<uint8_t>& extra);
  void add_send(const std::vector<cryptonote::tx_destination_entry>& dsts, uint64_t fee,
                size_t min_fake_outs, size_t fake_outputs_count, const detail::split_strategy& destination_split_strategy,
                const tx_dust_policy& dust_policy);
  void add_register_delegate(cryptonote::delegate_id_t delegate_id,
                             const cryptonote::account_public_address& address,
                             uint64_t registration_fee);
  uint64_t add_votes(size_t min_fake_outs, size_t fake_outputs_count, const tx_dust_policy& dust_policy,
                     uint64_t num_votes, const cryptonote::delegate_votes& desired_votes,
                     uint64_t delegates_per_vote);
  void replace_seqs(cryptonote::transaction& tx);
  void finalize(cryptonote::transaction& tx);
  void process_transaction_sent();
  
private:
  bool transfer_being_spent(size_t i) const;
  bool batch_being_spent(size_t i) const;
  
  // for voting
  size_t pop_random_weighted_transfer(std::vector<size_t>& indices);
  uint64_t select_batches_for_votes(uint64_t num_votes, uint64_t dust, const cryptonote::delegate_votes& voting_set,
                                    std::list<size_t>& batch_is);
  uint64_t select_transfers_for_votes(uint64_t num_votes, uint64_t dust, std::list<size_t>& transfer_is, size_t min_fake_outs);
  // for spending
  uint64_t select_transfers_for_spend(const cryptonote::coin_type& currency, uint64_t needed_money, uint64_t max_dusts,
                                      uint64_t dust, std::list<size_t>& transfer_is, size_t min_fake_outs);
  cryptonote::currency_map select_transfers_for_spend(cryptonote::currency_map needed_money, uint64_t max_dusts,
                                                      uint64_t dust, std::list<size_t>& transfer_is, size_t min_fake_outs);
  uint64_t select_batches_for_spend(uint64_t needed_money, std::list<size_t>& batch_is);
  
  cryptonote::tx_destination_entry process_change_dests(cryptonote::currency_map& found_money,
                                                        cryptonote::currency_map& needed_money,
                                                        std::vector<cryptonote::tx_destination_entry>& all_dests);
  std::vector<cryptonote::tx_source_entry> prepare_inputs(const std::list<size_t>& transfer_is, fake_outs_map& fake_outputs,
                                                          uint64_t fake_outputs_count);
  std::vector<cryptonote::tx_source_entry> prepare_batch(size_t batch_index);
  fake_outs_map get_fake_outputs(const std::list<size_t>& transfer_is, size_t min_fake_outs, size_t fake_outs_count);
  size_t filter_scanty_outs(std::vector<size_t>& transfer_indices, size_t min_fake_outs);

private:
  cryptonote::tx_builder m_txb;
  
  wallet2& m_wallet;
  state m_state;
  
  // state variables
  struct new_batch_vote_info
  {
    std::list<size_t> m_transfer_is;
    fake_outs_container m_fake_outs;
    cryptonote::delegate_votes m_votes;
  };
  
  struct change_batch_vote_info
  {
    size_t m_batch_i;
    cryptonote::delegate_votes m_votes;
  };
  
  cryptonote::currency_map m_change;
  std::list<size_t> m_spend_transfer_is;
  std::list<size_t> m_spend_batch_is;
  std::list<new_batch_vote_info> m_new_batch_votes;
  std::list<change_batch_vote_info> m_change_batch_votes;
  wallet2_known_transfer_details m_kd;
  
  cryptonote::transaction m_finalized_tx;
  
  uint64_t m_scanty_outs_amount;
};

//----------------------------------------------------------------------------------------------------
bool wallet_tx_builder::impl::transfer_being_spent(size_t i) const
{
  return contains(m_spend_transfer_is, i);
}
//----------------------------------------------------------------------------------------------------
bool wallet_tx_builder::impl::batch_being_spent(size_t i) const
{
  return contains(m_spend_batch_is, i);
}
//----------------------------------------------------------------------------------------------------
size_t wallet_tx_builder::impl::filter_scanty_outs(std::vector<size_t>& transfer_indices, size_t min_fake_outs)
{
  if (min_fake_outs == 0)
    return 0;
  
  std::list<size_t> for_fakes;
  for_fakes.insert(for_fakes.end(), transfer_indices.begin(), transfer_indices.end());
  auto fake_outs = get_fake_outputs(for_fakes, 0, min_fake_outs)[cryptonote::CP_XPB];
  
  std::vector<size_t> new_indices;
  
  size_t num_filtered = 0;
  
  for (size_t ii = 0; ii < transfer_indices.size(); ii++)
  {
    if (fake_outs[ii].size() >= min_fake_outs)
    {
      new_indices.push_back(transfer_indices[ii]);
    }
    else
    {
      num_filtered++;
      m_scanty_outs_amount += m_wallet.m_transfers[transfer_indices[ii]].amount();
    }
  }
  
  transfer_indices = new_indices;
  
  return num_filtered;
}
//----------------------------------------------------------------------------------------------------
size_t wallet_tx_builder::impl::pop_random_weighted_transfer(std::vector<size_t>& indices)
{
  if (indices.empty())
    throw std::runtime_error("Can't get random from empty list");
  
  uint64_t sum = tools::sum(indices, [this](size_t i) { return this->m_wallet.m_transfers[i].amount(); });
  
  auto choice = crypto::rand<uint64_t>() % sum;
  for (size_t vec_i = 0; vec_i < indices.size(); ++vec_i)
  {
    auto amt = m_wallet.m_transfers[indices[vec_i]].amount();
    if (choice <= amt)
    {
      size_t res = indices[vec_i];
      indices.erase(indices.begin() + vec_i);
      return res;
    }
    choice -= amt;
  }
  // should never reach here
  throw std::runtime_error("pop_random_weighted_transfer logic failed");
}
//----------------------------------------------------------------------------------------------------
uint64_t wallet_tx_builder::impl::select_transfers_for_votes(uint64_t num_votes, uint64_t dust,
                                                             std::list<size_t>& transfer_is, size_t min_fake_outs)
{
  std::vector<size_t> unused_transfers_indices;
  for (size_t i = 0; i < m_wallet.m_transfers.size(); ++i)
  {
    const auto& td = m_wallet.m_transfers[i];
    
    if (td.cp() != cryptonote::CP_XPB)
      continue;
    if (td.m_spent)
      continue;
    if (!m_wallet.is_transfer_unlocked(td))
      continue;
    if (transfer_being_spent(i))
      continue;
    if (td.amount() <= dust) // don't vote with dusts
      continue;
    
    size_t voting_batch_index = m_wallet.m_votes_info.m_transfer_batch_map[i];
    
    if (voting_batch_index != 0) // don't use if in a voting batch
      continue;
    
    unused_transfers_indices.push_back(i);
  }
  
  filter_scanty_outs(unused_transfers_indices, min_fake_outs);
  
  uint64_t found_votes = 0;
  while (found_votes < num_votes && !unused_transfers_indices.empty())
  {
    size_t idx = pop_random_weighted_transfer(unused_transfers_indices);
    
    transfer_is.push_back(idx);
    found_votes += m_wallet.m_transfers[idx].amount();
    if (transfer_is.size() >= MAX_VOTE_INPUTS_PER_TX)
      break;
  }
  
  return found_votes;
}
//----------------------------------------------------------------------------------------------------
uint64_t wallet_tx_builder::impl::select_batches_for_votes(uint64_t num_votes, uint64_t dust,
                                                           const cryptonote::delegate_votes& voting_set,
                                                           std::list<size_t>& batch_is)
{
  // pick out those unspent batches that need changes and sort by amount*num_changes_needed
  std::vector<std::pair<uint64_t, size_t> > batch_infos; // (sort_key, index)
  
  for (size_t i = 1; i < m_wallet.m_votes_info.m_batches.size(); i++)
  {
    const auto& batch = m_wallet.m_votes_info.m_batches[i];
    
    THROW_WALLET_EXCEPTION_IF(batch.m_vote_history.empty(),
                              error::wallet_internal_error, "voting batch with no vote history");
    
    if (batch.spent(m_wallet))
      continue;
    if (batch_being_spent(i))
      continue;
    
    // only use if there's something to update
    size_t num_unwanted = set_sub(batch.m_vote_history.back(), voting_set).size();
    
    if (num_unwanted == 0)
    {
      // nothing to update, don't use
      continue;
    }
    
    batch_infos.push_back(std::make_pair(batch.amount(m_wallet) * num_unwanted, i));
  }
  
  std::sort(batch_infos.rbegin(), batch_infos.rend()); // reversed sort
  
  // use up to all usable batches - any anonimity regarding them has already been resolved
  uint64_t found_votes = 0;
  BOOST_FOREACH(const auto& inf, batch_infos)
  {
    LOG_PRINT_L0("Voting with batch " << inf.second << " with score " << cryptonote::print_money(inf.first));
    batch_is.push_back(inf.second);
    found_votes += m_wallet.m_votes_info.m_batches[inf.second].amount(m_wallet);
    if (found_votes >= num_votes)
      break;
  }
  
  return found_votes;
}
//----------------------------------------------------------------------------------------------------
uint64_t wallet_tx_builder::impl::select_transfers_for_spend(const cryptonote::coin_type& cp, uint64_t needed_money,
                                                             uint64_t max_dusts, uint64_t dust, std::list<size_t>& transfer_is,
                                                             size_t min_fake_outs)
{
  THROW_WALLET_EXCEPTION_IF(cp != cryptonote::CP_XPB, error::wallet_internal_error, "non-XPB send not implemented");
  
  std::vector<size_t> unused_transfers_indices;
  std::vector<size_t> unused_dust_indices;
  for (size_t i = 0; i < m_wallet.m_transfers.size(); ++i)
  {
    const auto& td = m_wallet.m_transfers[i];
    if (td.cp() != cp)
      continue;
    if (td.m_spent)
      continue;
    if (!m_wallet.is_transfer_unlocked(td))
      continue;
    if (transfer_being_spent(i))
      continue;
    
    size_t voting_batch_index = m_wallet.m_votes_info.m_transfer_batch_map[i];
    if (voting_batch_index != 0) // don't use if in a batch
      continue;
    
    if (dust < td.amount())
      unused_transfers_indices.push_back(i);
    else
      unused_dust_indices.push_back(i);
  }
  
  filter_scanty_outs(unused_transfers_indices, min_fake_outs);
  filter_scanty_outs(unused_dust_indices, min_fake_outs);
  
  uint64_t num_dusts = 0;
  uint64_t found_money = 0;
  while (found_money < needed_money)
  {
    size_t idx;
    if (!unused_dust_indices.empty() && num_dusts < max_dusts)
    {
      idx = pop_random_value(unused_dust_indices);
      num_dusts++;
    }
    else if (!unused_transfers_indices.empty())
    {
      idx = pop_random_value(unused_transfers_indices);
    }
    else
    {
      break; // not enough money
    }
    
    transfer_is.push_back(idx);
    found_money += m_wallet.m_transfers[idx].amount();
  }

  return found_money;
}
//----------------------------------------------------------------------------------------------------
cryptonote::currency_map wallet_tx_builder::impl::select_transfers_for_spend(
    cryptonote::currency_map needed_money, uint64_t max_dusts, uint64_t dust,
    std::list<size_t>& transfer_is, size_t min_fake_outs)
{
  cryptonote::currency_map found_money;
  
  BOOST_FOREACH(const auto& item, needed_money)
  {
    found_money[item.first] = select_transfers_for_spend(item.first, item.second, max_dusts, dust, transfer_is,
                                                         min_fake_outs);
  }
  
  return found_money;
}
//----------------------------------------------------------------------------------------------------
uint64_t wallet_tx_builder::impl::select_batches_for_spend(uint64_t needed_money, std::list<size_t>& batch_is)
{
  // spend the batches with the smallest amounts
  std::vector<std::pair<uint64_t, size_t> > batch_infos; // (sort_key, index)
  
  for (size_t i = 1; i < m_wallet.m_votes_info.m_batches.size(); i++)
  {
    const auto& batch = m_wallet.m_votes_info.m_batches[i];
    
    THROW_WALLET_EXCEPTION_IF(batch.m_vote_history.empty(),
                              error::wallet_internal_error, "voting batch with no vote history");
    
    if (batch.spent(m_wallet))
      continue;
    if (batch_being_spent(i))
      continue;
    
    batch_infos.push_back(std::make_pair(batch.amount(m_wallet), i));
  }
  
  std::sort(batch_infos.begin(), batch_infos.end()); // regular sort
  
  uint64_t found_money = 0;
  BOOST_FOREACH(const auto& inf, batch_infos)
  {
    LOG_PRINT_L0("Spending batch " << inf.second);
    batch_is.push_back(inf.second);
    found_money += m_wallet.m_votes_info.m_batches[inf.second].amount(m_wallet);
    if (found_money >= needed_money)
      break;
  }
  
  return found_money;
}
//----------------------------------------------------------------------------------------------------
cryptonote::tx_destination_entry wallet_tx_builder::impl::process_change_dests(
    cryptonote::currency_map& found_money, cryptonote::currency_map& needed_money,
    std::vector<cryptonote::tx_destination_entry>& all_dests)
{
  cryptonote::tx_destination_entry xpb_change_dest;
  BOOST_FOREACH(const auto& item, needed_money) {
    // Send back change
    if (found_money[item.first] > needed_money[item.first])
    {
      cryptonote::tx_destination_entry change_dest;
      change_dest.addr = m_wallet.m_account.get_keys().m_account_address;
      change_dest.cp = item.first;
      change_dest.amount = found_money[item.first] - needed_money[item.first];
      if (item.first == cryptonote::CP_XPB)
        xpb_change_dest = change_dest;
      else
        all_dests.push_back(change_dest);
    }
  }
  
  return xpb_change_dest;
}
//----------------------------------------------------------------------------------------------------
std::vector<cryptonote::tx_source_entry> wallet_tx_builder::impl::prepare_inputs(
    const std::list<size_t>& transfer_is, fake_outs_map& fake_outputs, uint64_t fake_outputs_count)
{
  typedef cryptonote::tx_source_entry::output_entry tx_output_entry;
  
  size_t i = 0;
  std::vector<cryptonote::tx_source_entry> sources;
  BOOST_FOREACH(size_t idx, transfer_is)
  {
    sources.resize(sources.size()+1);
    cryptonote::tx_source_entry& src = sources.back();
    const auto& td = m_wallet.m_transfers[idx];
    src.type = cryptonote::tx_source_entry::InToKey;
    src.cp = td.cp();
    src.amount_in = src.amount_out = td.amount();
    //paste mixin transaction
    auto& these_fakes = fake_outputs[td.cp()];
    if(i < these_fakes.size())
    {
      these_fakes[i].sort([](const out_entry& a, const out_entry& b){return a.global_amount_index < b.global_amount_index;});
      BOOST_FOREACH(const auto& daemon_oe, these_fakes[i])
      {
        if(td.m_global_output_index == daemon_oe.global_amount_index)
          continue;
        tx_output_entry oe;
        oe.first = daemon_oe.global_amount_index;
        oe.second = daemon_oe.out_key;
        src.outputs.push_back(oe);
        if(src.outputs.size() >= fake_outputs_count)
          break;
      }
    }

    //paste real transaction to the random index
    auto it_to_insert = std::find_if(src.outputs.begin(), src.outputs.end(), [&](const tx_output_entry& a)
    {
      return a.first >= td.m_global_output_index;
    });
    //size_t real_index = src.outputs.size() ? (rand() % src.outputs.size() ):0;
    tx_output_entry real_oe;
    real_oe.first = td.m_global_output_index;
    real_oe.second = boost::get<cryptonote::txout_to_key>(td.m_tx.outs()[td.m_internal_output_index].target).key;
    auto interted_it = src.outputs.insert(it_to_insert, real_oe);
    src.real_out_tx_key = get_tx_pub_key_from_extra(td.m_tx);
    src.real_output = interted_it - src.outputs.begin();
    src.real_output_in_tx_index = td.m_internal_output_index;
    print_source_entry(src);
    ++i;
  }
  
  return sources;
}
//----------------------------------------------------------------------------------------------------
std::vector<cryptonote::tx_source_entry> wallet_tx_builder::impl::prepare_batch(size_t batch_idx)
{
  THROW_WALLET_EXCEPTION_IF(batch_idx >= m_wallet.m_votes_info.m_batches.size(), error::wallet_internal_error,
                            "invalid batch index");
  
  std::list<size_t> selected_transfers;
  fake_outs_map fake_outputs;
  
  const auto& batch = m_wallet.m_votes_info.m_batches[batch_idx];
  
  size_t i = 0; // index into m_fake_outs
  BOOST_FOREACH(size_t transfer_ix, batch.m_transfer_indices) {
    THROW_WALLET_EXCEPTION_IF(transfer_ix >= m_wallet.m_transfers.size(), error::wallet_internal_error,
                              "Invalid index into m_transfers in batch");
    // include only un-spent members: spent members already accounted for in batch selection
    if (m_wallet.m_transfers[transfer_ix].m_spent)
    {
      i++;
      continue;
    }
    
    selected_transfers.push_back(transfer_ix);
    
    THROW_WALLET_EXCEPTION_IF(i >= batch.m_fake_outs.size(), error::wallet_internal_error,
                              "batch has not enough fake outs");
    fake_outputs[cryptonote::CP_XPB].push_back(batch.m_fake_outs[i]);
    
    i++;
  }
  
  return prepare_inputs(selected_transfers, fake_outputs, 999999); // use as many fake outs as the batch has
}
//----------------------------------------------------------------------------------------------------
fake_outs_map wallet_tx_builder::impl::get_fake_outputs(const std::list<size_t>& transfer_is,
                                                        size_t min_fake_outs, size_t fake_outs_count)
{
  std::unordered_map<cryptonote::coin_type, std::list<uint64_t> > amounts;
  BOOST_FOREACH(size_t idx, transfer_is)
  {
    const auto& td = m_wallet.m_transfers[idx];
    THROW_WALLET_EXCEPTION_IF(td.m_tx.outs().size() <= td.m_internal_output_index, error::wallet_internal_error,
                              "m_internal_output_index = " + std::to_string(td.m_internal_output_index) +
                              " is greater or equal to outputs count = " + std::to_string(td.m_tx.outs().size()));
    THROW_WALLET_EXCEPTION_IF(td.cp() != cryptonote::CP_XPB, error::wallet_internal_error, "got transfer which wasn't xpb");
    amounts[cryptonote::CP_XPB].push_back(td.amount());
  }
    
  return m_wallet.get_fake_outputs(amounts, min_fake_outs, fake_outs_count);
}
//----------------------------------------------------------------------------------------------------
void wallet_tx_builder::impl::init_tx(uint64_t unlock_time, const std::vector<uint8_t>& extra)
{
  THROW_WALLET_EXCEPTION_IF(m_state != Uninitialized, error::wallet_internal_error,
                            "wallet tx already is not uninitialized");
  
  m_state = Broken;
  
  THROW_WALLET_EXCEPTION_IF(!m_txb.init(unlock_time, extra, false), error::wallet_internal_error,
                            "could not init tx builder");
  
  m_state = InProgress;
}
//----------------------------------------------------------------------------------------------------
void wallet_tx_builder::impl::add_send(const std::vector<cryptonote::tx_destination_entry>& dsts, uint64_t fee,
                                       size_t min_fake_outs, size_t fake_outputs_count,
                                       const detail::split_strategy& destination_split_strategy,
                                       const tx_dust_policy& dust_policy)
{
  THROW_WALLET_EXCEPTION_IF(m_state != InProgress, error::wallet_internal_error, "wallet tx is not in progress");
  
  m_state = Broken;
  
  // get needed money
  auto needed_money = calculate_needed_money(dsts, fee);
  
  // pick transfers/batches
  std::list<size_t> transfer_is;
  std::list<size_t> batch_is;
  cryptonote::currency_map found_money = select_transfers_for_spend(needed_money, 5, // at most 5 dusts per tx
                                                                    dust_policy.dust_threshold, transfer_is,
                                                                    min_fake_outs);
  // check found enough
  BOOST_FOREACH(const auto& item, needed_money)
  {
    uint64_t this_fee = item.first == cryptonote::CP_XPB ? fee : 0;
    
    if (found_money[item.first] < item.second)
    {
      // if not xpbs, there's no chance
      THROW_WALLET_EXCEPTION_IF(item.first != cryptonote::CP_XPB,
                                error::not_enough_money, item.first, found_money[item.first],
                                needed_money[item.first] - this_fee, this_fee,
                                m_scanty_outs_amount);
      
      // if yes xpbs, try spending a batch
      uint64_t amount_missing = needed_money[item.first] - found_money[item.first];
      uint64_t batch_found = select_batches_for_spend(amount_missing, batch_is);
      THROW_WALLET_EXCEPTION_IF(batch_found < amount_missing,
                                error::not_enough_money, item.first, found_money[item.first] + batch_found,
                                needed_money[item.first] - this_fee, this_fee,
                                m_scanty_outs_amount);
      found_money[item.first] += batch_found;
    }
  }

  // get fake outs for transfers
  auto fake_outputs = get_fake_outputs(transfer_is, min_fake_outs, fake_outputs_count);
  
  // prepare transfer inputs
  auto sources = prepare_inputs(transfer_is, fake_outputs, fake_outputs_count);
  
  // prepare the batches for spending
  BOOST_FOREACH(size_t idx, batch_is)
  {
    auto batch_sources = prepare_batch(idx); // uses batch's previous fake outs
    sources.insert(sources.end(), batch_sources.begin(), batch_sources.end());
  }
  
  // split destinations
  auto all_dests = dsts;
  auto xpb_change_dest = process_change_dests(found_money, needed_money, all_dests);
  
  uint64_t xpb_dust = 0;
  std::vector<cryptonote::tx_destination_entry> split_dests;
  destination_split_strategy.split(all_dests, xpb_change_dest, dust_policy.dust_threshold, split_dests, xpb_dust);
  
  THROW_WALLET_EXCEPTION_IF(xpb_dust > dust_policy.dust_threshold, error::wallet_internal_error,
                            "invalid dust value: dust = " + std::to_string(xpb_dust)
                            + ", dust_threshold = " + std::to_string(dust_policy.dust_threshold));
  if (xpb_dust > 0 && !dust_policy.add_to_fee)
  {
    split_dests.push_back(cryptonote::tx_destination_entry(cryptonote::CP_XPB, xpb_dust, dust_policy.addr_for_dust));
  }
  
  // add to tx
  bool r = m_txb.add_send(m_wallet.m_account.get_keys(), sources, split_dests);
  {
    uint64_t unlock_time;
    m_txb.get_unlock_time(unlock_time);
    THROW_WALLET_EXCEPTION_IF(!r, error::tx_not_constructed, sources, split_dests, unlock_time);
  }

  // update known transfer details
  m_kd.m_dests.insert(m_kd.m_dests.end(), dsts.begin(), dsts.end());
  m_kd.m_fee += fee;
  BOOST_FOREACH(const auto& item, found_money)
  {
    m_kd.m_all_change[item.first] += found_money[item.first] - needed_money[item.first];
  }
  m_kd.m_xpb_change += m_kd.m_all_change[cryptonote::CP_XPB];
  
  // update transfers & batches being spent here
  m_spend_transfer_is.splice(m_spend_transfer_is.end(), transfer_is);
  m_spend_batch_is.splice(m_spend_batch_is.end(), batch_is);
  
  m_state = InProgress;
  
  return;
}
//----------------------------------------------------------------------------------------------------
void wallet_tx_builder::impl::add_register_delegate(cryptonote::delegate_id_t delegate_id,
                                                    const cryptonote::account_public_address& address,
                                                    uint64_t registration_fee)
{
  THROW_WALLET_EXCEPTION_IF(m_state != InProgress, error::wallet_internal_error, "wallet tx is not in progress");
  
  m_state = Broken;
  
  THROW_WALLET_EXCEPTION_IF(m_kd.m_delegate_id_registered, error::wallet_internal_error,
                            "wallet tx can only register one delegate id per tx");
  
  bool success = m_txb.add_register_delegate(delegate_id, address, registration_fee);
  {
    uint64_t unlock_time;
    m_txb.get_unlock_time(unlock_time);
    THROW_WALLET_EXCEPTION_IF(!success, error::tx_not_constructed,
                              std::vector<cryptonote::tx_source_entry>(),
                              std::vector<cryptonote::tx_destination_entry>(), unlock_time);
  }
  
  m_kd.m_delegate_id_registered = delegate_id;
  m_kd.m_delegate_address_registered = address;
  m_kd.m_registration_fee_paid = registration_fee;
  
  m_state = InProgress;
}
//----------------------------------------------------------------------------------------------------
uint64_t wallet_tx_builder::impl::add_votes(size_t min_fake_outs, size_t fake_outs_count, const tx_dust_policy& dust_policy,
                                            uint64_t num_votes, const cryptonote::delegate_votes& desired_votes,
                                            uint64_t delegates_per_vote)
{
  THROW_WALLET_EXCEPTION_IF(m_state != InProgress, error::wallet_internal_error, "wallet tx is not in progress");
  
  m_state = Broken;
  
  if (!desired_votes.empty())
  {
    // try with unbatched, unspent transfers first
    std::list<size_t> transfer_is;
    uint64_t result = select_transfers_for_votes(num_votes, dust_policy.dust_threshold, transfer_is,
                                                 min_fake_outs);
  
    if (result)
    {
      LOG_PRINT_L0("Making new vote batch of " << cryptonote::print_money(result) << " votes from unspent, unbatched, unvoted transfers");
      
      // pick votes
      auto new_votes = random_subset(desired_votes, delegates_per_vote);
      // get fake outs
      auto fake_outputs = get_fake_outputs(transfer_is, min_fake_outs, fake_outs_count);
      THROW_WALLET_EXCEPTION_IF(fake_outputs[cryptonote::CP_XPB].size() != transfer_is.size(),
                                error::wallet_internal_error, "Didn't get right # of fake outs");
      // prepare inputs
      auto sources = prepare_inputs(transfer_is, fake_outputs, fake_outs_count);
      size_t seq = 0;
      // add to tx
      bool r = m_txb.add_vote(m_wallet.m_account.get_keys(), sources, seq, new_votes);
      THROW_WALLET_EXCEPTION_IF(!r, error::wallet_internal_error, "Could not add batch vote to tx builder");
      
      
      // update known details
      BOOST_FOREACH(const auto& d_id, new_votes)
      {
        m_kd.m_votes[d_id] += result;
      }
      // update state info
      new_batch_vote_info nbvi;
      nbvi.m_transfer_is = transfer_is;
      nbvi.m_fake_outs = fake_outputs[cryptonote::CP_XPB];
      nbvi.m_votes = new_votes;
      m_new_batch_votes.push_back(nbvi);
      
      m_state = InProgress;
      
      return result;
    }
  }
  
  // try with batches
  std::list<size_t> batch_is;
  uint64_t result = select_batches_for_votes(num_votes, dust_policy.dust_threshold, desired_votes, batch_is);
  
  if (!result)
  {
    LOG_PRINT_L0("Found no unused transfers or batches to vote with");
    m_state = InProgress;
    return 0;
  }
  
  BOOST_FOREACH(size_t idx, batch_is)
  {
    const auto& batch = m_wallet.m_votes_info.m_batches[idx];
    
    // pick votes
    const auto& old_votes = batch.m_vote_history.back();
    // take what's still common to the batch and to the desired votes
    auto new_votes = set_and(old_votes, desired_votes);
    // and add a random amount of votes that are in desired but not in the new_votes, up to delegates_per_vote total votes
    new_votes = set_or(new_votes, random_subset(set_sub(desired_votes, new_votes), delegates_per_vote - new_votes.size()));

    // prepare batch (uses previous fake outs)
    auto sources = prepare_batch(idx);
    size_t seq = batch.m_vote_history.size();
    
    // add to tx
    bool r = m_txb.add_vote(m_wallet.m_account.get_keys(), sources, seq, new_votes);
    THROW_WALLET_EXCEPTION_IF(!r, error::wallet_internal_error, "Could not add batch vote to tx builder");
    
    // update known details
    uint64_t amount_voted = batch.amount(m_wallet);
    BOOST_FOREACH(const auto& d_id, new_votes)
    {
      m_kd.m_votes[d_id] += amount_voted;
    }
    // update state info
    change_batch_vote_info cbvi;
    cbvi.m_batch_i = idx;
    cbvi.m_votes = new_votes;
    m_change_batch_votes.push_back(cbvi);
    
    LOG_PRINT_L0("Added batch voting for " << new_votes.size() << " delegates "
                 << "(changing " << (set_or(old_votes, new_votes).size() - set_and(old_votes, new_votes).size()) << " votes) "
                 << "for "  << cryptonote::print_money(amount_voted) << " XPBs");
  }
  
  m_state = InProgress;
  
  return result;
}
//----------------------------------------------------------------------------------------------------
void wallet_tx_builder::impl::replace_seqs(cryptonote::transaction& tx)
{
  using namespace cryptonote;

  // return type needed in lambda for MSVC 2012
  auto k_imgs = map_filter([](const txin_v& inp) -> crypto::key_image { return boost::get<txin_vote>(inp).ink.k_image; },
	                       tx.ins(),
                           [](const txin_v& inp) { return inp.type() == typeid(txin_vote); });
  
  auto im_seqs = m_wallet.get_key_image_seqs(k_imgs);
  tx.replace_vote_seqs(im_seqs);
}

//----------------------------------------------------------------------------------------------------
void wallet_tx_builder::impl::finalize(cryptonote::transaction& tx)
{
  THROW_WALLET_EXCEPTION_IF(m_state != InProgress, error::wallet_internal_error, "wallet tx is not in progress");
  
  m_state = Broken;
  
  // sanity checks
  BOOST_FOREACH(size_t transfer_i, m_spend_transfer_is)
  {
    THROW_WALLET_EXCEPTION_IF(m_wallet.m_transfers.size() <= transfer_i, error::wallet_internal_error,
                              "picked transfer spend that doesn't exist");
  }
  
  BOOST_FOREACH(size_t batch_i, m_spend_batch_is)
  {
    THROW_WALLET_EXCEPTION_IF(m_wallet.m_votes_info.m_batches.size() <= batch_i, error::wallet_internal_error,
                              "picked batch spend that doesn't exist");
  }
  
  BOOST_FOREACH(const auto& cbvi, m_change_batch_votes)
  {
    THROW_WALLET_EXCEPTION_IF(m_wallet.m_votes_info.m_batches.size() <= cbvi.m_batch_i, error::wallet_internal_error,
                              "picked batch change that doesn't exist");
    THROW_WALLET_EXCEPTION_IF(m_wallet.m_votes_info.m_batches[cbvi.m_batch_i].m_vote_history.back() == cbvi.m_votes,
                              error::wallet_internal_error,
                              "changing batch vote but votes aren't actually different");
  }
  
  BOOST_FOREACH(const auto& nbvi, m_new_batch_votes)
  {
    BOOST_FOREACH(size_t transfer_i, nbvi.m_transfer_is)
    {
      THROW_WALLET_EXCEPTION_IF(m_wallet.m_transfers.size() <= transfer_i, error::wallet_internal_error,
                                "picked transfer vote in ne wbatch that doesn't exist");
      THROW_WALLET_EXCEPTION_IF(m_wallet.m_votes_info.m_transfer_batch_map[transfer_i] != 0, error::wallet_internal_error,
                                "placing transfer in new batch that was already in batch");
    }
  }
  
  // replace the seqs with what they should be from the daemon
  
  bool r = true;
  r = r && m_txb.finalize([this](cryptonote::transaction& tx) { return this->replace_seqs(tx); });
  r = r && m_txb.get_finalized_tx(m_finalized_tx);
  THROW_WALLET_EXCEPTION_IF(!r, error::wallet_internal_error, "Could not finalize tx");
  
  m_kd.m_tx_hash = cryptonote::get_transaction_hash(m_finalized_tx);
  
  tx = m_finalized_tx;
  
  m_state = Finalized;
  
  return;
}

//----------------------------------------------------------------------------------------------------
void wallet_tx_builder::impl::process_transaction_sent()
{
  THROW_WALLET_EXCEPTION_IF(m_state != Finalized, error::wallet_internal_error, "wallet tx is not finalized");
  
  m_state = Broken;

  // update everything being spent + voted
  BOOST_FOREACH(size_t transfer_i, m_spend_transfer_is)
  {
    m_wallet.m_transfers[transfer_i].m_spent = true;
    LOG_PRINT_L0("Transfer " << transfer_i << " spent");
  }
  
  BOOST_FOREACH(size_t batch_i, m_spend_batch_is)
  {
    const auto& batch = m_wallet.m_votes_info.m_batches[batch_i];
    BOOST_FOREACH(size_t transfer_i, batch.m_transfer_indices)
    {
      LOG_PRINT_L0("Transfer " << transfer_i << " in batch " << batch_i << " spent");
      m_wallet.m_transfers[transfer_i].m_spent = true;
    }
    LOG_PRINT_L0("Batch " << batch_i << " spent");
  }
  
  BOOST_FOREACH(const auto& cbvi, m_change_batch_votes)
  {
    auto& batch = m_wallet.m_votes_info.m_batches[cbvi.m_batch_i];
    
    batch.m_vote_history.push_back(cbvi.m_votes);
    LOG_PRINT_L0("Batch " << cbvi.m_batch_i << " changed votes");
  }

  BOOST_FOREACH(const auto& nbvi, m_new_batch_votes)
  {
    auto& w_batches = m_wallet.m_votes_info.m_batches;
    w_batches.resize(w_batches.size() + 1);
    size_t new_batch_index = w_batches.size() - 1;
    if (new_batch_index == 0) // create fake first batch since 0 means not-in-a-batch
    {
      w_batches.resize(w_batches.size() + 1);
      new_batch_index = w_batches.size() - 1;
    }
    auto& new_batch = w_batches.back();
    
    new_batch.m_transfer_indices = nbvi.m_transfer_is;
    new_batch.m_fake_outs = nbvi.m_fake_outs;
    new_batch.m_vote_history.push_back(nbvi.m_votes);
    
    BOOST_FOREACH(size_t transfer_i, new_batch.m_transfer_indices)
    {
      m_wallet.m_votes_info.m_transfer_batch_map[transfer_i] = new_batch_index;
      LOG_PRINT_L0("Transfer " << transfer_i << " now in new batch " << new_batch_index);
    }
  }
  
  m_wallet.add_unconfirmed_tx(m_finalized_tx, m_kd.m_xpb_change);
  
  m_wallet.m_known_transfers[m_kd.m_tx_hash] = m_kd;
  
  if (m_wallet.m_callback != NULL)
    m_wallet.m_callback->on_new_transfer(m_finalized_tx, m_kd);
  
  // bring debug info
  std::string key_images;
  std::all_of(m_finalized_tx.ins().begin(), m_finalized_tx.ins().end(), [&](const cryptonote::txin_v& s_e) {
    if (s_e.type() == typeid(cryptonote::txin_to_key))
    {
      const auto& inp = boost::get<cryptonote::txin_to_key>(s_e);
      key_images += boost::lexical_cast<std::string>(inp.k_image) + " ";
    }
    else if (s_e.type() == typeid(cryptonote::txin_vote))
    {
      const auto& inp = boost::get<cryptonote::txin_vote>(s_e);
      key_images += "(vote " + boost::lexical_cast<std::string>(inp.ink.k_image) + ") ";
    }
    return true;
  });
  LOG_PRINT_L2("transaction " << m_kd.m_tx_hash << " generated ok and sent to daemon, "
               << "key_images: [" << key_images << "]");
  
  LOG_PRINT_L0("Transaction successfully sent. <" << m_kd.m_tx_hash << ">" << ENDL
               << "Commission: " << cryptonote::print_money(m_kd.m_fee)
               << " + maybe dust" << ENDL
               << "Balance: " << cryptonote::print_moneys(m_wallet.balance()) << ENDL
               << "Unlocked: " << cryptonote::print_moneys(m_wallet.unlocked_balance()) << ENDL
               << "Please, wait for confirmation for your balance to be unlocked.");
  
  LOG_PRINT_L0("Transaction was: " << ENDL << obj_to_json_str(m_finalized_tx));
  
  if (m_kd.m_delegate_id_registered != 0)
  {
    LOG_PRINT_L0("Transaction registered delegate " << m_kd.m_delegate_id_registered
                 << " with address " << m_kd.m_delegate_address_registered);
  }

  m_state = ProcessedSent;
  
  return;
}
//----------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------
wallet_tx_builder::wallet_tx_builder(wallet2& wallet)
    : m_pimpl(new impl(wallet))
{
}
//----------------------------------------------------------------------------------------------------
wallet_tx_builder::~wallet_tx_builder()
{
  delete m_pimpl; m_pimpl = NULL;
}
//----------------------------------------------------------------------------------------------------
void wallet_tx_builder::init_tx(uint64_t unlock_time, const std::vector<uint8_t>& extra)
{
  return m_pimpl->init_tx(unlock_time, extra);
}
//----------------------------------------------------------------------------------------------------
void wallet_tx_builder::add_send(const std::vector<cryptonote::tx_destination_entry>& dsts, uint64_t fee,
                                 size_t min_fake_outs, size_t fake_outputs_count,
                                 const detail::split_strategy& destination_split_strategy,
                                 const tx_dust_policy& dust_policy)
{
  return m_pimpl->add_send(dsts, fee, min_fake_outs, fake_outputs_count, destination_split_strategy, dust_policy);
}
//----------------------------------------------------------------------------------------------------
uint64_t wallet_tx_builder::add_votes(size_t min_fake_outs, size_t fake_outputs_count, const tx_dust_policy& dust_policy,
                                      uint64_t num_votes, const cryptonote::delegate_votes& desired_votes,
                                      uint64_t delegates_per_vote)
{
  return m_pimpl->add_votes(min_fake_outs, fake_outputs_count, dust_policy, num_votes, desired_votes, delegates_per_vote);
}
void wallet_tx_builder::add_register_delegate(cryptonote::delegate_id_t delegate_id,
                                              const cryptonote::account_public_address& address,
                                              uint64_t registration_fee)
{
  return m_pimpl->add_register_delegate(delegate_id, address, registration_fee);
}
//----------------------------------------------------------------------------------------------------
void wallet_tx_builder::finalize(cryptonote::transaction& tx)
{
  return m_pimpl->finalize(tx);
}
//----------------------------------------------------------------------------------------------------
void wallet_tx_builder::process_transaction_sent()
{
  return m_pimpl->process_transaction_sent();
}
//----------------------------------------------------------------------------------------------------
  
}
