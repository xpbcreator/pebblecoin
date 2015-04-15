// Copyright (c) 2014 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "common/command_line.h"

#include <string>

namespace daemon_opt
{
  extern const command_line::arg_descriptor<std::string> arg_config_file;
  extern const command_line::arg_descriptor<std::string> arg_pid_file;
  extern const command_line::arg_descriptor<bool> arg_os_version;
  extern const command_line::arg_descriptor<std::string> arg_log_file;
  extern const command_line::arg_descriptor<int> arg_log_level;
  extern const command_line::arg_descriptor<bool> arg_console;
  extern const command_line::arg_descriptor<bool> arg_testnet_on;
}
