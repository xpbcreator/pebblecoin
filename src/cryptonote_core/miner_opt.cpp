// Copyright (c) 2014 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner_opt.h"

namespace miner_opt
{
  const command_line::arg_descriptor<std::string> arg_extra_messages =  {"extra-messages-file", "Specify file for extra messages to include into coinbase transactions", "", true};
  const command_line::arg_descriptor<std::string> arg_start_mining =    {"start-mining", "Specify wallet address to mining for", "", true};
  const command_line::arg_descriptor<uint32_t>    arg_mining_threads =  {"mining-threads", "Specify mining threads count (+13gb RAM for each after 1)", 0, true};
  const command_line::arg_descriptor<bool>        arg_dont_share_state = {"no-share-state", "Specify the first mining thread should not use the global boulderhash state (+13gb RAM)"};
}
