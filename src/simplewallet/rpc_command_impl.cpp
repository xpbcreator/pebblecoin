// Copyright (c) 2014-2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include <vector>
#include <thread>
#include "include_base_utils.h"
#include "net/http_client.h"
#include "storages/http_abstract_invoke.h"

#include "rpc/core_rpc_server_commands_defs.h"
#include "wallet/wallet2.h"

#include "simplewallet.h"
#include "message_writer.h"

using namespace std;
using namespace epee;
using namespace cryptonote;

std::string interpret_rpc_response(bool ok, const std::string& status)
{
  std::string err;
  if (ok)
  {
    if (status == CORE_RPC_STATUS_BUSY)
    {
      err = "daemon is busy. Please try later";
    }
    else if (status != CORE_RPC_STATUS_OK)
    {
      err = status;
    }
  }
  else
  {
    err = "possible lost connection to daemon";
  }
  return err;
}

//----------------------------------------------------------------------------------------------------
void simple_wallet::init_http_client()
{
  m_phttp_client = new epee::net_utils::http::http_simple_client();
}
//----------------------------------------------------------------------------------------------------
void simple_wallet::destroy_http_client()
{
  delete m_phttp_client;
  m_phttp_client = NULL;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::start_mining(const std::vector<std::string>& args)
{
  if (!try_connect_to_daemon())
    return true;

  COMMAND_RPC_START_MINING::request req;
  req.miner_address = m_wallet->get_account().get_public_address_str();

  bool ok = true;
  size_t max_mining_threads_count = (std::max)(std::thread::hardware_concurrency(), static_cast<unsigned>(2));
  if (0 == args.size())
  {
    req.threads_count = 1;
  }
  else if (1 == args.size())
  {
    uint16_t num;
    ok = string_tools::get_xtype_from_string(num, args[0]);
    ok &= (1 <= num && num <= max_mining_threads_count);
    req.threads_count = num;
  }
  else
  {
    ok = false;
  }

  if (!ok)
  {
    fail_msg_writer() << "invalid arguments. Please use start_mining [<number_of_threads>], " <<
      "<number_of_threads> should be from 1 to " << max_mining_threads_count;
    return true;
  }

  COMMAND_RPC_START_MINING::response res;
  bool r = net_utils::invoke_http_json_remote_command2(m_daemon_address + "/start_mining", req, res, *m_phttp_client);
  std::string err = interpret_rpc_response(r, res.status);
  if (err.empty())
    success_msg_writer() << "Mining started in daemon";
  else
    fail_msg_writer() << "mining has NOT been started: " << err;
  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::stop_mining(const std::vector<std::string>& args)
{
  if (!try_connect_to_daemon())
    return true;

  COMMAND_RPC_STOP_MINING::request req;
  COMMAND_RPC_STOP_MINING::response res;
  bool r = net_utils::invoke_http_json_remote_command2(m_daemon_address + "/stop_mining", req, res, *m_phttp_client);
  std::string err = interpret_rpc_response(r, res.status);
  if (err.empty())
    success_msg_writer() << "Mining stopped in daemon";
  else
    fail_msg_writer() << "mining has NOT been stopped: " << err;
  return true;
}
//----------------------------------------------------------------------------------------------------
uint64_t simple_wallet::get_daemon_blockchain_height(std::string& err)
{
  COMMAND_RPC_GET_HEIGHT::request req;
  COMMAND_RPC_GET_HEIGHT::response res = boost::value_initialized<COMMAND_RPC_GET_HEIGHT::response>();
  bool r = net_utils::invoke_http_json_remote_command2(m_daemon_address + "/getheight", req, res, *m_phttp_client);
  err = interpret_rpc_response(r, res.status);
  return res.height;
}
