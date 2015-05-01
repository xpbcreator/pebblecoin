#include "qt_options.h"
#include "include_base_utils.h"

namespace qt_opt
{
  const command_line::arg_descriptor<std::string> arg_lang = {"lang", "Specify language (e.g. de_DE)", "", true};
  const command_line::arg_descriptor<bool> arg_min = {"min", "Whether to start minimized", false};
  const command_line::arg_descriptor<bool> arg_no_splash = {"nosplash", "Disable the splash screen", false};
  const command_line::arg_descriptor<bool> arg_disable_wallet = {"disable-wallet", "Whether to run without the wallet", false};
  const command_line::arg_descriptor<bool> arg_choose_data_dir = {"choose-data-dir", "Force choosing a data directory", false};
  const command_line::arg_descriptor<std::string> arg_root_certificates = {"root-certificates", "Choose SSL root certificates", "-system-"};
  const command_line::arg_descriptor<std::string> arg_wallet_file = {"wallet-file", "Use wallet <arg>", "wallet"};
}