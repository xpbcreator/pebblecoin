#ifndef PEBBLECOIN_TYPES_H
#define PEBBLECOIN_TYPES_H

namespace cryptonote
{
  class core;
  template<class t_core> class t_cryptonote_protocol_handler;
  class core_rpc_server;
  struct account_public_address;
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

#endif