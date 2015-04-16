// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "command_line.h"
#include "util.h"

namespace command_line
{
  const arg_descriptor<bool> arg_help = {"help", "Produce help message"};
  const arg_descriptor<bool> arg_version = {"version", "Output version information"};
  const arg_descriptor<std::string> arg_data_dir = {"data-dir", "Specify data directory", "", true};

  std::string get_data_dir(const boost::program_options::variables_map& vm)
  {
    if (has_arg(vm, arg_data_dir))
    {
      return get_arg(vm, arg_data_dir);
    }
    
    return tools::get_default_data_dir();
  }
}
