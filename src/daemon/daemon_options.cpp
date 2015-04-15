#include "daemon_options.h"
#include "include_base_utils.h"
#include "cryptonote_config.h"

namespace daemon_opt
{
  const command_line::arg_descriptor<std::string> arg_config_file = {"config-file", "Specify configuration file", std::string(CRYPTONOTE_NAME) + ".conf"};
  const command_line::arg_descriptor<std::string> arg_pid_file    = {"pid-file", "Specify pid file", std::string(CRYPTONOTE_NAME) + "d.pid"};
  const command_line::arg_descriptor<bool>        arg_os_version  = {"os-version", ""};
  const command_line::arg_descriptor<std::string> arg_log_file    = {"log-file", "", ""};
  const command_line::arg_descriptor<int>         arg_log_level   = {"log-level", "", LOG_LEVEL_0};
  const command_line::arg_descriptor<bool>        arg_console     = {"no-console", "Disable daemon console commands"};
  const command_line::arg_descriptor<bool>        arg_testnet_on  = {"testnet", "Enable testnet"};
}
