// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <set>

#if defined(_MSC_VER)
#define POD_CLASS struct
#else
#define POD_CLASS class
#endif

namespace cryptonote
{
  class core;
  template<class t_core> class t_cryptonote_protocol_handler;
  class core_rpc_server;
  struct account_public_address;
  
  typedef uint16_t delegate_id_t;
  typedef std::set<delegate_id_t> delegate_votes;
}

namespace nodetool
{
  template<class t_payload_net_handler> class node_server;
}

namespace tools
{
  class wallet2;
}

typedef cryptonote::core core_t;
typedef cryptonote::t_cryptonote_protocol_handler<core_t> protocol_handler_t;
typedef nodetool::node_server<protocol_handler_t> node_server_t;
typedef cryptonote::core_rpc_server rpc_server_t;
typedef tools::wallet2 wallet_t;
typedef cryptonote::account_public_address address_t;
