// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <vector>

#include "include_base_utils.h"

#include "common/functional.h"
#include "common/stl-util.h"
#include "crypto/crypto_basic_impl.h"
#include "wallet/wallet2.h"
#include "wallet/wallet_errors.h"

#include "simplewallet.h"
#include "message_writer.h"

using namespace std;
using namespace epee;
using namespace cryptonote;

#define EXTENDED_LOGS_FILE "wallet_details.log"

//----------------------------------------------------------------------------------------------------
void simple_wallet::on_new_block_processed(uint64_t height, const cryptonote::block& block)
{
  m_refresh_progress_reporter.update(height, false);
}
//----------------------------------------------------------------------------------------------------
void simple_wallet::on_money_received(uint64_t height, const tools::wallet2::transfer_details& td)
{
  message_writer(epee::log_space::console_color_green, false) <<
    "Height " << height <<
    ", transaction " << get_transaction_hash(td.m_tx) <<
    ", received " << print_money(td.m_tx.outs()[td.m_internal_output_index].amount);
  m_refresh_progress_reporter.update(height, true);
}
//----------------------------------------------------------------------------------------------------
void simple_wallet::on_money_spent(uint64_t height, const tools::wallet2::transfer_details& td)
{
  message_writer(epee::log_space::console_color_magenta, false) <<
    "Height " << height <<
    ", transaction " << get_transaction_hash(td.m_spent_by_tx) <<
    ", spent " << print_money(td.m_tx.outs()[td.m_internal_output_index].amount);
  m_refresh_progress_reporter.update(height, true);
}
//----------------------------------------------------------------------------------------------------
void simple_wallet::on_skip_transaction(uint64_t height, const cryptonote::transaction& tx)
{
  message_writer(epee::log_space::console_color_red, true) <<
    "Height " << height <<
    ", transaction " << get_transaction_hash(tx) <<
    ", unsupported transaction format";
  m_refresh_progress_reporter.update(height, true);
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::refresh(const std::vector<std::string>& args)
{
  if (!try_connect_to_daemon())
    return true;

  message_writer() << "Starting refresh...";
  size_t fetched_blocks = 0;
  bool ok = false;
  std::ostringstream ss;
  capturing_exceptions([&]() {
    m_wallet->refresh(fetched_blocks);
    ok = true;
    // Clear line "Height xxx of xxx"
    std::cout << "\r                                                                \r";
    success_msg_writer(true) << "Refresh done, blocks received: " << fetched_blocks;
    show_balance();
  });

  if (!ok)
  {
    fail_msg_writer() << "refresh failed: " << ss.str() << ". Blocks received: " << fetched_blocks;
  }

  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::show_balance(const std::vector<std::string>& args)
{
  success_msg_writer() << "balance: " << print_moneys(m_wallet->balance()) << ", unlocked balance: " << print_moneys(m_wallet->unlocked_balance());
  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::show_incoming_transfers(const std::vector<std::string>& args)
{
  bool filter = false;
  bool available = false;
  if (!args.empty())
  {
    if (args[0] == "available")
    {
      filter = true;
      available = true;
    }
    else if (args[0] == "unavailable")
    {
      filter = true;
      available = false;
    }
  }

  tools::wallet2::transfer_container transfers;
  m_wallet->get_transfers(transfers);

  bool transfers_found = false;
  for (const auto& td : transfers)
  {
    if (!filter || available != td.m_spent)
    {
      if (!transfers_found)
      {
        message_writer() << "        amount       \tspent\tglobal index\t                              tx id";
        transfers_found = true;
      }
      message_writer(td.m_spent ? epee::log_space::console_color_magenta : epee::log_space::console_color_green, false) <<
        std::setw(21) << print_money(td.amount()) << '\t' <<
        std::setw(3) << (td.m_spent ? 'T' : 'F') << "  \t" <<
        std::setw(12) << td.m_global_output_index << '\t' <<
        get_transaction_hash(td.m_tx);
    }
  }

  if (!transfers_found)
  {
    if (!filter)
    {
      success_msg_writer() << "No incoming transfers";
    }
    else if (available)
    {
      success_msg_writer() << "No incoming available transfers";
    }
    else
    {
      success_msg_writer() << "No incoming unavailable transfers";
    }
  }

  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::show_payments(const std::vector<std::string> &args)
{
  if(args.empty())
  {
    fail_msg_writer() << "expected at least one payment_id";
    return true;
  }

  message_writer() << "                            payment                             \t" <<
    "                          transaction                           \t" <<
    "  height\t       amount        \tunlock time";

  bool payments_found = false;
  for(std::string arg : args)
  {
    crypto::hash payment_id;
    if(parse_payment_id(arg, payment_id))
    {
      std::list<tools::wallet2::payment_details> payments;
      m_wallet->get_payments(payment_id, payments);
      if(payments.empty())
      {
        success_msg_writer() << "No payments with id " << payment_id;
        continue;
      }

      for (const tools::wallet2::payment_details& pd : payments)
      {
        if(!payments_found)
        {
          payments_found = true;
        }
        success_msg_writer(true) <<
          payment_id << '\t' <<
          pd.m_tx_hash << '\t' <<
          std::setw(8)  << pd.m_block_height << '\t' <<
          std::setw(21) << print_money(pd.m_amount) << '\t' <<
          pd.m_unlock_time << '\t' <<
          (pd.m_sent ? "Sent" : "Received");
      }
    }
    else
    {
      fail_msg_writer() << "payment id has invalid format: \"" << arg << "\", expected 64-character string";
    }
  }

  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::show_blockchain_height(const std::vector<std::string>& args)
{
  if (!try_connect_to_daemon())
    return true;

  std::string err;
  uint64_t bc_height = get_daemon_blockchain_height(err);
  if (err.empty())
    success_msg_writer() << bc_height;
  else
    fail_msg_writer() << "failed to get blockchain height: " << err;
  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::transfer(const std::vector<std::string> &args_, uint64_t currency_id)
{
  if (currency_id != CURRENCY_XPB)
  {
    throw std::runtime_error("Sending non-xpb currencies not yet implemented");
  }
  
  if (!try_connect_to_daemon())
    return true;

  std::vector<std::string> local_args = args_;
  if(local_args.size() < 3)
  {
    fail_msg_writer() << "wrong number of arguments, expected at least 3, got " << local_args.size();
    return true;
  }

  size_t min_fake_outs;
  if(!epee::string_tools::get_xtype_from_string(min_fake_outs, local_args[0]))
  {
    fail_msg_writer() << "minimum mixin_count should be non-negative integer, got " << local_args[0];
    return true;
  }
  local_args.erase(local_args.begin());

  size_t fake_outs_count;
  if(!epee::string_tools::get_xtype_from_string(fake_outs_count, local_args[0]))
  {
    fail_msg_writer() << "desired mixin_count should be non-negative integer, got " << local_args[0];
    return true;
  }
  local_args.erase(local_args.begin());

  std::vector<uint8_t> extra;
  if (1 == local_args.size() % 2)
  {
    std::string payment_id_str = local_args.back();
    local_args.pop_back();

    crypto::hash payment_id;
    bool r = parse_payment_id(payment_id_str, payment_id);
    if(r)
    {
      std::string extra_nonce;
      set_payment_id_to_tx_extra_nonce(extra_nonce, payment_id);
      r = add_extra_nonce_to_tx_extra(extra, extra_nonce);
    }

    if(!r)
    {
      fail_msg_writer() << "payment id has invalid format: \"" << payment_id_str << "\", expected 64-character string";
      return true;
    }
  }

  vector<cryptonote::tx_destination_entry> dsts;
  for (size_t i = 0; i < local_args.size(); i += 2)
  {
    cryptonote::tx_destination_entry de;
    if(!get_account_address_from_str(de.addr, local_args[i]))
    {
      fail_msg_writer() << "wrong address: " << local_args[i];
      return true;
    }

    de.cp = CP_XPB;

    bool ok;
    ok = cryptonote::parse_amount(de.amount, local_args[i + 1]);
    // when non-xpb, account for different decimal point
    
    if(!ok || 0 == de.amount)
    {
      fail_msg_writer() << "amount is wrong: " << local_args[i] << ' ' << local_args[i + 1] <<
        ", expected number from 0 to " << print_money(std::numeric_limits<uint64_t>::max());
      return true;
    }

    dsts.push_back(de);
  }

  capturing_exceptions([&]() {
    cryptonote::transaction tx;
    m_wallet->transfer(dsts, min_fake_outs, fake_outs_count, 0, DEFAULT_FEE, extra, tx);
    success_msg_writer(true) << "Money successfully sent, transaction " << get_transaction_hash(tx);
  });

  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::transfer_currency(const std::vector<std::string> &args_)
{
  std::vector<std::string> local_args = args_;
  if(local_args.size() < 4)
  {
    fail_msg_writer() << "wrong number of arguments, expected at least 4, got " << local_args.size();
    return true;
  }
  
  uint64_t currency_id;
  
  if (!epee::string_tools::get_xnum_from_hex_string(local_args[0], currency_id))
  {
    fail_msg_writer() << "currency_id should be hex of a 64-bit integer, got " << local_args[0];
    return true;
  }
  local_args.erase(local_args.begin());
  
  return transfer(local_args, currency_id);
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::mint(const std::vector<std::string> &args_)
{
  if (!try_connect_to_daemon())
    return true;

  std::vector<std::string> local_args = args_;
  if(local_args.size() < 3)
  {
    fail_msg_writer() << "wrong number of arguments, expected at least 3, got " << local_args.size();
    return true;
  }
  
  uint64_t currency_id, amount;
  size_t fake_outs_count;
  uint64_t decimals = 2;
  std::string description = "";
  bool remintable = true;
  
  if (!epee::string_tools::get_xnum_from_hex_string(local_args[0], currency_id))
  {
    fail_msg_writer() << "currency_id should be hex of a 64-bit integer, got " << local_args[0];
    return true;
  }
  local_args.erase(local_args.begin());

  if (!epee::string_tools::get_xtype_from_string(amount, local_args[0]))
  {
    fail_msg_writer() << "amount should be non-negative 64-bit integer, got " << local_args[0];
    return true;
  }
  local_args.erase(local_args.begin());
  
  if(!epee::string_tools::get_xtype_from_string(fake_outs_count, local_args[0]))
  {
    fail_msg_writer() << "mixin_count should be non-negative integer, got " << local_args[0];
    return true;
  }
  local_args.erase(local_args.begin());
  
  if (!local_args.empty())
  {
    if (!epee::string_tools::get_xtype_from_string(decimals, local_args[0]))
    {
      fail_msg_writer() << "decimals should be non-negative 64-bit integer, got " << local_args[0];
      return true;
    }
    local_args.erase(local_args.begin());
  }
  
  if (!local_args.empty())
  {
    description = local_args[0];
    local_args.erase(local_args.begin());
  }
  
  if (!local_args.empty())
  {
    if (!epee::string_tools::get_xtype_from_string(remintable, local_args[0]))
    {
      fail_msg_writer() << "remintable should be a boolean, got " << local_args[0];
      return true;
    }
    local_args.erase(local_args.begin());
  }
  
  capturing_exceptions([&]() {
    cryptonote::transaction tx;
    m_wallet->mint_subcurrency(currency_id, description, amount, decimals, remintable, DEFAULT_FEE, fake_outs_count);
    success_msg_writer(true) << "Currency successfully minted, transaction " << get_transaction_hash(tx);
  });
  
  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::remint(const std::vector<std::string> &args_)
{
  if (!try_connect_to_daemon())
    return true;

  std::vector<std::string> local_args = args_;
  if(local_args.size() < 3)
  {
    fail_msg_writer() << "wrong number of arguments, expected at least 3, got " << local_args.size();
    return true;
  }
  
  uint64_t currency_id, amount;
  size_t fake_outs_count;
  bool keep_remintable = true;
  
  if (!epee::string_tools::get_xnum_from_hex_string(local_args[0], currency_id))
  {
    fail_msg_writer() << "currency_id should be hex of a 64-bit integer, got " << local_args[0];
    return true;
  }
  local_args.erase(local_args.begin());

  if (!epee::string_tools::get_xtype_from_string(amount, local_args[0]))
  {
    fail_msg_writer() << "amount should be non-negative 64-bit integer, got " << local_args[0];
    return true;
  }
  local_args.erase(local_args.begin());
  
  if(!epee::string_tools::get_xtype_from_string(fake_outs_count, local_args[0]))
  {
    fail_msg_writer() << "mixin_count should be non-negative integer, got " << local_args[0];
    return true;
  }
  local_args.erase(local_args.begin());
  
  if (!local_args.empty())
  {
    if (!epee::string_tools::get_xtype_from_string(keep_remintable, local_args[0]))
    {
      fail_msg_writer() << "remintable should be a boolean, got " << local_args[0];
      return true;
    }
    local_args.erase(local_args.begin());
  }
  
  capturing_exceptions([&]() {
    cryptonote::transaction tx;
    m_wallet->remint_subcurrency(currency_id, amount, keep_remintable, DEFAULT_FEE, fake_outs_count);
    success_msg_writer(true) << "Currency successfully reminted, transaction " << get_transaction_hash(tx);
  });

  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::register_delegate(const std::vector<std::string> &args_)
{
  if (!try_connect_to_daemon())
    return true;
  
  std::vector<std::string> local_args = args_;
  if(local_args.size() < 2)
  {
    fail_msg_writer() << "wrong number of arguments, expected at least 3, got " << local_args.size();
    return true;
  }
  
  cryptonote::delegate_id_t delegate_id = 0;
  uint64_t registration_fee = 0;
  size_t min_fake_outs = 5;
  size_t fake_outs = 5;
  cryptonote::account_public_address addr = m_wallet->get_account().get_keys().m_account_address;
  
  if (!epee::string_tools::get_xtype_from_string(delegate_id, local_args[0]) || delegate_id == 0)
  {
    fail_msg_writer() << "delegate_id should be number 1-65535, got " << local_args[0];
    return true;
  }
  local_args.erase(local_args.begin());
  
  if (!cryptonote::parse_amount(registration_fee, local_args[0]))
  {
    fail_msg_writer() << "fee should be an amount, got " << local_args[0] << " (parsed into " << registration_fee << ")";
    return true;
  }
  local_args.erase(local_args.begin());
  
  if (!local_args.empty())
  {
    if(!epee::string_tools::get_xtype_from_string(min_fake_outs, local_args[0]))
    {
      fail_msg_writer() << "3rd arg should be min fake outs, got " << local_args[0];
      return true;
    }
    local_args.erase(local_args.begin());
  }
  
  if (!local_args.empty())
  {
    if(!epee::string_tools::get_xtype_from_string(fake_outs, local_args[0]))
    {
      fail_msg_writer() << "4th arg should be fake outs, got " << local_args[0];
      return true;
    }
    local_args.erase(local_args.begin());
  }
  
  if (!local_args.empty())
  {
    if(!get_account_address_from_str(addr, local_args[0]))
    {
      fail_msg_writer() << "5th arg should be address, got " << local_args[0];
      return true;
    }
    local_args.erase(local_args.begin());
  }
  
  capturing_exceptions([&]() {
    cryptonote::transaction tx;
    m_wallet->register_delegate(delegate_id, registration_fee, min_fake_outs, fake_outs, addr, tx);
    success_msg_writer(true) << "Successfully registered delegate, transaction " << get_transaction_hash(tx);
  });
  
  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::print_address(const std::vector<std::string> &args)
{
  success_msg_writer() << m_wallet->get_account().get_public_address_str();
  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::debug_batches(const std::vector<std::string> &args)
{
  success_msg_writer() << m_wallet->debug_batches();
  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::list_delegates(const std::vector<std::string> &args)
{
  success_msg_writer() << "Currently voting " << (m_wallet->voting_user_delegates() ? "user" : "automatically-selected") << " delegates <" << tools::str_join(m_wallet->current_delegate_set()) << ">";
  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::enable_autovote(const std::vector<std::string> &args)
{
  m_wallet->set_voting_user_delegates(false);
  list_delegates(args);
  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::disable_autovote(const std::vector<std::string> &args)
{
  m_wallet->set_voting_user_delegates(true);
  list_delegates(args);
  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::add_delegates(const std::vector<std::string> &_args)
{
  if (!m_wallet->voting_user_delegates())
  {
    fail_msg_writer() << "Must disable_autovote to select your own delegates";
    return true;
  }
  
  auto args = _args;
  delegate_votes to_add;
  while (!args.empty())
  {
    delegate_id_t add_d;
    if(!epee::string_tools::get_xtype_from_string(add_d, args[0]))
    {
      fail_msg_writer() << "Expected delegate id, got " << args[0];
      return true;
    }
    to_add.insert(add_d);
    args.erase(args.begin());
  }
  
  delegate_votes new_set = tools::set_or(m_wallet->user_delegates(), to_add);
  m_wallet->set_user_delegates(new_set);
  
  list_delegates(args);
  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::remove_delegates(const std::vector<std::string> &_args)
{
  if (!m_wallet->voting_user_delegates())
  {
    fail_msg_writer() << "Must disable_autovote to select your own delegates";
    return true;
  }
  
  auto args = _args;
  delegate_votes to_remove;
  while (!args.empty())
  {
    delegate_id_t remove_d;
    if(!epee::string_tools::get_xtype_from_string(remove_d, args[0]))
    {
      fail_msg_writer() << "Expected delegate id, got " << args[0];
      return true;
    }
    to_remove.insert(remove_d);
    args.erase(args.begin());
  }
  
  delegate_votes new_set = tools::set_sub(m_wallet->user_delegates(), to_remove);
  m_wallet->set_user_delegates(new_set);
  
  list_delegates(args);
  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::set_delegates(const std::vector<std::string> &_args)
{
  if (!m_wallet->voting_user_delegates())
  {
    fail_msg_writer() << "Must disable_autovote to select your own delegates";
    return true;
  }
  
  auto args = _args;
  delegate_votes to_set;
  while (!args.empty())
  {
    delegate_id_t set_d;
    if(!epee::string_tools::get_xtype_from_string(set_d, args[0]))
    {
      fail_msg_writer() << "Expected delegate id, got " << args[0];
      return true;
    }
    to_set.insert(set_d);
    args.erase(args.begin());
  }
  
  m_wallet->set_user_delegates(to_set);
  
  list_delegates(args);
  return true;
}
