// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "common/command_line.h"

#include <string>

namespace qt_opt
{
  extern const command_line::arg_descriptor<std::string> arg_lang;
  extern const command_line::arg_descriptor<bool> arg_min;
  extern const command_line::arg_descriptor<bool> arg_no_splash;
  extern const command_line::arg_descriptor<bool> arg_disable_wallet;
  extern const command_line::arg_descriptor<bool> arg_choose_data_dir;
  extern const command_line::arg_descriptor<std::string> arg_root_certificates;
  extern const command_line::arg_descriptor<std::string> arg_wallet_file;
}
