// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <vector>
#include <iostream>
#include <sstream>

#include "include_base_utils.h"

#include "console_handler.h"

#include "p2p/net_node.h"
#include "cryptonote_core/blockchain_storage.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/cryptonote_basic_impl.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/miner.h"
#include "cryptonote_core/contract_grading.h"
#include "cryptonote_core/nulls.h"
#include "cryptonote_core/keypair.h"

#include "chaingen.h"

using namespace std;
using namespace epee;
using namespace cryptonote;

tools::ntp_time g_ntp_time(60*60);

void test_generator::get_block_chain(std::vector<block_info>& blockchain, const crypto::hash& head, size_t n) const
{
  crypto::hash curr = head;
  while (null_hash != curr && blockchain.size() < n)
  {
    auto it = m_blocks_info.find(curr);
    if (m_blocks_info.end() == it)
    {
      throw std::runtime_error("block hash wasn't found");
    }

    blockchain.push_back(it->second);
    curr = it->second.prev_id;
  }

  std::reverse(blockchain.begin(), blockchain.end());
}

void test_generator::get_last_n_block_sizes(std::vector<size_t>& block_sizes, const crypto::hash& head, size_t n) const
{
  std::vector<block_info> blockchain;
  get_block_chain(blockchain, head, n);
  BOOST_FOREACH(auto& bi, blockchain)
  {
    block_sizes.push_back(bi.block_size);
  }
}

uint64_t test_generator::get_already_generated_coins(const crypto::hash& blk_id) const
{
  auto it = m_blocks_info.find(blk_id);
  if (it == m_blocks_info.end())
    throw std::runtime_error("block hash wasn't found");

  return it->second.already_generated_coins;
}

uint64_t test_generator::get_already_generated_coins(const cryptonote::block& blk) const
{
  crypto::hash blk_hash;
  get_block_hash(blk, blk_hash);
  return get_already_generated_coins(blk_hash);
}

bool test_generator::add_block(const cryptonote::block& blk, size_t tsx_size, std::vector<size_t>& block_sizes,
                               uint64_t already_generated_coins)
{
  size_t miner_tx_size;
  try
  {
    miner_tx_size = get_object_blobsize(blk.miner_tx);
  }
  catch (...)
  {
    miner_tx_size = 0;
  }
  
  const size_t block_size = tsx_size + miner_tx_size;
  uint64_t block_reward = 0;
  crypto::hash block_id = cryptonote::null_hash;
  if (!get_block_hash(blk, block_id))
    return false;
  
  get_block_reward(misc_utils::median(block_sizes), block_size, already_generated_coins, get_block_height(blk), block_reward);
  m_blocks_info[block_id] = block_info(blk.prev_id, already_generated_coins + block_reward, block_size);
  return true;
}

bool make_miner_tx(transaction& miner_tx_res, size_t txs_size, size_t height, std::vector<size_t>& block_sizes,
                   uint64_t already_generated_coins, uint64_t fee,
                   const account_public_address& dest_address)
{
  transaction miner_tx = AUTO_VAL_INIT(miner_tx);
  size_t target_block_size = txs_size + get_object_blobsize(miner_tx);
  
  while (true)
  {
    if (!construct_miner_tx(height, misc_utils::median(block_sizes), already_generated_coins, target_block_size, fee, dest_address, miner_tx, blobdata(), 10))
      return false;
    
    size_t actual_block_size = txs_size + get_object_blobsize(miner_tx);
    if (target_block_size < actual_block_size)
    {
      target_block_size = actual_block_size;
    }
    else if (actual_block_size < target_block_size)
    {
      size_t delta = target_block_size - actual_block_size;
      miner_tx.extra.resize(miner_tx.extra.size() + delta, 0);
      actual_block_size = txs_size + get_object_blobsize(miner_tx);
      if (actual_block_size == target_block_size)
      {
        break;
      }
      else
      {
        CHECK_AND_ASSERT_MES(target_block_size < actual_block_size, false, "Unexpected block size");
        delta = actual_block_size - target_block_size;
        miner_tx.extra.resize(miner_tx.extra.size() - delta);
        actual_block_size = txs_size + get_object_blobsize(miner_tx);
        if (actual_block_size == target_block_size)
        {
          break;
        }
        else
        {
          CHECK_AND_ASSERT_MES(actual_block_size < target_block_size, false, "Unexpected block size");
          miner_tx.extra.resize(miner_tx.extra.size() + delta, 0);
          target_block_size = txs_size + get_object_blobsize(miner_tx);
        }
      }
    }
    else
    {
      break;
    }
  }
  
  miner_tx_res = miner_tx;
  return true;
}

bool test_generator::construct_block(cryptonote::block& blk, uint64_t height, const crypto::hash& prev_id,
                                     const cryptonote::account_base& miner_acc, uint64_t timestamp,
                                     uint64_t already_generated_coins,
                                     std::vector<size_t>& block_sizes, const std::list<cryptonote::transaction>& tx_list,
                                     bool ignore_overflow_check)
{
  blk.major_version = POW_BLOCK_MAJOR_VERSION;
  blk.minor_version = POW_BLOCK_MINOR_VERSION;
  blk.timestamp = timestamp;
  blk.prev_id = prev_id;

  blk.tx_hashes.reserve(tx_list.size());
  BOOST_FOREACH(const transaction &tx, tx_list)
  {
    crypto::hash tx_hash;
    get_transaction_hash(tx, tx_hash);
    blk.tx_hashes.push_back(tx_hash);
  }

  uint64_t total_fee = 0;
  size_t txs_size = 0;
  BOOST_FOREACH(auto& tx, tx_list)
  {
    uint64_t fee = 0;
    bool r = check_inputs_outputs(tx, fee);
    if (!r)
    {
      if (ignore_overflow_check)
      {
        LOG_PRINT_YELLOW("Ignoring overflow, setting fee to default", LOG_LEVEL_0);
        fee = TESTS_DEFAULT_FEE;
      }
      else
      {
        LOG_ERROR("wrong transaction passed to construct_block");
        return false;
      }
    }
    total_fee += fee;
    txs_size += get_object_blobsize(tx);
  }
  
  if (!make_miner_tx(blk.miner_tx, txs_size, height, block_sizes, already_generated_coins, total_fee, miner_acc.get_keys().m_account_address))
    return false;

  //blk.tree_root_hash = get_tx_tree_hash(blk);

  // Nonce search...
  blk.nonce = 0;
  while (!miner::find_nonce_for_given_block(blk, get_test_difficulty(), height, crypto::g_boulderhash_state))
    blk.timestamp++;

  return add_block(blk, txs_size, block_sizes, already_generated_coins);
}

bool test_generator::construct_block(cryptonote::block& blk, const cryptonote::account_base& miner_acc, uint64_t timestamp, bool ignore_overflow_check)
{
  std::vector<size_t> block_sizes;
  std::list<cryptonote::transaction> tx_list;
  return construct_block(blk, 0, null_hash, miner_acc, timestamp, 0, block_sizes, tx_list, ignore_overflow_check);
}

bool test_generator::construct_block(cryptonote::block& blk, const cryptonote::block& blk_prev,
                                     const cryptonote::account_base& miner_acc,
                                     const std::list<cryptonote::transaction>& tx_list,
                                     bool ignore_overflow_check)
{
  uint64_t height = boost::get<txin_gen>(blk_prev.miner_tx.ins().front()).height + 1;
  crypto::hash prev_id = get_block_hash(blk_prev);
  // Keep difficulty unchanged
  uint64_t timestamp = blk_prev.timestamp + cryptonote::config::difficulty_blocks_estimate_timespan();
  uint64_t already_generated_coins = get_already_generated_coins(prev_id);
  std::vector<size_t> block_sizes;
  get_last_n_block_sizes(block_sizes, prev_id, CRYPTONOTE_REWARD_BLOCKS_WINDOW);

  return construct_block(blk, height, prev_id, miner_acc, timestamp, already_generated_coins, block_sizes, tx_list, ignore_overflow_check);
}

bool test_generator::construct_block_pos(cryptonote::block& blk, const cryptonote::block& blk_prev,
                                         const cryptonote::account_base& staker_acc,
                                         const cryptonote::delegate_id_t& delegate_id,
                                         uint64_t avg_fee,
                                         const std::list<cryptonote::transaction>& tx_list)
{
  uint64_t height = boost::get<txin_gen>(blk_prev.miner_tx.ins().front()).height + 1;
  crypto::hash prev_id = get_block_hash(blk_prev);
  uint64_t timestamp = g_ntp_time.get_time(); //blk_prev.timestamp + cryptonote::config::difficulty_blocks_estimate_timespan();
  uint64_t already_generated_coins = get_already_generated_coins(prev_id);
  std::vector<size_t> block_sizes;
  get_last_n_block_sizes(block_sizes, prev_id, CRYPTONOTE_REWARD_BLOCKS_WINDOW);
  
  blk.major_version = DPOS_BLOCK_MAJOR_VERSION;
  blk.minor_version = DPOS_BLOCK_MINOR_VERSION;
  
  blk.timestamp = timestamp;
  blk.prev_id = prev_id;

  blk.tx_hashes.reserve(tx_list.size());
  BOOST_FOREACH(const transaction &tx, tx_list)
  {
    crypto::hash tx_hash;
    get_transaction_hash(tx, tx_hash);
    blk.tx_hashes.push_back(tx_hash);
  }

  size_t txs_size = 0;
  BOOST_FOREACH(auto& tx, tx_list)
  {
    txs_size += get_object_blobsize(tx);
  }

  if (!make_miner_tx(blk.miner_tx, txs_size, height, block_sizes, already_generated_coins, avg_fee, staker_acc.get_keys().m_account_address))
    return false;

  blk.nonce = 0;
  blk.signing_delegate_id = delegate_id;

  // Sign it
  if (!sign_dpos_block(blk, staker_acc))
    return false;
  
  return add_block(blk, txs_size, block_sizes, already_generated_coins);
}

bool test_generator::construct_block_manually(block& blk, const block& prev_block, const account_base& miner_acc,
                                              int actual_params, uint8_t major_ver,
                                              uint8_t minor_ver, uint64_t timestamp,
                                              const crypto::hash& prev_id, const difficulty_type& diffic,
                                              const transaction& miner_tx,
                                              const std::vector<crypto::hash>& tx_hashes,
                                              size_t txs_sizes)
{
  blk.major_version = actual_params & bf_major_ver ? major_ver : POW_BLOCK_MAJOR_VERSION;
  blk.minor_version = actual_params & bf_minor_ver ? minor_ver : POW_BLOCK_MINOR_VERSION;
  blk.timestamp     = actual_params & bf_timestamp ? timestamp : prev_block.timestamp + cryptonote::config::difficulty_blocks_estimate_timespan(); // Keep difficulty unchanged
  blk.prev_id       = actual_params & bf_prev_id   ? prev_id   : get_block_hash(prev_block);
  blk.tx_hashes     = actual_params & bf_tx_hashes ? tx_hashes : std::vector<crypto::hash>();
  blk.nonce         = 0;

  size_t height = get_block_height(prev_block) + 1;
  uint64_t already_generated_coins = get_already_generated_coins(prev_block);
  std::vector<size_t> block_sizes;
  get_last_n_block_sizes(block_sizes, get_block_hash(prev_block), CRYPTONOTE_REWARD_BLOCKS_WINDOW);
  if (actual_params & bf_miner_tx)
  {
    blk.miner_tx = miner_tx;
  }
  else
  {
    size_t current_block_size = txs_sizes + get_object_blobsize(blk.miner_tx);
    // TODO: This will work, until size of constructed block is less then CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE
    if (!construct_miner_tx(height, misc_utils::median(block_sizes), already_generated_coins, current_block_size, 0, miner_acc.get_keys().m_account_address, blk.miner_tx, blobdata(), 1))
      return false;
  }

  //blk.tree_root_hash = get_tx_tree_hash(blk);

  difficulty_type a_diffic = actual_params & bf_diffic ? diffic : get_test_difficulty();
  if (a_diffic > 1) //anything works for difficulty 1
  {
    if (!fill_nonce(blk, a_diffic, height))
      return false;
  }

  return add_block(blk, txs_sizes, block_sizes, already_generated_coins);
}

bool test_generator::construct_block_manually_tx(cryptonote::block& blk, const cryptonote::block& prev_block,
                                                 const cryptonote::account_base& miner_acc,
                                                 const std::vector<crypto::hash>& tx_hashes, size_t txs_size)
{
  return construct_block_manually(blk, prev_block, miner_acc, bf_tx_hashes, 0, 0, 0, crypto::hash(), 0, transaction(), tx_hashes, txs_size);
}


struct output_index {
  cryptonote::tx_source_entry::source_type type;
  const cryptonote::txout_target_v out;
  uint64_t amount_in;  // amount the out has initially
  uint64_t amount_out; // amount the out gives us now
  uint64_t contract_resolving; // which contract is being resolved if type is ResolveBacking/ResolveContract
  size_t blk_height; // block height
  size_t tx_no; // index of transaction in block
  size_t out_no; // index of out in transaction
  size_t idx;
  bool spent;
  const cryptonote::block *p_blk;
  const cryptonote::transaction *p_tx;

  output_index(cryptonote::tx_source_entry::source_type _type, const cryptonote::txout_target_v &_out,
               uint64_t _a, uint64_t _a_out, uint64_t _cr, size_t _h, size_t tno, size_t ono,
               const cryptonote::block *_pb, const cryptonote::transaction *_pt)
      : type(_type), out(_out), amount_in(_a), amount_out(_a_out), contract_resolving(_cr)
      , blk_height(_h), tx_no(tno), out_no(ono), idx(0)
      , spent(false), p_blk(_pb), p_tx(_pt) { }

  output_index(const output_index &other)
      : type(other.type), out(other.out), amount_in(other.amount_in), amount_out(other.amount_out)
      , contract_resolving(other.contract_resolving), blk_height(other.blk_height), tx_no(other.tx_no), out_no(other.out_no)
      , idx(other.idx), spent(other.spent), p_blk(other.p_blk), p_tx(other.p_tx) { }

  const std::string toString() const {
    std::stringstream ss;
    
    ss << "output_index{blk_height=" << blk_height
    << " tx_no=" << tx_no
    << " out_no=" << out_no
    << " amount_in=" << amount_in
    << " amount_out=" << amount_out
    << " idx=" << idx
    << " spent=" << spent
    << "}";
    
    return ss.str();
  }

  output_index& operator=(const output_index& other)
  {
    new(this) output_index(other);
    return *this;
  }
};
typedef std::pair<coin_type, uint64_t> output_key;
typedef std::map<output_key, std::vector<output_index> > map_output_idx_t;
typedef std::map<output_key, std::vector<size_t> > map_output_mine_t;
typedef pair<uint64_t, size_t>  outloc_t;

currency_map get_inputs_amount(const vector<tx_source_entry> &s)
{
  currency_map amounts;
  
  BOOST_FOREACH(const tx_source_entry &e, s)
  {
    amounts[e.cp] += e.amount_out;
  }

  return amounts;
}

bool is_tx_spendtime_unlocked(uint64_t blockchain_size, uint64_t unlock_time)
{
  if(unlock_time < CRYPTONOTE_MAX_BLOCK_NUMBER)
  {
    //interpret as block index
    if(blockchain_size - 1 + cryptonote::config::cryptonote_locked_tx_allowed_delta_blocks() >= unlock_time)
      return true;
    else
      return false;
  }else
  {
    //interpret as time
    uint64_t current_time = static_cast<uint64_t>(time(NULL));
    if(current_time + cryptonote::config::cryptonote_locked_tx_allowed_delta_seconds() >= unlock_time)
      return true;
    else
      return false;
  }
  return false;
}

bool init_output_indices(bool respect_unlock_time,
                         map_output_idx_t& outs, map_output_mine_t& outs_mine,
                         const std::vector<cryptonote::block>& blockchain, const map_hash2tx_isregular_t& mtx,
                         const cryptonote::account_base& from, cryptonote::coin_type cp)
{
  uint64_t blockchain_size = get_block_height(blockchain.back());
  
  // list out all transactions in order
  struct tx_entry {
    const block* pblk;
    const transaction* ptx;
    size_t height;
    size_t tx_no;
    
    tx_entry(const block* pblk_in, const transaction* ptx_in, size_t height_in, size_t tx_no_in)
        : pblk(pblk_in), ptx(ptx_in), height(height_in), tx_no(tx_no_in) { }
  };
  
  size_t blockchain_height = 0;
  
  vector<tx_entry> vtx;
  BOOST_FOREACH (const block& blk, blockchain) {
    size_t height = boost::get<txin_gen>(*blk.miner_tx.ins().begin()).height;
    
    size_t tx_no = 0;
    vtx.push_back(tx_entry(&blk, &blk.miner_tx, height, tx_no++));
  
    BOOST_FOREACH(const crypto::hash &h, blk.tx_hashes) {
      const map_hash2tx_isregular_t::const_iterator cit = mtx.find(h);
      if (mtx.end() == cit)
        throw std::runtime_error("block contains an unknown tx hash");
      
      vtx.push_back(tx_entry(&blk, cit->second.first, height, tx_no++));
    }
    
    blockchain_height = std::max(blockchain_height, height);
  }
  
  // process all contracts first
  std::unordered_map<uint64_t, blockchain_storage::contract_info> contract_info;
  BOOST_FOREACH (const auto& ent, vtx) {
    const transaction& tx = *ent.ptx;
    
    for (size_t j = 0; j < tx.ins().size(); ++j) {
      const auto& txin = tx.ins()[j];
      
      if (txin.type() == typeid(txin_create_contract)) {
        const auto& inp = boost::get<txin_create_contract>(txin);
        
        blockchain_storage::contract_info ci;
        ci.contract = inp.contract;
        ci.fee_scale = inp.fee_scale;
        ci.expiry_block = inp.expiry_block;
        ci.default_grade = inp.default_grade;
        ci.is_graded = false;
        ci.grade = 0;
        contract_info[inp.contract] = ci;
        
        LOG_PRINT_CYAN("Found contract " << inp.contract, LOG_LEVEL_0);
      }
      
      if (txin.type() == typeid(txin_grade_contract)) {
        const auto& inp = boost::get<txin_grade_contract>(txin);
        auto& ci = contract_info[inp.contract];
        ci.is_graded = true;
        ci.grade = inp.grade;
        LOG_PRINT_CYAN("Found txin grading contract "
                       << inp.contract << " at " << print_grade_scale(inp.grade), LOG_LEVEL_0);
      }
    }
  }
  
  BOOST_FOREACH (const auto& ent, vtx) {
    const transaction &tx = *ent.ptx;
    
    // Skip unspendable txs
    if (respect_unlock_time && !is_tx_spendtime_unlocked(blockchain_size + 1, tx.unlock_time))
    {
      /*LOG_PRINT_YELLOW("Skipping tx with tx.unlock_time=" << tx.unlock_time
                       << ", blockchain.size()=" << blockchain.size()
                       << ", last block height=" << blockchain_size
                       << ", time=" << time(NULL),
                       LOG_LEVEL_0);*/
      continue;
    }
    
    for (size_t out_no = 0; out_no < tx.outs().size(); ++out_no) {
      const tx_out &out = tx.outs()[out_no];
      const auto& tx_out_cp = tx.out_cp(out_no);
      
      auto type = tx_source_entry::InToKey;
      uint64_t amount_in = 0, amount_out = 0, contract_resolving = CURRENCY_N_A;
      const txout_to_key* p_outp = NULL;
      
      if (tx_out_cp != cp)
      {
        // maybe it's a contract backed by the currency we want
        auto r = contract_info.find(tx_out_cp.currency);
        if (r == contract_info.end() || tx_out_cp.backed_by_currency != cp.currency)
        {
          LOG_PRINT_YELLOW("other currency " << tx_out_cp.currency << (r == contract_info.end() ? " was not a " : " was a ")
                           << "contract, backed_by=" << tx_out_cp.backed_by_currency << " vs. us looking for " << cp,
                           LOG_LEVEL_0);
          continue;
        }

        auto& ci = r->second;
        uint64_t grade = 0;
        uint64_t fee_scale = 0;
        //bool expired = false;
        if (ci.is_graded)
        {
          grade = ci.grade;
          fee_scale = ci.fee_scale;
        }
        else if (blockchain_height >= ci.expiry_block)
        {
          grade = ci.default_grade;
          fee_scale = 0;
          //expired = true;
        }
        else
        {
          LOG_PRINT_YELLOW("found unexpired, ungraded backing/contract coin of " << tx_out_cp,
                           LOG_LEVEL_0);
          continue;
        }
        
        CHECK_AND_ASSERT_MES(tx_out_cp.contract_type == BackingCoin ||
                             tx_out_cp.contract_type == ContractCoin, false,
                             "CONSISTENCY FAIL: found an out w/ a backing but not a contract type");
        
        CHECK_AND_ASSERT_MES(cp.backed_by_currency == BACKED_BY_N_A, false,
                             "CONSISTENCY FAIL: found a contract backed by a contract");

        CHECK_AND_ASSERT_MES(out.target.type() == typeid(txout_to_key), false,
                             "CONSISTENCY FAIL: found a contract that wasn't a txout_to_key");

        p_outp = &boost::get<txout_to_key>(out.target);
        type = tx_out_cp.contract_type == ContractCoin ? tx_source_entry::ResolveContract : tx_source_entry::ResolveBacking;
        amount_in = out.amount;
        amount_out = (tx_out_cp.contract_type == ContractCoin ? grade_contract_amount : grade_backing_amount)(out.amount, grade, fee_scale);
        if (amount_out == 0)
        {
          LOG_PRINT_YELLOW("Not using" << (tx_out_cp.contract_type == ContractCoin ? " contract " : " backing ")
                           << "coins of " << tx_out_cp.currency << " that would grade to 0",
                           LOG_LEVEL_0);
          continue;
        }
        
        contract_resolving = tx_out_cp.currency;
        /*LOG_PRINT_CYAN(out.amount << (tx_out_cp.contract_type == ContractCoin ? " contract " : " backing ")
                       << "coins of " << tx_out_cp.currency
                       << (expired ? " expired " : " graded ") << "at "
                       << print_grade_scale(grade) << " with fee " << print_grade_scale(fee_scale)
                       << " can spend " << amount_out, LOG_LEVEL_0);*/
      }
      else
      {
        // currency matches, spend it
        if (out.target.type() == typeid(txout_to_key))
        {
          p_outp = &boost::get<txout_to_key>(out.target);
          type = tx_source_entry::InToKey;
          amount_in = amount_out = out.amount;
          contract_resolving = CURRENCY_N_A;
          //LOG_PRINT_CYAN("Found " << out.amount << " of " << cp, LOG_LEVEL_0);
        }
        else
        {
          LOG_ERROR("Unknown out target type: " << out.target.which());
          continue;
        }
      }
      
      output_index oi(type, out.target, amount_in, amount_out, contract_resolving,
                      ent.height, ent.tx_no, out_no, ent.pblk, ent.ptx);
      
      auto key = std::make_pair(tx_out_cp, out.amount);
      outs[key].push_back(oi);
      size_t tx_global_idx = outs[key].size() - 1;
      outs[key][tx_global_idx].idx = tx_global_idx;
      
      // Is out to me?
      if (is_out_to_acc(from.get_keys(), *p_outp, get_tx_pub_key_from_extra(tx), out_no)) {
        //LOG_PRINT_YELLOW("found match to 'me' with out_cp " << tx.out_cp(j) << ", amount " << out.amount, LOG_LEVEL_0);
        outs_mine[key].push_back(tx_global_idx);
        /*LOG_PRINT_L0("Added an out for " << out.amount << ", tx.unlock_time=" << tx.unlock_time
                     << ", blockchain.size()=" << blockchain.size()
                     << ", last block height=" << blockchain_size << ", time=" << time(NULL));*/
      }
    }
  }
  
  return true;
}

bool init_spent_output_indices(map_output_idx_t& outs, map_output_mine_t& outs_mine,
                               const std::vector<cryptonote::block>& blockchain, const map_hash2tx_isregular_t& mtx,
                               const cryptonote::account_base& from)
{
  BOOST_FOREACH (const map_output_mine_t::value_type &o, outs_mine) {
    for (size_t i = 0; i < o.second.size(); ++i) {
      output_index &oi = outs[o.first][o.second[i]];
      
      // construct key image for this output
      crypto::key_image img;
      keypair in_ephemeral;
      generate_key_image_helper(from.get_keys(), get_tx_pub_key_from_extra(*oi.p_tx), oi.out_no, in_ephemeral, img);
      
      // lookup for this key image in the events vector
      BOOST_FOREACH(auto& tx_pair, mtx) {
        const transaction& tx = *tx_pair.second.first;
        if (!tx_pair.second.second) {
          // don't count it as spent
          continue;
        }
        size_t vini = 0;
        BOOST_FOREACH(const txin_v &in, tx.ins()) {
          if (in.type() == typeid(txin_to_key)) {
            const txin_to_key &itk = boost::get<txin_to_key>(in);
            if (itk.k_image == img) {
              oi.spent = true;
            }
          }
          vini += 1;
        }
      }
    }
  }
  
  return true;
}

bool fill_output_entries(std::vector<output_index>& out_indices, size_t sender_out, size_t nmix, size_t& real_entry_idx,
                         std::vector<tx_source_entry::output_entry>& output_entries)
{
  if (out_indices.size() <= nmix)
  {
    LOG_PRINT_CYAN("fill_output_entries: not enough to mix, nmix=" << nmix << ", indices.size()=" << out_indices.size(),
                   LOG_LEVEL_0);
    return false;
  }

  bool sender_out_found = false;
  size_t rest = nmix;
  for (size_t i = 0; i < out_indices.size() && (0 < rest || !sender_out_found); ++i)
  {
    const output_index& oi = out_indices[i];
    if (oi.spent)
      continue;

    bool append = false;
    if (i == sender_out)
    {
      append = true;
      sender_out_found = true;
      real_entry_idx = output_entries.size();
    }
    else if (0 < rest)
    {
      --rest;
      append = true;
    }

    if (append)
    {
      CHECK_AND_ASSERT_MES(oi.out.type() == typeid(txout_to_key), false, "internal logic fail");
      const crypto::public_key& key = boost::get<txout_to_key>(oi.out).key;
      output_entries.push_back(tx_source_entry::output_entry(oi.idx, key));
    }
  }

  return 0 == rest && sender_out_found;
}

bool fill_tx_sources(std::vector<tx_source_entry>& sources, const std::vector<test_event_entry>& events,
                     const block& blk_head, const cryptonote::account_base& from, uint64_t amount, size_t nmix,
                     cryptonote::coin_type cp, bool ignore_unlock_times)
{
  map_output_idx_t outs;
  map_output_mine_t outs_mine;
  
  std::vector<cryptonote::block> blockchain;
  map_hash2tx_isregular_t mtx;
  if (!find_block_chain(events, blockchain, mtx, get_block_hash(blk_head)))
  {
    LOG_ERROR("fill_tx_sources: could not find_block_chain");
    return false;
  }
  
  if (!init_output_indices(!ignore_unlock_times, outs, outs_mine, blockchain, mtx, from, cp))
  {
    LOG_ERROR("fill_tx_sources: could not init_output_indices");
    return false;
  }
  
  if (!init_spent_output_indices(outs, outs_mine, blockchain, mtx, from))
  {
    LOG_ERROR("fill_tx_sources: could not init_spent_output_indices");
    return false;
  }
  
  uint64_t sources_amount = 0;
  bool sources_found = false;
  BOOST_REVERSE_FOREACH(const map_output_mine_t::value_type o, outs_mine)
  {
    for (size_t i = 0; i < o.second.size() && !sources_found; ++i)
    {
      size_t sender_out = o.second[i];
      const output_index& oi = outs[o.first][sender_out];
      if (oi.spent)
      {
        //LOG_PRINT_CYAN("fill_tx_sources: skipping spent output_index", LOG_LEVEL_0);
        continue;
      }
      
      cryptonote::tx_source_entry ts;
      ts.type = oi.type;
      ts.cp = cp;
      ts.amount_in = oi.amount_in;
      ts.amount_out = oi.amount_out;
      ts.real_output_in_tx_index = oi.out_no;
      ts.real_out_tx_key = get_tx_pub_key_from_extra(*oi.p_tx); // incoming tx public key
      ts.contract_resolving = oi.contract_resolving;
      size_t realOutput;
      if (!fill_output_entries(outs[o.first], sender_out, nmix, realOutput, ts.outputs))
      {
        LOG_PRINT_CYAN("fill_tx_sources: couldn't fill_output_entries", LOG_LEVEL_0);
        continue;
      }
      
      ts.real_output = realOutput;
      
      sources.push_back(ts);
      
      sources_amount += ts.amount_out;
      sources_found = amount <= sources_amount;
      LOG_PRINT_CYAN("found " << sources_amount << "/" << amount << " of " << cp << " so far", LOG_LEVEL_0);
    }
    
    if (sources_found)
      break;
  }
  
  if (!sources_found)
    LOG_ERROR("fill_tx_sources: sources were not found for cp " << cp);
  
  return sources_found;
}

bool fill_tx_destination(tx_destination_entry &de, const cryptonote::account_base &to, uint64_t amount, cryptonote::coin_type cp) {
  de.addr = to.get_keys().m_account_address;
  de.amount = amount;
  de.cp = cp;
  return true;
}

bool fill_tx_sources_and_destinations(const std::vector<test_event_entry>& events, const block& blk_head,
                                      const cryptonote::account_base& from, const cryptonote::account_base& to,
                                      uint64_t amount, uint64_t fee, size_t nmix, std::vector<tx_source_entry>& sources,
                                      std::vector<tx_destination_entry>& destinations, cryptonote::coin_type cp,
                                      bool ignore_unlock_times)
{
  if (cp != CP_XPB && fee > 0)
  {
    LOG_ERROR("fill_tx_sources_and_destinations with sub-currency and fee not yet supported");
    return false;
  }
  
  sources.clear();
  destinations.clear();

  if (!fill_tx_sources(sources, events, blk_head, from, amount + fee, nmix, cp, ignore_unlock_times))
  {
    LOG_ERROR("couldn't fill transaction sources");
    return false;
  }

  tx_destination_entry de;
  if (!fill_tx_destination(de, to, amount, cp))
  {
    LOG_ERROR("couldn't fill transaction destination");
    return false;
  }
  destinations.push_back(de);

  tx_destination_entry de_change;
  uint64_t cache_back = get_inputs_amount(sources)[cp] - (amount + fee);
  if (0 < cache_back)
  {
    if (!fill_tx_destination(de_change, from, cache_back, cp))
    {
      LOG_ERROR("couldn't fill transaction cache back destination");
      return false;
    }
    destinations.push_back(de_change);
  }
  
  return true;
}

bool fill_tx_sources_and_destinations(const std::vector<test_event_entry>& events, const block& blk_head,
                                      const cryptonote::account_base& from, const cryptonote::account_base& to,
                                      uint64_t amount, uint64_t fee, size_t nmix, std::vector<tx_source_entry>& sources,
                                      std::vector<tx_destination_entry>& destinations, uint64_t currency,
                                      bool ignore_unlock_times)
{
  return fill_tx_sources_and_destinations(events, blk_head, from, to, amount, fee, nmix, sources, destinations, cryptonote::coin_type(currency, cryptonote::NotContract, BACKED_BY_N_A), ignore_unlock_times);
}


bool fill_nonce(cryptonote::block& blk, const difficulty_type& diffic, uint64_t height)
{
  blk.nonce = 0;
  try
  {
    while (!miner::find_nonce_for_given_block(blk, diffic, height, crypto::g_boulderhash_state))
      blk.timestamp++;
  }
  catch (...)
  {
    return false;
  }
  return true;
}

bool construct_miner_tx_manually(size_t height, uint64_t already_generated_coins,
                                 const account_public_address& miner_address, transaction& tx, uint64_t fee,
                                 keypair* p_txkey/* = 0*/)
{
  tx.version = VANILLA_TRANSACTION_VERSION;

  keypair txkey;
  txkey = keypair::generate();
  add_tx_pub_key_to_extra(tx, txkey.pub);

  if (0 != p_txkey)
    *p_txkey = txkey;

  txin_gen in;
  in.height = height;
  tx.add_in(in, CP_XPB);

  // This will work, until size of constructed block is less then CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE
  uint64_t block_reward;
  if (!get_block_reward(0, 0, already_generated_coins, height, block_reward))
  {
    LOG_PRINT_L0("Block is too big");
    return false;
  }
  block_reward += fee;

  crypto::key_derivation derivation;
  crypto::public_key out_eph_public_key;
  crypto::generate_key_derivation(miner_address.m_view_public_key, txkey.sec, derivation);
  crypto::derive_public_key(derivation, 0, miner_address.m_spend_public_key, out_eph_public_key);

  tx_out out;
  out.amount = block_reward;
  out.target = txout_to_key(out_eph_public_key);
  tx.add_out(out, CP_XPB);

  tx.unlock_time = height + CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW;

  return true;
}

transaction construct_tx_with_fee(std::vector<test_event_entry>& events, const block& blk_head,
                                  const account_base& acc_from, const account_base& acc_to, uint64_t amount, uint64_t fee)
{
  transaction tx;
  if (!construct_tx_to_key(events, tx, blk_head, acc_from, acc_to, amount, fee, 0, CP_XPB))
    throw std::runtime_error("Couldn't construct_tx_to_key");
  events.push_back(tx);
  return tx;
}

uint64_t get_balance(const cryptonote::account_base& addr, const std::vector<cryptonote::block>& blockchain,
                     const map_hash2tx_isregular_t& mtx)
{
  uint64_t res = 0;
  map_output_idx_t outs;
  map_output_mine_t outs_mine;
  
  map_hash2tx_isregular_t confirmed_txs;
  get_confirmed_txs(blockchain, mtx, confirmed_txs);
  
  if (!init_output_indices(false, outs, outs_mine, blockchain, confirmed_txs, addr, CP_XPB))
    return false;
  
  if (!init_spent_output_indices(outs, outs_mine, blockchain, confirmed_txs, addr))
    return false;
  
  BOOST_FOREACH (const map_output_mine_t::value_type &o, outs_mine) {
    for (size_t i = 0; i < o.second.size(); ++i) {
      if (outs[o.first][o.second[i]].spent)
        continue;
      
      res += outs[o.first][o.second[i]].amount_out;
    }
  }
  
  return res;
}

uint64_t get_balance(const std::vector<test_event_entry>& events, const cryptonote::account_base& addr,
                     const cryptonote::block& head)
{
  std::vector<cryptonote::block> chain;
  map_hash2tx_isregular_t mtx;
  if (!find_block_chain(events, chain, mtx, get_block_hash(head)))
    throw test_gen_error("Could not find_block_chain");
  
  return get_balance(addr, chain, mtx);
}

void get_confirmed_txs(const std::vector<cryptonote::block>& blockchain, const map_hash2tx_isregular_t& mtx, map_hash2tx_isregular_t& confirmed_txs)
{
  std::unordered_set<crypto::hash> confirmed_hashes;
  BOOST_FOREACH(const block& blk, blockchain)
  {
    BOOST_FOREACH(const crypto::hash& tx_hash, blk.tx_hashes)
    {
      confirmed_hashes.insert(tx_hash);
    }
  }

  BOOST_FOREACH(const auto& tx_pair, mtx)
  {
    if (0 != confirmed_hashes.count(tx_pair.first))
    {
      confirmed_txs.insert(tx_pair);
    }
  }
}

bool find_block_chain(const std::vector<test_event_entry>& events, std::vector<cryptonote::block>& blockchain,
                      map_hash2tx_isregular_t& mtx, const crypto::hash& head)
{
  std::unordered_map<crypto::hash, const block*> block_index;
  bool next_tx_invalid = false;
  bool next_tx_regular = true;
  BOOST_FOREACH(const test_event_entry& ev, events)
  {
    if (typeid(block) == ev.type())
    {
      const block* blk = &boost::get<block>(ev);
      block_index[get_block_hash(*blk)] = blk;
    }
    else if (typeid(transaction) == ev.type())
    {
      if (next_tx_invalid)
      {
        // don't include it in 'mtx':
        // - it can't be in a block because it should be invalid
        // - it can't spend anything because it should be invalid
      }
      else
      {
        const transaction& tx = boost::get<transaction>(ev);
        mtx[get_transaction_hash(tx)] = std::make_pair(&tx, next_tx_regular);
      }
    }
    
    next_tx_invalid = false;
    next_tx_regular = true;
    if (typeid(callback_entry) == ev.type())
    {
      const auto& cb = boost::get<callback_entry>(ev);
      if (cb.callback_name == "mark_invalid_tx")
      {
        next_tx_invalid = true;
      }
    }
    else if (typeid(dont_mark_spent_tx) == ev.type())
    {
      next_tx_regular = false;
    }
  }
  
  bool b_success = false;
  crypto::hash id = head;
  for (auto it = block_index.find(id); block_index.end() != it; it = block_index.find(id))
  {
    blockchain.push_back(*it->second);
    id = it->second->prev_id;
    if (null_hash == id)
    {
      b_success = true;
      break;
    }
  }
  reverse(blockchain.begin(), blockchain.end());
  
  return b_success;
}

//--------------------------------------------------------------------------

bool replay_events_through_core(core_t& cr, const std::vector<test_event_entry>& events, test_chain_unit_base& validator)
{
  TRY_ENTRY();

  //init core here

  CHECK_AND_ASSERT_MES(typeid(cryptonote::block) == events[0].type(), false, "First event must be genesis block creation");
  cr.set_genesis_block(boost::get<cryptonote::block>(events[0]));

  bool r = true;
  push_core_event_visitor visitor(cr, events, validator);
  for(size_t i = 1; i < events.size() && r; ++i)
  {
    visitor.event_index(i);
    r = boost::apply_visitor(visitor, events[i]);
  }

  return r;

  CATCH_ENTRY_L0("replay_events_through_core", false);
}

//--------------------------------------------------------------------------

bool do_replay_events(std::vector<test_event_entry>& events, test_chain_unit_base& validator)
{
  boost::program_options::options_description desc("Allowed options");
  core_t::init_options(desc);
  command_line::add_arg(desc, command_line::arg_data_dir);
  boost::program_options::variables_map vm;
  bool r = command_line::handle_error_helper(desc, [&]()
  {
    boost::program_options::store(boost::program_options::basic_parsed_options<char>(&desc), vm);
    boost::program_options::notify(vm);
    return true;
  });
  if (!r)
    return false;
  
  // use local config folder
  vm.insert(std::make_pair(command_line::arg_data_dir.name,
                           boost::program_options::variable_value(std::string("coretests_data"), false)));

  cryptonote::cryptonote_protocol_stub pr; //TODO: stub only for this kind of test, make real validation of relayed objects
  core_t c(&pr, g_ntp_time);
  if (!c.init(vm))
  {
    std::cout << concolor::magenta << "Failed to init core" << concolor::normal << std::endl;
    return false;
  }
  reset_test_defaults();
  return replay_events_through_core(c, events, validator);
}

//--------------------------------------------------------------------------

bool do_replay_file(const std::string& filename, test_chain_unit_base& validator)
{
  std::vector<test_event_entry> events;
  if (!tools::unserialize_obj_from_file(events, filename))
  {
    std::cout << concolor::magenta << "Failed to deserialize data from file: " << filename << concolor::normal << std::endl;
    return false;
  }
  return do_replay_events(events, validator);
}

//--------------------------------------------------------------------------

cryptonote::account_base generate_account()
{
  account_base account;
  account.generate();
  return account;
}

std::vector<cryptonote::account_base> generate_accounts(size_t n)
{
  std::vector<cryptonote::account_base> res;
  for (size_t i=0; i < n; i++)
    res.push_back(generate_account());
  return res;
}

std::list<cryptonote::transaction> start_tx_list(const cryptonote::transaction& tx)
{
  LOG_PRINT_L0("start_tx_list");
  std::list<cryptonote::transaction> res;
  res.push_back(tx);
  return res;
}

cryptonote::block make_genesis_block(test_generator& generator, std::vector<test_event_entry>& events,
                                     const cryptonote::account_base& miner_account, uint64_t timestamp)
{
  LOG_PRINT_L0("make_genesis_block");
  cryptonote::block blk;
  if (!generator.construct_block(blk, miner_account, timestamp))
  {
    throw test_gen_error("Failed to make_genesis_block");
  }
  events.push_back(blk);
  return blk;
}

cryptonote::block make_next_block(test_generator& generator, std::vector<test_event_entry>& events,
                                  const cryptonote::block& block_prev,
                                  const cryptonote::account_base& miner_account,
                                  const std::list<cryptonote::transaction>& tx_list)
{
  LOG_PRINT_L0("make_next_block");
  cryptonote::block blk;
  if (!generator.construct_block(blk, block_prev, miner_account, tx_list))
  {
    throw test_gen_error("Failed to make_next_block");
  }
  events.push_back(blk);
  return blk;
}

uint64_t average_past_block_fees(const std::vector<cryptonote::block>& blockchain,
                                 const map_hash2tx_isregular_t& mtx,
                                 uint64_t for_block_height)
{
  if (for_block_height == 0)
    return 0;
  
  // get block at height 1 below the target height
  // average all fees from that block inclusive until first block that has timestamp more than 1 day ago
  
  if (for_block_height - 1 >= blockchain.size()) {
    LOG_ERROR("Invalid for_block_height " << for_block_height << ", blockchain.size()= " << blockchain.size());
    throw test_gen_error("Invalid for_block_height in average_past_block_fees");
  }
  
  uint64_t end_timestamp = blockchain[for_block_height - 1].timestamp;
  size_t used_blocks = 0;
  uint64_t fee_summaries = 0;
  
  for (int height = for_block_height - 1; height >= 0; height--)
  {
    const auto& bl = blockchain[height];
    
    if (bl.timestamp < end_timestamp - 86400)
      break;
    
    BOOST_FOREACH(const auto& tx_id, bl.tx_hashes) {
      auto it = mtx.find(tx_id);
      if (it == mtx.end()) {
        LOG_ERROR("Inconsistency, block in chain has unknown transaction " << tx_id);
        throw test_gen_error("Inconsistency in blockchain");
      }
      fee_summaries += get_tx_fee(*it->second.first);
    }
    used_blocks++;
  }
  
  if (used_blocks == 0)
    return 0;
  
  return fee_summaries / used_blocks;
}

cryptonote::block make_next_pos_block(test_generator& generator, std::vector<test_event_entry>& events,
                                      const cryptonote::block& block_prev,
                                      const cryptonote::account_base& staker_account,
                                      const cryptonote::delegate_id_t& staker_id,
                                      const std::list<cryptonote::transaction>& tx_list)
{
  LOG_PRINT_L0("make_next_pos_block");
  
  g_ntp_time.apply_manual_delta(CRYPTONOTE_DPOS_BLOCK_MINIMUM_BLOCK_SPACING);
  
  std::vector<cryptonote::block> blockchain;
  map_hash2tx_isregular_t mtx;
  if (!find_block_chain(events, blockchain, mtx, get_block_hash(block_prev)))
  {
    throw test_gen_error("make_next_pos_block: could not find_block_chain");
  }
  uint64_t average_fee = average_past_block_fees(blockchain, mtx, blockchain.size());
  LOG_PRINT_L0("make_next_pos_block, fee=" << average_fee << ", prev_height=" << get_block_height(block_prev));
  
  cryptonote::block blk;
  if (!generator.construct_block_pos(blk, block_prev, staker_account, staker_id, average_fee, tx_list))
  {
    throw test_gen_error("Failed to make_next_pos_block");
  }
  events.push_back(blk);
  return blk;
}

cryptonote::block make_next_pos_block(test_generator& generator, std::vector<test_event_entry>& events,
                                      const cryptonote::block& block_prev,
                                      const cryptonote::account_base& staker_account,
                                      const cryptonote::delegate_id_t& staker_id,
                                      const cryptonote::transaction& tx1)
{
  return make_next_pos_block(generator, events, block_prev, staker_account, staker_id, start_tx_list(tx1));
}

cryptonote::block make_next_pos_blocks(test_generator& generator, std::vector<test_event_entry>& events,
                                       const cryptonote::block& block_prev,
                                       const std::vector<cryptonote::account_base>& staker_account,
                                       const std::vector<cryptonote::delegate_id_t>& staker_id)
{
  if (staker_account.size() != staker_id.size())
    throw test_gen_error("make_next_pos_blocks called w/ different # of accounts vs ids");
  
  block last = block_prev;
  for (size_t i=0; i < staker_account.size(); i++)
  {
    last = make_next_pos_block(generator, events, last, staker_account[i], staker_id[i]);
  }
  return last;
}

cryptonote::block make_next_block(test_generator& generator, std::vector<test_event_entry>& events,
                                  const cryptonote::block& block_prev,
                                  const cryptonote::account_base& miner_account,
                                  const cryptonote::transaction& tx1)
{
  return make_next_block(generator, events, block_prev, miner_account, start_tx_list(tx1));
}

cryptonote::block rewind_blocks(test_generator& generator, std::vector<test_event_entry>& events,
                                const cryptonote::block& block_prev,
                                const std::vector<cryptonote::account_base>& miner_accounts,
                                size_t n)
{
  LOG_PRINT_L0("rewind_blocks");
  cryptonote::block blk_last = block_prev;
  for (size_t i = 0; i < n; ++i)
  {
    blk_last = make_next_block(generator, events, blk_last, miner_accounts[i % miner_accounts.size()]);
  }
  return blk_last;
}

cryptonote::block rewind_blocks(test_generator& generator, std::vector<test_event_entry>& events,
                                const cryptonote::block& block_prev,
                                const cryptonote::account_base& miner_account,
                                size_t n)
{
  std::vector<cryptonote::account_base> no_initializer_lists;
  no_initializer_lists.push_back(miner_account);
  return rewind_blocks(generator, events, block_prev, no_initializer_lists, n);
}

cryptonote::transaction make_tx_send(std::vector<test_event_entry>& events,
                                     const cryptonote::account_base& from, const cryptonote::account_base& to,
                                     uint64_t amount, const cryptonote::block& head,
                                     uint64_t fee, const coin_type& cp,
                                     uint64_t nmix)
{
  cryptonote::transaction result;
  if (!construct_tx_to_key(events, result, head, from, to, amount, fee, nmix, cp, tools::identity()))
  {
    throw test_gen_error("Failed to make_tx_send");
  }
  events.push_back(result);
  return result;
}

void do_callback_func(std::vector<test_event_entry>& events, const verify_callback_func& cb)
{
  callback_entry_func c;
  c.cb = cb;
  events.push_back(c);
}

void set_dpos_switch_block(std::vector<test_event_entry>& events, uint64_t block)
{
  // set event so it switches during replay
  events.push_back(set_dpos_switch_block_struct(block, -1));
  // change global so block's are generated properly
  cryptonote::config::dpos_switch_block = block;
}

void set_dpos_registration_start_block(std::vector<test_event_entry>& events, uint64_t block)
{
  // set event so it switches during replay
  events.push_back(set_dpos_switch_block_struct(-1, block));
  // change global so block's are generated properly
  cryptonote::config::dpos_registration_start_block = block;
}

void set_default_fee(std::vector<test_event_entry>& events, uint64_t default_fee)
{
  DEFAULT_FEE = default_fee;
  do_callback_func(events, [=](core_t& c, size_t ev_index) {
    DEFAULT_FEE = default_fee;
    return true;
  });
}

void do_callback(std::vector<test_event_entry>& events, const std::string& cb_name)
{
  callback_entry c;
  c.callback_name = cb_name;
  events.push_back(c);
}

void do_debug_mark(std::vector<test_event_entry>& events, const std::string& msg)
{
  debug_mark d;
  d.message = msg;
  events.push_back(d);
}

void do_register_delegate_account(std::vector<test_event_entry>& events, cryptonote::delegate_id_t delegate_id,
                                  const cryptonote::account_base& acct)
{
  register_delegate_account rda;
  rda.delegate_id = delegate_id;
  rda.acct = acct;
  events.push_back(rda);
}

void reset_test_defaults()
{
  cryptonote::config::dpos_registration_start_block = 83120;
  cryptonote::config::dpos_switch_block = 85300;
  crypto::g_hash_ops_small_boulderhash = true;
  cryptonote::config::do_boulderhash = true;
  cryptonote::config::no_reward_ramp = true;
  cryptonote::config::test_serialize_unserialize_block = true;
  cryptonote::config::dpos_num_delegates = 5;
  DEFAULT_FEE = 0; // add no-fee txs, lot of tests use them
}

