// Copyright (c) 2014 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <stdint.h>

#include "common/command_line.h"

namespace conn_tool_opt
{
  extern const command_line::arg_descriptor<std::string, true> arg_ip;
  extern const command_line::arg_descriptor<size_t>      arg_por;
  extern const command_line::arg_descriptor<size_t>      arg_rpc_port;
  extern const command_line::arg_descriptor<uint32_t, true> arg_timeout;
  extern const command_line::arg_descriptor<std::string> arg_priv_key;
  extern const command_line::arg_descriptor<uint64_t>    arg_peer_id;
  extern const command_line::arg_descriptor<bool>        arg_generate_keys;
  extern const command_line::arg_descriptor<bool>        arg_request_stat_info;
  extern const command_line::arg_descriptor<bool>        arg_request_net_state;
  extern const command_line::arg_descriptor<bool>        arg_get_daemon_info;
}