// Copyright (c) 2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <algorithm>
#include <numeric>
#include <sstream>

#include <boost/utility/value_init.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/foreach.hpp>

#include "include_base_utils.h"
#include "misc_language.h"
#include "profile_tools.h"
#include "net/http_client.h"
#include "storages/http_abstract_invoke.h"

#include "common/boost_serialization_helper.h"
#include "common/stl-util.h"
#include "common/functional.h"
#include "crypto/crypto.h"
#include "crypto/chacha8.h"
#include "crypto/crypto_basic_impl.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/cryptonote_basic_impl.h"
#include "cryptonote_core/nulls.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "serialization/binary_utils.h"

#include "wallet2.h"
#include "wallet_errors.h"
#include "split_strategies.h"
#include "boost_serialization.h"
#include "wallet_tx_builder.h"

using namespace epee;
using namespace cryptonote;

namespace
{
void do_prepare_file_names(const std::string& file_path, std::string& keys_file, std::string& wallet_file, std::string& known_transfers_file, std::string& currency_keys_file, std::string& votes_info_file)
{
  keys_file = file_path;
  wallet_file = file_path;
  boost::system::error_code e;
  if(string_tools::get_extension(keys_file) == "keys")
  {//provided keys file name
    wallet_file = string_tools::cut_off_extension(wallet_file);
  }else
  {//provided wallet file name
    keys_file += ".keys";
  }
  known_transfers_file = wallet_file + ".known_transfers";
  currency_keys_file = wallet_file + ".currency_keys";
  votes_info_file = wallet_file + ".voting";
}
} //namespace

namespace tools
{
namespace detail
{
  struct keys_file_data
  {
    crypto::chacha8_iv iv;
    std::string account_data;

    BEGIN_SERIALIZE_OBJECT()
      FIELD(iv)
      FIELD(account_data)
    END_SERIALIZE()
  };
}
}

namespace tools
{
//----------------------------------------------------------------------------------------------------
uint64_t wallet2_voting_batch::amount(const tools::wallet2 &wallet) const
{
  uint64_t res = 0;
  BOOST_FOREACH(size_t i, m_transfer_indices)
  {
    THROW_WALLET_EXCEPTION_IF(i >= wallet.m_transfers.size(), error::wallet_internal_error,
                              "invalid transfer index in wallet2_voting_batch");
    const auto& td = wallet.m_transfers[i];
    if (td.m_spent)
      continue;
    
    res += td.amount();
  }
  return res;
}
//----------------------------------------------------------------------------------------------------
bool wallet2_voting_batch::spent(const tools::wallet2 &wallet) const
{
  const auto& transfers = wallet.m_transfers;
  return std::all_of(m_transfer_indices.begin(), m_transfer_indices.end(),
                     [&](size_t idx) {
                       THROW_WALLET_EXCEPTION_IF(idx >= transfers.size(), error::wallet_internal_error,
                                                 "invalid transfer index in wallet2_voting_batch");
                       return transfers[idx].m_spent;
                     });
}
//----------------------------------------------------------------------------------------------------
wallet2::wallet2(const wallet2&)
    : m_run(true), m_callback(0), m_conn_timeout(WALLET_DEFAULT_RCP_CONNECTION_TIMEOUT)
    , m_phttp_client(new epee::net_utils::http::http_simple_client())
    , m_read_only(false)
    , m_account_public_address(cryptonote::null_public_address)
    , m_voting_user_delegates(false)
{
}
//----------------------------------------------------------------------------------------------------
wallet2::wallet2(bool read_only)
    : m_run(true), m_callback(0), m_conn_timeout(WALLET_DEFAULT_RCP_CONNECTION_TIMEOUT)
    , m_phttp_client(new epee::net_utils::http::http_simple_client())
    , m_read_only(read_only)
    , m_account_public_address(cryptonote::null_public_address)
    , m_voting_user_delegates(false)
{
}
//----------------------------------------------------------------------------------------------------
wallet2::~wallet2()
{
  delete m_phttp_client; m_phttp_client = NULL;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::is_mine(const cryptonote::account_public_address& account_public_address) const
{
  return account_public_address == m_account_public_address;
}
//----------------------------------------------------------------------------------------------------
void wallet2::init(const std::string& daemon_address, uint64_t upper_transaction_size_limit)
{
  m_upper_transaction_size_limit = upper_transaction_size_limit;
  m_daemon_address = daemon_address;
}
//----------------------------------------------------------------------------------------------------
void wallet2::process_new_transaction(const cryptonote::transaction& tx, uint64_t height, bool is_miner_tx)
{
  process_unconfirmed(tx);
  std::vector<size_t> outs;
  uint64_t tx_money_got_in_outs = 0;

  std::vector<tx_extra_field> tx_extra_fields;
  if(!parse_tx_extra(tx.extra, tx_extra_fields))
  {
    // Extra may only be partially parsed, it's OK if tx_extra_fields contains public key
    LOG_PRINT_L0("Transaction extra has unsupported format: " << get_transaction_hash(tx));
  }

  tx_extra_pub_key pub_key_field;
  if(!find_tx_extra_field_by_type(tx_extra_fields, pub_key_field))
  {
    LOG_PRINT_L0("Public key wasn't found in the transaction extra. Skipping transaction " << get_transaction_hash(tx));
    if(0 != m_callback)
      m_callback->on_skip_transaction(height, tx);
    return;
  }

  crypto::public_key tx_pub_key = pub_key_field.pub_key;
  bool r = lookup_acc_outs(m_account.get_keys(), tx, tx_pub_key, outs, tx_money_got_in_outs);
  THROW_WALLET_EXCEPTION_IF(!r, error::acc_outs_lookup_error, tx, tx_pub_key, m_account.get_keys());

  if(!outs.empty() && tx_money_got_in_outs)
  {
    //good news - got money! take care about it
    //usually we have only one transfer for user in transaction
    cryptonote::COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::request req = AUTO_VAL_INIT(req);
    cryptonote::COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::response res = AUTO_VAL_INIT(res);
    req.txid = get_transaction_hash(tx);
    bool r = net_utils::invoke_http_bin_remote_command2(m_daemon_address + "/get_o_indexes.bin", req, res, *m_phttp_client, m_conn_timeout);
    THROW_WALLET_EXCEPTION_IF(!r, error::no_connection_to_daemon, "get_o_indexes.bin");
    THROW_WALLET_EXCEPTION_IF(res.status == CORE_RPC_STATUS_BUSY, error::daemon_busy, "get_o_indexes.bin");
    THROW_WALLET_EXCEPTION_IF(res.status != CORE_RPC_STATUS_OK, error::get_out_indices_error, res.status);
    THROW_WALLET_EXCEPTION_IF(res.o_indexes.size() != tx.outs().size(), error::wallet_internal_error,
      "transactions outputs size=" + std::to_string(tx.outs().size()) +
      " not match with COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES response size=" + std::to_string(res.o_indexes.size()));

    size_t out_ix = 0;
    BOOST_FOREACH(size_t o, outs)
    {
      THROW_WALLET_EXCEPTION_IF(tx.outs().size() <= o, error::wallet_internal_error, "wrong out in transaction: internal index=" +
        std::to_string(o) + ", total_outs=" + std::to_string(tx.outs().size()));

      m_transfers.push_back(boost::value_initialized<transfer_details>());
      transfer_details& td = m_transfers.back();
      td.m_block_height = height;
      td.m_from_miner_tx = is_miner_tx;
      td.m_internal_output_index = o;
      td.m_global_output_index = res.o_indexes[o];
      td.m_tx = tx;
      td.m_spent = false;
      cryptonote::keypair in_ephemeral;
      cryptonote::generate_key_image_helper(m_account.get_keys(), tx_pub_key, o, in_ephemeral, td.m_key_image);
      THROW_WALLET_EXCEPTION_IF(in_ephemeral.pub != boost::get<cryptonote::txout_to_key>(tx.outs()[o].target).key,
        error::wallet_internal_error, "key_image generated ephemeral public key not matched with output_key");

      m_key_images[td.m_key_image] = m_transfers.size()-1;
      LOG_PRINT_L0("Received money: " << print_money(td.amount()) << ", with tx: " << get_transaction_hash(tx));
      if (0 != m_callback)
        m_callback->on_money_received(height, td);
      
      out_ix++;
    }
  }

  uint64_t tx_money_spent_in_ins = 0;
  // check all outputs for spending (compare key images)
  size_t in_ix = 0;
  BOOST_FOREACH(const auto& in, tx.ins())
  {
    if(in.type() != typeid(cryptonote::txin_to_key))
      continue;
    auto it = m_key_images.find(boost::get<cryptonote::txin_to_key>(in).k_image);
    if(it != m_key_images.end())
    {
      LOG_PRINT_L0("Spent money: " << print_money(boost::get<cryptonote::txin_to_key>(in).amount) << ", with tx: " << get_transaction_hash(tx));
      tx_money_spent_in_ins += boost::get<cryptonote::txin_to_key>(in).amount;
      transfer_details& td = m_transfers[it->second];
      td.m_spent = true;
      td.m_spent_by_tx = tx;
      if (0 != m_callback)
        m_callback->on_money_spent(height, td);
      
      in_ix++;
    }
  }

  tx_extra_nonce extra_nonce;
  if (find_tx_extra_field_by_type(tx_extra_fields, extra_nonce))
  {
    crypto::hash payment_id;
    if(get_payment_id_from_tx_extra_nonce(extra_nonce.nonce, payment_id))
    {
      uint64_t received = (tx_money_spent_in_ins < tx_money_got_in_outs) ? tx_money_got_in_outs - tx_money_spent_in_ins : 0;
      uint64_t sent = (tx_money_spent_in_ins > tx_money_got_in_outs) ? tx_money_spent_in_ins - tx_money_got_in_outs : 0;
      if ((received > 0 || sent > 0) && null_hash != payment_id)
      {
        payment_details payment;
        payment.m_tx_hash      = cryptonote::get_transaction_hash(tx);
        payment.m_amount       = received;
        payment.m_block_height = height;
        payment.m_unlock_time  = tx.unlock_time;
        payment.m_sent         = sent > 0;
        m_payments.emplace(payment_id, payment);
        LOG_PRINT_L2("Payment found: " << payment_id << " / " << payment.m_tx_hash << " / " << payment.m_amount);
      }
    }
  }
}
//----------------------------------------------------------------------------------------------------
void wallet2::process_unconfirmed(const cryptonote::transaction& tx)
{
  auto unconf_it = m_unconfirmed_txs.find(get_transaction_hash(tx));
  if(unconf_it != m_unconfirmed_txs.end())
    m_unconfirmed_txs.erase(unconf_it);
}
//----------------------------------------------------------------------------------------------------
void wallet2::process_new_blockchain_entry(const cryptonote::block& b, cryptonote::block_complete_entry& bche,
                                           crypto::hash& bl_id, uint64_t height)
{
  //handle transactions from new block
  THROW_WALLET_EXCEPTION_IF(height != m_blockchain.size(), error::wallet_internal_error,
    "current_index=" + std::to_string(height) + ", m_blockchain.size()=" + std::to_string(m_blockchain.size()));

  //optimization: seeking only for blocks that are not older then the wallet creation time plus 1 day. 1 day is for possible user incorrect time setup
  if(b.timestamp + 60*60*24 > m_account.get_createtime())
  {
    TIME_MEASURE_START(miner_tx_handle_time);
    process_new_transaction(b.miner_tx, height, true);
    TIME_MEASURE_FINISH(miner_tx_handle_time);

    TIME_MEASURE_START(txs_handle_time);
    BOOST_FOREACH(auto& txblob, bche.txs)
    {
      cryptonote::transaction tx;
      bool r = parse_and_validate_tx_from_blob(txblob, tx);
      THROW_WALLET_EXCEPTION_IF(!r, error::tx_parse_error, txblob);
      process_new_transaction(tx, height, false);
    }
    TIME_MEASURE_FINISH(txs_handle_time);
    LOG_PRINT_L2("Processed block: " << bl_id << ", height " << height << ", " <<  miner_tx_handle_time + txs_handle_time << "(" << miner_tx_handle_time << "/" << txs_handle_time <<")ms");
  }else
  {
    LOG_PRINT_L2( "Skipped block by timestamp, height: " << height << ", block time " << b.timestamp << ", account time " << m_account.get_createtime());
  }
  m_blockchain.push_back(bl_id);
  ++m_local_bc_height;

  if (0 != m_callback)
    m_callback->on_new_block_processed(height, b);
  
  if (m_local_bc_height % 1000 == 0)
    store();
}
//----------------------------------------------------------------------------------------------------
void wallet2::get_short_chain_history(std::list<crypto::hash>& ids)
{
  size_t i = 0;
  size_t current_multiplier = 1;
  size_t sz = m_blockchain.size();
  if(!sz)
    return;
  size_t current_back_offset = 1;
  bool genesis_included = false;
  while(current_back_offset < sz)
  {
    ids.push_back(m_blockchain[sz-current_back_offset]);
    if(sz-current_back_offset == 0)
      genesis_included = true;
    if(i < 10)
    {
      ++current_back_offset;
    }else
    {
      current_back_offset += current_multiplier *= 2;
    }
    ++i;
  }
  if(!genesis_included)
    ids.push_back(m_blockchain[0]);
}
//----------------------------------------------------------------------------------------------------
void wallet2::pull_blocks(size_t& blocks_added)
{
  blocks_added = 0;
  cryptonote::COMMAND_RPC_GET_BLOCKS_FAST::request req = AUTO_VAL_INIT(req);
  cryptonote::COMMAND_RPC_GET_BLOCKS_FAST::response res = AUTO_VAL_INIT(res);
  get_short_chain_history(req.block_ids);
  bool r = net_utils::invoke_http_bin_remote_command2(m_daemon_address + "/getblocks.bin", req, res, *m_phttp_client, m_conn_timeout);
  THROW_WALLET_EXCEPTION_IF(!r, error::no_connection_to_daemon, "getblocks.bin");
  THROW_WALLET_EXCEPTION_IF(res.status == CORE_RPC_STATUS_BUSY, error::daemon_busy, "getblocks.bin");
  THROW_WALLET_EXCEPTION_IF(res.status != CORE_RPC_STATUS_OK, error::get_blocks_error, res.status);
  THROW_WALLET_EXCEPTION_IF(m_blockchain.size() <= res.start_height, error::wallet_internal_error,
    "wrong daemon response: m_start_height=" + std::to_string(res.start_height) +
    " not less than local blockchain size=" + std::to_string(m_blockchain.size()));

  size_t current_index = res.start_height;
  BOOST_FOREACH(auto& bl_entry, res.blocks)
  {
    cryptonote::block bl;
    r = cryptonote::parse_and_validate_block_from_blob(bl_entry.block, bl);
    THROW_WALLET_EXCEPTION_IF(!r, error::block_parse_error, bl_entry.block);

    crypto::hash bl_id = get_block_hash(bl);
    if(current_index >= m_blockchain.size())
    {
      process_new_blockchain_entry(bl, bl_entry, bl_id, current_index);
      ++blocks_added;
    }
    else if(bl_id != m_blockchain[current_index])
    {
      //split detected here !!!
      THROW_WALLET_EXCEPTION_IF(current_index == res.start_height, error::wallet_internal_error,
        "wrong daemon response: split starts from the first block in response " + string_tools::pod_to_hex(bl_id) +
        " (height " + std::to_string(res.start_height) + "), local block id at this height: " +
        string_tools::pod_to_hex(m_blockchain[current_index]));

      detach_blockchain(current_index);
      process_new_blockchain_entry(bl, bl_entry, bl_id, current_index);
    }
    else
    {
      LOG_PRINT_L2("Block is already in blockchain: " << string_tools::pod_to_hex(bl_id));
    }

    ++current_index;
    
    if (!m_run.load(std::memory_order_relaxed))
      break;
  }
}
//----------------------------------------------------------------------------------------------------
void wallet2::pull_autovote_delegates()
{
  cryptonote::COMMAND_RPC_GET_AUTOVOTE_DELEGATES::request req = AUTO_VAL_INIT(req);
  cryptonote::COMMAND_RPC_GET_AUTOVOTE_DELEGATES::response res = AUTO_VAL_INIT(res);
  bool r = net_utils::invoke_http_json_remote_command2(m_daemon_address + "/getautovotedelegates", req, res, *m_phttp_client, m_conn_timeout);
  THROW_WALLET_EXCEPTION_IF(!r, error::no_connection_to_daemon, "getautovotedelegates");
  THROW_WALLET_EXCEPTION_IF(res.status == CORE_RPC_STATUS_BUSY, error::daemon_busy, "getautovotedelegates");
  THROW_WALLET_EXCEPTION_IF(res.status != CORE_RPC_STATUS_OK, error::get_blocks_error, res.status);
  
  m_autovote_delegates = delegate_votes(res.autovote_delegates.begin(), res.autovote_delegates.end());
}
//----------------------------------------------------------------------------------------------------
void wallet2::refresh()
{
  size_t blocks_fetched = 0;
  refresh(blocks_fetched);
}
//----------------------------------------------------------------------------------------------------
void wallet2::refresh(size_t & blocks_fetched)
{
  bool received_money = false;
  refresh(blocks_fetched, received_money);
}
//----------------------------------------------------------------------------------------------------
void wallet2::refresh(size_t & blocks_fetched, bool& received_money)
{
  THROW_WALLET_EXCEPTION_IF(m_read_only, error::invalid_read_only_operation, "refresh");
  
  received_money = false;
  blocks_fetched = 0;
  size_t added_blocks = 0;
  size_t try_count = 0;
  crypto::hash last_tx_hash_id = m_transfers.size() ? get_transaction_hash(m_transfers.back().m_tx) : null_hash;

  LOG_PRINT_L0("Starting pull_blocks...");
  while(m_run.load(std::memory_order_relaxed))
  {
    try
    {
      pull_blocks(added_blocks);
      blocks_fetched += added_blocks;
      if(!added_blocks)
        break;
    }
    catch (const std::exception&)
    {
      blocks_fetched += added_blocks;
      if(try_count < 3)
      {
        LOG_PRINT_L1("Another try pull_blocks (try_count=" << try_count << ")...");
        ++try_count;
      }
      else
      {
        LOG_ERROR("pull_blocks failed, try_count=" << try_count);
        throw;
      }
    }
  }

  store();
  
  LOG_PRINT_L0("Starting pull_autovote_delegates...");
  // refresh autovote delegates
  try_count = 0;
  while (m_run.load(std::memory_order_relaxed))
  {
    try
    {
      pull_autovote_delegates();
      break;
    }
    catch (const std::exception&)
    {
      if(try_count < 3)
      {
        LOG_PRINT_L1("Another try pull_autovote_delegates (try_count=" << try_count << ")...");
        ++try_count;
      }
      else
      {
        LOG_ERROR("pull_autovote_delegates failed, try_count=" << try_count);
        throw;
      }
    }
  }
  
  if(last_tx_hash_id != (m_transfers.size() ? get_transaction_hash(m_transfers.back().m_tx) : null_hash))
    received_money = true;

  store();
  
  LOG_PRINT_L2("Refresh done, blocks received: " << blocks_fetched << ", balance: " << print_moneys(balance()) << ", unlocked: " << print_moneys(unlocked_balance()));
}
//----------------------------------------------------------------------------------------------------
bool wallet2::refresh(size_t & blocks_fetched, bool& received_money, bool& ok)
{
  try
  {
    refresh(blocks_fetched, received_money);
    ok = true;
  }
  catch (...)
  {
    ok = false;
  }
  return ok;
}
//----------------------------------------------------------------------------------------------------
void wallet2::detach_blockchain(uint64_t height)
{
  LOG_PRINT_L0("Detaching blockchain on height " << height);
  size_t transfers_detached = 0;

  auto it = std::find_if(m_transfers.begin(), m_transfers.end(), [&](const transfer_details& td){return td.m_block_height >= height;});
  size_t i_start = it - m_transfers.begin();

  for(size_t i = i_start; i!= m_transfers.size();i++)
  {
    auto it_ki = m_key_images.find(m_transfers[i].m_key_image);
    THROW_WALLET_EXCEPTION_IF(it_ki == m_key_images.end(), error::wallet_internal_error, "key image not found");
    m_key_images.erase(it_ki);
    
    // remove this transfer from any batches
    BOOST_FOREACH(auto& batch, m_votes_info.m_batches)
    {
      size_t batch_tx_index = 0;
      for (auto itr = batch.m_transfer_indices.begin(); itr != batch.m_transfer_indices.end(); )
      {
        if (*itr == i)
        {
          itr = batch.m_transfer_indices.erase(itr);
          batch.m_fake_outs.erase(batch.m_fake_outs.begin() + batch_tx_index);
        }
        else
        {
          ++itr;
          ++batch_tx_index;
        }
      }
    }
    
    // and from the tx batch map
    m_votes_info.m_transfer_batch_map.erase(i);
    
    ++transfers_detached;
  }
  m_transfers.erase(it, m_transfers.end());

  size_t blocks_detached = m_blockchain.end() - (m_blockchain.begin()+height);
  m_blockchain.erase(m_blockchain.begin()+height, m_blockchain.end());
  m_local_bc_height -= blocks_detached;

  for (auto it = m_payments.begin(); it != m_payments.end(); )
  {
    if(height <= it->second.m_block_height)
      it = m_payments.erase(it);
    else
      ++it;
  }

  LOG_PRINT_L0("Detached blockchain on height " << height << ", transfers detached " << transfers_detached << ", blocks detached " << blocks_detached);
}
//----------------------------------------------------------------------------------------------------
bool wallet2::deinit()
{
  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::clear()
{
  m_blockchain.clear();
  m_transfers.clear();
  cryptonote::block b;
  if (!cryptonote::generate_genesis_block(b))
    throw std::runtime_error("Failed to generate genesis block when clearing wallet");
  
  m_blockchain.push_back(get_block_hash(b));
  m_local_bc_height = 1;
  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::store_keys(const std::string& keys_file_name, const std::string& password)
{
  CHECK_AND_ASSERT_MES(!m_read_only, false, "read-only wallet not storing keys");
  
  std::string account_data;
  bool r = epee::serialization::store_t_to_binary(m_account, account_data);
  CHECK_AND_ASSERT_MES(r, false, "failed to serialize wallet keys");
  detail::keys_file_data keys_file_data = boost::value_initialized<detail::keys_file_data>();

  crypto::chacha8_key key;
  crypto::generate_chacha8_key(password, key);
  std::string cipher;
  cipher.resize(account_data.size());
  keys_file_data.iv = crypto::rand<crypto::chacha8_iv>();
  crypto::chacha8(account_data.data(), account_data.size(), key, keys_file_data.iv, &cipher[0]);
  keys_file_data.account_data = cipher;

  std::string buf;
  r = ::serialization::dump_binary(keys_file_data, buf);
  r = r && epee::file_io_utils::save_string_to_file(keys_file_name, buf); //and never touch wallet_keys_file again, only read
  CHECK_AND_ASSERT_MES(r, false, "failed to generate wallet keys file " << keys_file_name);

  return true;
}
//----------------------------------------------------------------------------------------------------
namespace
{
  bool verify_keys(const crypto::secret_key& sec, const crypto::public_key& expected_pub)
  {
    crypto::public_key pub;
    bool r = crypto::secret_key_to_public_key(sec, pub);
    return r && expected_pub == pub;
  }
}
//----------------------------------------------------------------------------------------------------
void wallet2::load_keys(const std::string& keys_file_name, const std::string& password)
{
  detail::keys_file_data keys_file_data;
  std::string buf;
  bool r = epee::file_io_utils::load_file_to_string(keys_file_name, buf);
  THROW_WALLET_EXCEPTION_IF(!r, error::file_read_error, keys_file_name);
  r = ::serialization::parse_binary(buf, keys_file_data);
  THROW_WALLET_EXCEPTION_IF(!r, error::wallet_internal_error, "internal error: failed to deserialize \"" + keys_file_name + '\"');

  crypto::chacha8_key key;
  crypto::generate_chacha8_key(password, key);
  std::string account_data;
  account_data.resize(keys_file_data.account_data.size());
  crypto::chacha8(keys_file_data.account_data.data(), keys_file_data.account_data.size(), key, keys_file_data.iv, &account_data[0]);

  const cryptonote::account_keys& keys = m_account.get_keys();
  r = epee::serialization::load_t_from_binary(m_account, account_data);
  r = r && verify_keys(keys.m_view_secret_key,  keys.m_account_address.m_view_public_key);
  r = r && verify_keys(keys.m_spend_secret_key, keys.m_account_address.m_spend_public_key);
  THROW_WALLET_EXCEPTION_IF(!r, error::invalid_password);
}
//----------------------------------------------------------------------------------------------------
void wallet2::generate(const std::string& wallet_, const std::string& password)
{
  THROW_WALLET_EXCEPTION_IF(m_read_only, error::invalid_read_only_operation, "generate");
  
  clear();
  prepare_file_names(wallet_);

  boost::system::error_code ignored_ec;
  THROW_WALLET_EXCEPTION_IF(boost::filesystem::exists(m_wallet_file, ignored_ec), error::file_exists, m_wallet_file);
  THROW_WALLET_EXCEPTION_IF(boost::filesystem::exists(m_keys_file,   ignored_ec), error::file_exists, m_keys_file);

  m_account.generate();
  m_account_public_address = m_account.get_keys().m_account_address;

  bool r = store_keys(m_keys_file, password);
  THROW_WALLET_EXCEPTION_IF(!r, error::file_save_error, m_keys_file);

  r = file_io_utils::save_string_to_file(m_wallet_file + ".address.txt", m_account.get_public_address_str());
  if(!r) LOG_PRINT_RED_L0("String with address text not saved");

  store();
}
//----------------------------------------------------------------------------------------------------
void wallet2::wallet_exists(const std::string& file_path, bool& keys_file_exists, bool& wallet_file_exists)
{
  std::string keys_file, wallet_file, known_transfers_file, currency_keys_file, votes_info_file;
  do_prepare_file_names(file_path, keys_file, wallet_file, known_transfers_file, currency_keys_file,
                        votes_info_file);

  boost::system::error_code ignore;
  keys_file_exists = boost::filesystem::exists(keys_file, ignore);
  wallet_file_exists = boost::filesystem::exists(wallet_file, ignore);
}
//----------------------------------------------------------------------------------------------------
bool wallet2::prepare_file_names(const std::string& file_path)
{
  do_prepare_file_names(file_path, m_keys_file, m_wallet_file, m_known_transfers_file, m_currency_keys_file,
                        m_votes_info_file);
  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::check_connection()
{
  if(m_phttp_client->is_connected())
    return true;

  net_utils::http::url_content u;
  net_utils::parse_url(m_daemon_address, u);
  if(!u.port)
    u.port = cryptonote::config::rpc_default_port();
  return m_phttp_client->connect(u.host, std::to_string(u.port), m_conn_timeout);
}
//----------------------------------------------------------------------------------------------------
void wallet2::load(const std::string& wallet_, const std::string& password)
{
  clear();
  prepare_file_names(wallet_);

  boost::system::error_code e;
  bool exists = boost::filesystem::exists(m_keys_file, e);
  THROW_WALLET_EXCEPTION_IF(e || !exists, error::file_not_found, m_keys_file);

  load_keys(m_keys_file, password);
  LOG_PRINT_L0("Loaded wallet keys file, with public address: " << m_account.get_public_address_str());

  //keys loaded ok!
  
  //load known transfers file if it exists
  if (!boost::filesystem::exists(m_known_transfers_file, e) || e)
  {
    LOG_PRINT_L0("known transfers file not found: " << m_known_transfers_file << ", have no transfer data");
  }
  else
  {
    bool r = tools::unserialize_obj_from_file(this->m_known_transfers, m_known_transfers_file);
    if (!r)
      LOG_PRINT_RED_L0("known transfers file is corrupted: " << m_known_transfers_file << ", delete and restart");
    THROW_WALLET_EXCEPTION_IF(!r, error::file_read_error, m_known_transfers_file);
  }
  
  //load currency keys file if it exists
  if (!boost::filesystem::exists(m_currency_keys_file, e) || e)
  {
    LOG_PRINT_L0("currency keys file not found: " << m_currency_keys_file << ", have no currency key data");
  }
  else
  {
    bool r = tools::unserialize_obj_from_file(this->m_currency_keys, m_currency_keys_file);
    if (!r)
      LOG_PRINT_RED_L0("currency keys file is corrupted: " << m_currency_keys_file << ", delete and restart");
    THROW_WALLET_EXCEPTION_IF(!r, error::file_read_error, m_currency_keys_file);
  }
  
  //load voting batch file if it exists
  if (!boost::filesystem::exists(m_votes_info_file, e) || e)
  {
    LOG_PRINT_L0("voting batch file not found: " << m_votes_info_file << ", have no past vote data");
  }
  else
  {
    bool r = tools::unserialize_obj_from_file(this->m_votes_info, m_votes_info_file);
    if (!r)
      LOG_PRINT_RED_L0("voting batch file is corrupted: " << m_votes_info_file << ", delete and restart");
    THROW_WALLET_EXCEPTION_IF(!r, error::file_read_error, m_votes_info_file);
  }
  
  //try to load wallet file. but even if we failed, it is not big problem
  if(!boost::filesystem::exists(m_wallet_file, e) || e)
  {
    LOG_PRINT_L0("file not found: " << m_wallet_file << ", starting with empty blockchain");
    m_account_public_address = m_account.get_keys().m_account_address;
    return;
  }
  bool r = tools::unserialize_obj_from_file(*this, m_wallet_file);
  THROW_WALLET_EXCEPTION_IF(!r, error::file_read_error, m_wallet_file);
  THROW_WALLET_EXCEPTION_IF(
    m_account_public_address.m_spend_public_key != m_account.get_keys().m_account_address.m_spend_public_key ||
    m_account_public_address.m_view_public_key  != m_account.get_keys().m_account_address.m_view_public_key,
    error::wallet_files_doesnt_correspond, m_keys_file, m_wallet_file);
  
  if(m_blockchain.empty())
  {
    cryptonote::block b;
    if (!cryptonote::generate_genesis_block(b))
      throw std::runtime_error("Failed to generate genesis block when loading wallet");
    m_blockchain.push_back(get_block_hash(b));
  }
  m_local_bc_height = m_blockchain.size();
}
//----------------------------------------------------------------------------------------------------
void wallet2::store()
{
  THROW_WALLET_EXCEPTION_IF(m_read_only, error::invalid_read_only_operation, "store");
  LOG_PRINT_GREEN("Storing wallet ...", LOG_LEVEL_1);
  bool r = tools::serialize_obj_to_file(*this, m_wallet_file);
  r = r && tools::serialize_obj_to_file(this->m_known_transfers, m_known_transfers_file);
  r = r && tools::serialize_obj_to_file(this->m_currency_keys, m_currency_keys_file);
  r = r && tools::serialize_obj_to_file(this->m_votes_info, m_votes_info_file);
  THROW_WALLET_EXCEPTION_IF(!r, error::file_save_error, m_wallet_file);
  LOG_PRINT_GREEN("Wallet stored.", LOG_LEVEL_1);
}
//----------------------------------------------------------------------------------------------------
cryptonote::currency_map wallet2::unlocked_balance() const
{
  cryptonote::currency_map amounts;
  
  BOOST_FOREACH(const auto& td, m_transfers)
    if(!td.m_spent && is_transfer_unlocked(td))
      amounts[td.cp()] += td.amount();

  return amounts;
}
//----------------------------------------------------------------------------------------------------
cryptonote::currency_map wallet2::balance() const
{
  cryptonote::currency_map amounts;
  
  BOOST_FOREACH(const auto& td, m_transfers)
    if(!td.m_spent)
      amounts[td.cp()] += td.amount();


  BOOST_FOREACH(const auto& utx, m_unconfirmed_txs)
    amounts[CP_XPB] += utx.second.m_change;

  return amounts;
}
//----------------------------------------------------------------------------------------------------
void wallet2::get_transfers(wallet2::transfer_container& incoming_transfers) const
{
  incoming_transfers = m_transfers;
}
void wallet2::get_known_transfers(wallet2::known_transfer_container& known_transfers) const
{
  known_transfers = m_known_transfers;
}
bool wallet2::get_known_transfer(const crypto::hash& tx_hash, wallet2::known_transfer_details& kd) const
{
  const auto& mi = m_known_transfers.find(tx_hash);
  if (mi == m_known_transfers.end())
    return false;

  kd = mi->second;
  return true;
}

//----------------------------------------------------------------------------------------------------
size_t wallet2::get_num_transfers() const
{
  return m_transfers.size();
}
//----------------------------------------------------------------------------------------------------
void wallet2::get_payments(const crypto::hash& payment_id, std::list<wallet2::payment_details>& payments) const
{
  auto range = m_payments.equal_range(payment_id);
  std::for_each(range.first, range.second, [&payments](const payment_container::value_type& x) {
    payments.push_back(x.second);
  });
}
//----------------------------------------------------------------------------------------------------
bool wallet2::get_payment_for_tx(const crypto::hash& tx_hash,
                                 crypto::hash& payment_id, wallet2::payment_details& payment) const
{
  BOOST_FOREACH(const auto& item, m_payments)
  {
    if (item.second.m_tx_hash == tx_hash)
    {
      payment_id = item.first;
      payment = item.second;
      return true;
    }
  }
  return false;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::is_transfer_unlocked(const transfer_details& td) const
{
  if(!is_tx_spendtime_unlocked(td.m_tx.unlock_time))
    return false;

  if(td.m_block_height + DEFAULT_TX_SPENDABLE_AGE > m_blockchain.size())
    return false;

  return true;
}
//----------------------------------------------------------------------------------------------------
bool wallet2::is_tx_spendtime_unlocked(uint64_t unlock_time) const
{
  if(unlock_time < CRYPTONOTE_MAX_BLOCK_NUMBER)
  {
    //interpret as block index
    if(m_blockchain.size()-1 + cryptonote::config::cryptonote_locked_tx_allowed_delta_blocks() >= unlock_time)
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
//----------------------------------------------------------------------------------------------------
const cryptonote::delegate_votes& wallet2::current_delegate_set() const
{
  return m_voting_user_delegates ? m_user_delegates : m_autovote_delegates;
}
//----------------------------------------------------------------------------------------------------
const cryptonote::delegate_votes& wallet2::user_delegates() const
{
  return m_user_delegates;
}
//----------------------------------------------------------------------------------------------------
const cryptonote::delegate_votes& wallet2::autovote_delegates() const
{
  return m_autovote_delegates;
}
//----------------------------------------------------------------------------------------------------
void wallet2::set_user_delegates(const cryptonote::delegate_votes& new_set)
{
  THROW_WALLET_EXCEPTION_IF(m_read_only, error::invalid_read_only_operation, "set_user_voting_set");
  
  m_user_delegates = new_set;
  store();
}
//----------------------------------------------------------------------------------------------------
void wallet2::set_voting_user_delegates(bool voting_user_delegates)
{
  THROW_WALLET_EXCEPTION_IF(m_read_only, error::invalid_read_only_operation, "set_vote_user_set");
  
  m_voting_user_delegates = voting_user_delegates;
  store();
}
//----------------------------------------------------------------------------------------------------
bool wallet2::voting_user_delegates() const
{
  return m_voting_user_delegates;
}
//----------------------------------------------------------------------------------------------------
void wallet2::add_unconfirmed_tx(const cryptonote::transaction& tx, uint64_t change_amount)
{
  unconfirmed_transfer_details& utd = m_unconfirmed_txs[cryptonote::get_transaction_hash(tx)];
  utd.m_change = change_amount;
  utd.m_sent_time = time(NULL);
  utd.m_tx = tx;
}
//----------------------------------------------------------------------------------------------------
fake_outs_map wallet2::get_fake_outputs(const std::unordered_map<cryptonote::coin_type, std::list<uint64_t> >& amounts, uint64_t min_fake_outs, uint64_t fake_outputs_count)
{
  fake_outs_map res = AUTO_VAL_INIT(res);
  if (!fake_outputs_count)
  {
    // push back empty fake outs
    BOOST_FOREACH(const auto& item, amounts)
    {
      BOOST_FOREACH(const auto& amt, item.second)
      {
        suppress_warning(amt);
        res[item.first].push_back(std::list<out_entry>());
      }
    }
    
    return res;
  }
  
  BOOST_FOREACH(const auto& item, amounts)
  {
    THROW_WALLET_EXCEPTION_IF(item.first != cryptonote::CP_XPB, error::wallet_internal_error,
                              "cannot handle fake outputs of non-xpb yet");
    
    cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request req = AUTO_VAL_INIT(req);
    req.outs_count = fake_outputs_count + 1;// add one to make possible (if need) to skip real output key
    req.amounts = item.second;
    
    cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response this_resp;
    
    bool r = epee::net_utils::invoke_http_bin_remote_command2(m_daemon_address + "/getrandom_outs.bin", req, this_resp, *m_phttp_client, 200000);
    THROW_WALLET_EXCEPTION_IF(!r, error::no_connection_to_daemon, "getrandom_outs.bin");
    THROW_WALLET_EXCEPTION_IF(this_resp.status == CORE_RPC_STATUS_BUSY, error::daemon_busy, "getrandom_outs.bin");
    THROW_WALLET_EXCEPTION_IF(this_resp.status != CORE_RPC_STATUS_OK, error::get_random_outs_error, this_resp.status);
    THROW_WALLET_EXCEPTION_IF(this_resp.outs.size() != req.amounts.size(), error::wallet_internal_error,
                              "daemon returned wrong response for getrandom_outs.bin, wrong amounts count = " +
                              std::to_string(this_resp.outs.size()) + ", expected " +  std::to_string(req.amounts.size()));
    
    std::vector<cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount> scanty_outs;
    BOOST_FOREACH(const auto& amount_outs, this_resp.outs)
    {
      res[item.first].push_back(amount_outs.outs);
      
      if (amount_outs.outs.size() < min_fake_outs)
      {
        scanty_outs.push_back(amount_outs);
      }
    }
    THROW_WALLET_EXCEPTION_IF(!scanty_outs.empty(), error::not_enough_outs_to_mix, scanty_outs, min_fake_outs);
  }
  
  return res;
}
//----------------------------------------------------------------------------------------------------
key_image_seqs wallet2::get_key_image_seqs(const std::vector<crypto::key_image>& key_images)
{
  cryptonote::COMMAND_RPC_GET_KEY_IMAGE_SEQS::request req = AUTO_VAL_INIT(req);
  req.images = key_images;
  
  cryptonote::COMMAND_RPC_GET_KEY_IMAGE_SEQS::response resp;
  
  bool r = epee::net_utils::invoke_http_bin_remote_command2(m_daemon_address + "/getkeyimageseqs.bin", req, resp, *m_phttp_client, 200000);
  THROW_WALLET_EXCEPTION_IF(!r, error::no_connection_to_daemon, "getkeyimageseqs.bin");
  THROW_WALLET_EXCEPTION_IF(resp.status == CORE_RPC_STATUS_BUSY, error::daemon_busy, "getkeyimageseqs.bin");
  THROW_WALLET_EXCEPTION_IF(resp.status != CORE_RPC_STATUS_OK, error::get_key_image_seqs_error, resp.status);
  
  return resp.image_seqs;
}
//----------------------------------------------------------------------------------------------------
void wallet2::send_raw_tx_to_daemon(const cryptonote::transaction& tx)
{
  THROW_WALLET_EXCEPTION_IF(m_upper_transaction_size_limit <= get_object_blobsize(tx),
                            error::tx_too_big, tx, m_upper_transaction_size_limit);
  
  cryptonote::COMMAND_RPC_SEND_RAW_TX::request req;
  
  transaction parsed_tx;
  THROW_WALLET_EXCEPTION_IF(!cryptonote::parse_and_validate_tx_from_blob(tx_to_blob(tx), parsed_tx),
                            error::wallet_internal_error, "Couldn't parse_tx_from_blob(tx_to_blob(tx))");

  req.tx_as_hex = epee::string_tools::buff_to_hex_nodelimer(tx_to_blob(tx));
  
  cryptonote::COMMAND_RPC_SEND_RAW_TX::response daemon_send_resp;
  bool r = epee::net_utils::invoke_http_json_remote_command2(m_daemon_address + "/sendrawtransaction", req, daemon_send_resp,
                                                             *m_phttp_client, 200000);
  THROW_WALLET_EXCEPTION_IF(!r, error::no_connection_to_daemon, "sendrawtransaction");
  THROW_WALLET_EXCEPTION_IF(daemon_send_resp.status == CORE_RPC_STATUS_BUSY, error::daemon_busy, "sendrawtransaction");
  THROW_WALLET_EXCEPTION_IF(daemon_send_resp.status != CORE_RPC_STATUS_OK, error::tx_rejected, tx, daemon_send_resp.status);
}
//----------------------------------------------------------------------------------------------------
void wallet2::transfer(const std::vector<cryptonote::tx_destination_entry>& dsts,
                       size_t min_fake_outs, size_t fake_outputs_count,
                       uint64_t unlock_time, uint64_t fee,
                       const std::vector<uint8_t>& extra,
                       const detail::split_strategy& destination_split_strategy, const tx_dust_policy& dust_policy,
                       cryptonote::transaction &tx)
{
  THROW_WALLET_EXCEPTION_IF(m_read_only, error::invalid_read_only_operation, "transfer");
  THROW_WALLET_EXCEPTION_IF(dsts.empty(), error::zero_destination);
  
  cryptonote::currency_map all_change;
  wallet_tx_builder wtxb(*this);
  wtxb.init_tx(unlock_time, extra);
  wtxb.add_send(dsts, fee, min_fake_outs, fake_outputs_count, destination_split_strategy, dust_policy);
  // try voting up to 2000 xpb, 25 delegates per vote
  wtxb.add_votes(min_fake_outs, fake_outputs_count, dust_policy, COIN*2000, current_delegate_set(), 25);
  wtxb.finalize(tx);
  
  send_raw_tx_to_daemon(tx);

  wtxb.process_transaction_sent();
  
  store();
}
//----------------------------------------------------------------------------------------------------
void wallet2::transfer(const std::vector<cryptonote::tx_destination_entry>& dsts,
                       size_t min_fake_outs, size_t fake_outputs_count,
                       uint64_t unlock_time, uint64_t fee,
                       const std::vector<uint8_t>& extra,
                       const detail::split_strategy& destination_split_strategy, const tx_dust_policy& dust_policy)
{
  cryptonote::transaction tx;
  transfer(dsts, min_fake_outs, fake_outputs_count, unlock_time, fee, extra, destination_split_strategy, dust_policy, tx);
}
//----------------------------------------------------------------------------------------------------
void wallet2::transfer(const std::vector<cryptonote::tx_destination_entry>& dsts, size_t min_fake_outs,
                       size_t fake_outputs_count, uint64_t unlock_time, uint64_t fee, const std::vector<uint8_t>& extra,
                       cryptonote::transaction& tx)
{
  transfer(dsts, min_fake_outs, fake_outputs_count, unlock_time, fee, extra,
           detail::digit_split_strategy(), tx_dust_policy(fee), tx);
}
//----------------------------------------------------------------------------------------------------
void wallet2::transfer(const std::vector<cryptonote::tx_destination_entry>& dsts,
                       size_t min_fake_outs, size_t fake_outputs_count,
                       uint64_t unlock_time, uint64_t fee, const std::vector<uint8_t>& extra)
{
  cryptonote::transaction tx;
  transfer(dsts, min_fake_outs, fake_outputs_count, unlock_time, fee, extra, tx);
}
//----------------------------------------------------------------------------------------------------
void wallet2::mint_subcurrency(uint64_t currency, const std::string &description, uint64_t amount, uint64_t decimals,
                               bool remintable, uint64_t fee, size_t fee_fake_outs_count)
{
  throw std::runtime_error("Not implemented yet");
  /*THROW_WALLET_EXCEPTION_IF(m_read_only, error::invalid_read_only_operation, "mint");
  THROW_WALLET_EXCEPTION_IF(m_currency_keys.find(currency) != m_currency_keys.end(), error::mint_currency_exists, currency);
  
  // construct tx paying the fee, 0 unlock time
  cryptonote::currency_map all_change;
  transfer_selection selection;
  auto txb = partially_construct_tx_to(std::vector<cryptonote::tx_destination_entry>(),
                                       fee_fake_outs_count, 0, fee,
                                       std::vector<uint8_t>(),
                                       detail::digit_split_strategy(), tx_dust_policy(fee),
                                       selected_transfers, all_change);
  
  LOG_PRINT_L0("Made tx with fee of " << print_money(fee) << ", change is " << print_money(all_change[cryptonote::CP_XPB]));
  
  // Generate an anonymous remint keypair
  cryptonote::keypair tx_remint_keypair;
  tx_remint_keypair = remintable ? cryptonote::keypair::generate() : null_keypair;
  assert(remintable || tx_remint_keypair.pub == null_pkey);

  // Send all new coins to ourselves
  std::vector<tx_destination_entry> unsplit_dests;
  unsplit_dests.push_back(tx_destination_entry(coin_type(currency, NotContract, BACKED_BY_N_A),
                                               amount, m_account.get_keys().m_account_address));

  // Split with digit split strategy
  std::vector<cryptonote::tx_destination_entry> split_dests;
  uint64_t xpb_dust = 0;
  detail::digit_split_strategy().split(unsplit_dests, tx_destination_entry(), 0, split_dests, xpb_dust);
  THROW_WALLET_EXCEPTION_IF(xpb_dust != 0, error::wallet_internal_error, "Unexpected xpb dust should be 0");
  
  // Add the mint transaction
  bool success = txb.add_mint(currency, description, amount, decimals, tx_remint_keypair.pub, split_dests);
  THROW_WALLET_EXCEPTION_IF(!success, error::tx_not_constructed, std::vector<cryptonote::tx_source_entry>(), split_dests, 0);
  
  txb.finalize();
  
  cryptonote::transaction tx;
  THROW_WALLET_EXCEPTION_IF(!txb.get_finalized_tx(tx), error::wallet_internal_error, "Could not finalize mint tx");
  
  // Send it
  send_raw_tx_to_daemon(tx);
  
  // Record success
  LOG_PRINT_L2("transaction " << get_transaction_hash(tx) << " generated ok and sent to daemon");

  BOOST_FOREACH(auto it, selected_transfers)
    it->m_spent = true;
  
  known_transfer_details kd;
  kd.m_tx_hash = get_transaction_hash(tx);
  kd.m_dests = unsplit_dests;
  kd.m_fee = fee;
  kd.m_xpb_change = all_change[cryptonote::CP_XPB];
  kd.m_all_change = all_change;
  kd.m_currency_minted = currency;
  kd.m_amount_minted = amount;
  kd.m_delegate_id_registered = 0;
  kd.m_registration_fee_paid = 0;
  m_known_transfers[kd.m_tx_hash] = kd;
  
  m_currency_keys[currency].push_back(tx_remint_keypair);
  
  if (0 != m_callback)
    m_callback->on_new_transfer(tx, kd);
  
  LOG_PRINT_L0("Mint transaction successfully sent. <" << kd.m_tx_hash << ">" << ENDL
                << "Minted " << print_money(amount, decimals) << " of " << currency << "(\"" << description << "\")" << ENDL
                << "Please, wait for confirmation for your balance to be unlocked.");
  
  store();*/
}
//----------------------------------------------------------------------------------------------------
void wallet2::remint_subcurrency(uint64_t currency, uint64_t amount, bool keep_remintable,
                                 uint64_t fee, size_t fee_fake_outs_count)
{
  throw std::runtime_error("Not implemented yet");
  /*THROW_WALLET_EXCEPTION_IF(m_read_only, error::invalid_read_only_operation, "remint");
  THROW_WALLET_EXCEPTION_IF(m_currency_keys.find(currency) == m_currency_keys.end(),
                            error::remint_currency_does_not_exist, currency);
  
  THROW_WALLET_EXCEPTION_IF(m_currency_keys[currency].back() == null_keypair, error::remint_currency_not_remintable, currency);
  
  // construct tx paying the fee, 0 unlock time
  cryptonote::currency_map all_change;
  std::list<transfer_container::iterator> selected_transfers;
  auto txb = partially_construct_tx_to(std::vector<cryptonote::tx_destination_entry>(),
                                       fee_fake_outs_count, 0, fee,
                                       std::vector<uint8_t>(),
                                       detail::digit_split_strategy(), tx_dust_policy(fee),
                                       selected_transfers, all_change);
  
  LOG_PRINT_L0("Made tx with fee of " << print_money(fee) << ", change is " << print_money(all_change[cryptonote::CP_XPB]));
  
  // Generate an anonymous new remint keypair
  cryptonote::keypair tx_new_remint_keypair;
  tx_new_remint_keypair = keep_remintable ? cryptonote::keypair::generate() : null_keypair;
  assert(keep_remintable || tx_new_remint_keypair.pub == null_pkey);

  // Send all new coins to ourselves
  std::vector<tx_destination_entry> unsplit_dests;
  unsplit_dests.push_back(tx_destination_entry(coin_type(currency, NotContract, BACKED_BY_N_A),
                                               amount, m_account.get_keys().m_account_address));

  // Split with digit split strategy
  std::vector<cryptonote::tx_destination_entry> split_dests;
  uint64_t xpb_dust = 0;
  detail::digit_split_strategy().split(unsplit_dests, tx_destination_entry(), 0, split_dests, xpb_dust);
  THROW_WALLET_EXCEPTION_IF(xpb_dust != 0, error::wallet_internal_error, "Unexpected xpb dust should be 0");
  
  // Add the remint transaction
  bool success = txb.add_remint(currency, amount,
                                m_currency_keys[currency].back().sec, tx_new_remint_keypair.pub,
                                split_dests);
  THROW_WALLET_EXCEPTION_IF(!success, error::tx_not_constructed, std::vector<cryptonote::tx_source_entry>(), split_dests, 0);
  
  txb.finalize();
  
  cryptonote::transaction tx;
  THROW_WALLET_EXCEPTION_IF(!txb.get_finalized_tx(tx), error::wallet_internal_error, "Could not finalize remint tx");
  
  // Send it
  send_raw_tx_to_daemon(tx);
  
  // Record success
  LOG_PRINT_L2("transaction " << get_transaction_hash(tx) << " generated ok and sent to daemon");

  BOOST_FOREACH(auto it, selected_transfers)
    it->m_spent = true;
  
  known_transfer_details kd;
  kd.m_tx_hash = get_transaction_hash(tx);
  kd.m_dests = unsplit_dests;
  kd.m_fee = fee;
  kd.m_xpb_change = all_change[cryptonote::CP_XPB];
  kd.m_all_change = all_change;
  kd.m_currency_minted = currency;
  kd.m_amount_minted = amount;
  kd.m_delegate_id_registered = 0;
  kd.m_registration_fee_paid = 0;
  m_known_transfers[kd.m_tx_hash] = kd;
  
  m_currency_keys[currency].push_back(tx_new_remint_keypair);
  
  if (0 != m_callback)
    m_callback->on_new_transfer(tx, kd);
  
  LOG_PRINT_L0("Remint transaction successfully sent. <" << kd.m_tx_hash << ">" << ENDL
                << "Minted " << print_money(amount, 0) << " of " << currency << ENDL
                << "Please, wait for confirmation for your balance to be unlocked.");
  
  store();*/
}
//----------------------------------------------------------------------------------------------------
void wallet2::register_delegate(const cryptonote::delegate_id_t& delegate_id,
                                uint64_t registration_fee, size_t min_fake_outs, size_t fake_outputs_count,
                                const cryptonote::account_public_address& address,
                                cryptonote::transaction& result)
{
  THROW_WALLET_EXCEPTION_IF(m_read_only, error::invalid_read_only_operation, "register_delegate");
  THROW_WALLET_EXCEPTION_IF(delegate_id == 0, error::zero_delegate_id);
  
  wallet_tx_builder wtxb(*this);
  wtxb.init_tx();
  // add the tx + registration fee
  wtxb.add_send(std::vector<cryptonote::tx_destination_entry>(), registration_fee + DEFAULT_FEE,
                min_fake_outs, fake_outputs_count, detail::digit_split_strategy(),
                tx_dust_policy(DEFAULT_FEE));
  // add the delegate
  wtxb.add_register_delegate(delegate_id, address, registration_fee);
  // try adding votes
  wtxb.add_votes(min_fake_outs, fake_outputs_count, tx_dust_policy(DEFAULT_FEE), COIN*2000, current_delegate_set(), 25);
  wtxb.finalize(result);
  
  send_raw_tx_to_daemon(result);
  
  wtxb.process_transaction_sent();
  
  store();
}
//----------------------------------------------------------------------------------------------------
bool wallet2::sign_dpos_block(block& bl) const
{
  return cryptonote::sign_dpos_block(bl, m_account);
}
//----------------------------------------------------------------------------------------------------
uint64_t wallet2::get_amount_unvoted() const
{
  uint64_t result = 0;
  size_t i = 0;
  BOOST_FOREACH(const auto& td, m_transfers)
  {
    if (!td.m_spent && const_get(m_votes_info.m_transfer_batch_map, i) == 0)
    {
      result += td.amount();
    }
    i++;
  }
  return result;
}
//----------------------------------------------------------------------------------------------------
std::string wallet2::debug_batches() const
{
  std::stringstream ss;
  
  for (size_t batch_i=1; batch_i < m_votes_info.m_batches.size(); ++batch_i)
  {
    const auto& batch = m_votes_info.m_batches[batch_i];
    
    ss << "Batch #" << batch_i << (batch.spent(*this) ? " (spent)" : "") << ":" << ENDL;
    size_t i = 0;
    BOOST_FOREACH(const auto& td_i, batch.m_transfer_indices)
    {
      const auto& td = m_transfers[td_i];
      ss << "  " << (td.m_spent ? "*" : "") << "Transfer #" << td_i << ": "
         << std::setw(13) << print_money(td.amount()) << " XPB, "
         << "Block " << td.m_block_height
         << ", tx " << boost::lexical_cast<std::string>(get_transaction_hash(td.m_tx)).substr(0, 10) << "...>"
         << "[" << td.m_internal_output_index << "]"
         << ", " << td.m_global_output_index << " "
         << "(" << str_join(batch.m_fake_outs[i], [](const tools::out_entry& oe) { return oe.global_amount_index; }) << ")"
         << ENDL;
      i++;
    }
    ss << "  Total unspent in batch: " << print_money(batch.amount(*this)) << " XPB" << ENDL;
    ss << "  Vote history: " << ENDL;
    BOOST_FOREACH(const auto& votes, batch.m_vote_history)
    {
      ss << "    <" << str_join(votes) << ">" << ENDL;
    }
    ss << "-------------------------------" << ENDL;
  }
  
  ss << print_money(get_amount_unvoted()) << " XPB unbatched/unvoted" << ENDL;
  return ss.str();
}
//----------------------------------------------------------------------------------------------------
  
}
