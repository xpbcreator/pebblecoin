// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "common/command_line.h"

namespace miner_opt
{
  extern const command_line::arg_descriptor<std::string> arg_extra_messages;
  extern const command_line::arg_descriptor<std::string> arg_start_mining;
  extern const command_line::arg_descriptor<uint32_t>    arg_mining_threads;
  extern const command_line::arg_descriptor<bool>        arg_dont_share_state;
}

