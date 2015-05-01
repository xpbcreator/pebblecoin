// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <string>
#include <vector>
#if defined(WIN32)
#include <crtdbg.h>
#endif

#include <boost/program_options.hpp>

#include "console_handler.h"

#include "cryptonote_genesis_config.h"
#include "common/util.h"
#include "common/command_line.h"
#include "wallet/wallet_rpc_server.h"
#include "wallet/wallet2.h"

#include "message_writer.h"
#include "password_container.h"
#include "simplewallet.h"


using namespace std;
using namespace epee;
using namespace cryptonote;
namespace po = boost::program_options;

namespace
{
  const command_line::arg_descriptor<std::string> arg_wallet_file = {"wallet-file", "Use wallet <arg>", ""};
  const command_line::arg_descriptor<std::string> arg_generate_new_wallet = {"generate-new-wallet", "Generate new wallet and save it to <arg> or <address>.wallet by default", ""};
  const command_line::arg_descriptor<std::string> arg_daemon_address = {"daemon-address", "Use daemon instance at <host>:<port>", ""};
  const command_line::arg_descriptor<std::string> arg_daemon_host = {"daemon-host", "Use daemon instance at host <arg> instead of localhost", ""};
  const command_line::arg_descriptor<std::string> arg_password = {"password", "Wallet password", "", true};
  const command_line::arg_descriptor<int>         arg_daemon_port = {"daemon-port", "Use daemon instance at port <arg> instead of default", 0};
  const command_line::arg_descriptor<uint32_t>    arg_log_level = {"set_log", "", 0, true};
  const command_line::arg_descriptor<bool>        arg_testnet_on  = {"testnet", "Enable testnet"};

  const command_line::arg_descriptor< std::vector<std::string> > arg_command = {"command", ""};
}

std::string simple_wallet::get_commands_str()
{
  std::stringstream ss;
  ss << "Commands: " << ENDL;
  std::string usage = m_pcmd_binder->get_usage();
  boost::replace_all(usage, "\n", "\n  ");
  usage.insert(0, "  ");
  ss << usage << ENDL;
  return ss.str();
}

bool simple_wallet::help(const std::vector<std::string> &args)
{
  success_msg_writer() << get_commands_str();
  return true;
}

simple_wallet::simple_wallet()
  : m_daemon_port(0)
  , m_refresh_progress_reporter(*this)
  , m_pcmd_binder(new epee::console_handlers_binder())
{
  init_http_client();
  auto& m_cmd_binder = *m_pcmd_binder;
  m_cmd_binder.set_handler("start_mining", boost::bind(&simple_wallet::start_mining, this, _1), "start_mining [<number_of_threads>] - Start mining in daemon");
  m_cmd_binder.set_handler("stop_mining", boost::bind(&simple_wallet::stop_mining, this, _1), "Stop mining in daemon");
  m_cmd_binder.set_handler("refresh", boost::bind(&simple_wallet::refresh, this, _1), "Resynchronize transactions and balance");
  m_cmd_binder.set_handler("balance", boost::bind(&simple_wallet::show_balance, this, _1), "Show current wallet balance");
  m_cmd_binder.set_handler("incoming_transfers", boost::bind(&simple_wallet::show_incoming_transfers, this, _1), "incoming_transfers [available|unavailable] - Show incoming transfers - all of them or filter them by availability");
  m_cmd_binder.set_handler("payments", boost::bind(&simple_wallet::show_payments, this, _1), "payments <payment_id_1> [<payment_id_2> ... <payment_id_N>] - Show payments <payment_id_1>, ... <payment_id_N>");
  m_cmd_binder.set_handler("bc_height", boost::bind(&simple_wallet::show_blockchain_height, this, _1), "Show blockchain height");
  m_cmd_binder.set_handler("transfer", boost::bind(&simple_wallet::transfer, this, _1, CURRENCY_XPB), "transfer <min_mixin_count> <mixin_count> <addr_1> <amount_1> [<addr_2> <amount_2> ... <addr_N> <amount_N>] [payment_id] - Transfer <amount_1>,... <amount_N> to <address_1>,... <address_N>, respectively. <mixin_count> is the number of transactions yours is indistinguishable from (from 0 to maximum available), <min_mixin_count> is the minimum acceptable mixin count.");
  //m_cmd_binder.set_handler("transfer_currency", boost::bind(&simple_wallet::transfer_currency, this, _1), "transfer <currency_id_hex> <mixin_count> <addr_1> <amount_1> [<addr_2> <amount_2> ... <addr_N> <amount_N>] [payment_id] - Transfer <amount_1>,... <amount_N> to <address_1>,... <address_N>, respectively. <currency_id_hex> is the hex of the currency id to transfer. <mixin_count> is the number of transactions yours is indistinguishable from (from 0 to maximum available)");
  //m_cmd_binder.set_handler("mint", boost::bind(&simple_wallet::mint, this, _1), "mint <currency_id_hex> <amount> <fee mixin_count> [<decimals=2> <description=""> <remintable=true>]");
  //m_cmd_binder.set_handler("remint", boost::bind(&simple_wallet::remint, this, _1), "remint <currency_id_hex> <amount> <fee mixin_count> [<keep_remintable=true>]");
  m_cmd_binder.set_handler("register_delegate", boost::bind(&simple_wallet::register_delegate, this, _1), "register_delegate <delegate_id> <registration_fee> [<min_fake_outs=5> <fake_outs=5> [<address>]] - Register address (default wallet address) as a delegate. <delegate_id> is a number 1-65535. Fee is minimum 5 XPB. ");
  m_cmd_binder.set_handler("set_log", boost::bind(&simple_wallet::set_log, this, _1), "set_log <level> - Change current log detalization level, <level> is a number 0-4");
  m_cmd_binder.set_handler("address", boost::bind(&simple_wallet::print_address, this, _1), "Show current wallet public address");
  m_cmd_binder.set_handler("save", boost::bind(&simple_wallet::save, this, _1), "Save wallet synchronized data");
  m_cmd_binder.set_handler("help", boost::bind(&simple_wallet::help, this, _1), "Show this help");
  m_cmd_binder.set_handler("debug_batches", boost::bind(&simple_wallet::debug_batches, this, _1),
                           "DEBUG: print transfer batches");

  m_cmd_binder.set_handler("list_delegates", boost::bind(&simple_wallet::list_delegates, this, _1),
                           "List the delegates you are currently voting for");
  m_cmd_binder.set_handler("enable_autovote", boost::bind(&simple_wallet::enable_autovote, this, _1),
                           "Enable voting for automatically-selected delegates");
  m_cmd_binder.set_handler("disable_autovote", boost::bind(&simple_wallet::disable_autovote, this, _1),
                           "Disable voting for automatically-selected delegates");
  m_cmd_binder.set_handler("add_delegates", boost::bind(&simple_wallet::add_delegates, this, _1),
                           "add_delegates [<delegate_id> [<delegate_id> [...] ]] - Add delegates to the user voting set");
  m_cmd_binder.set_handler("remove_delegates", boost::bind(&simple_wallet::remove_delegates, this, _1),
                           "remove_delegates [<delegate_id> [<delegate_id> [...] ]] - Remove delegates from the user voting set");
  m_cmd_binder.set_handler("set_delegates", boost::bind(&simple_wallet::set_delegates, this, _1),
                           "set_delegates [<delegate_id> [<delegate_id> [...] ]] - Set the user voting set to the given delegates");
  
  
}
//----------------------------------------------------------------------------------------------------
simple_wallet::~simple_wallet()
{
  delete m_pcmd_binder; m_pcmd_binder = NULL;
  destroy_http_client();
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::set_log(const std::vector<std::string> &args)
{
  if(args.size() != 1)
  {
    fail_msg_writer() << "use: set_log <log_level_number_0-4>";
    return true;
  }
  uint16_t l = 0;
  if(!epee::string_tools::get_xtype_from_string(l, args[0]))
  {
    fail_msg_writer() << "wrong number format, use: set_log <log_level_number_0-4>";
    return true;
  }
  if(LOG_LEVEL_4 < l)
  {
    fail_msg_writer() << "wrong number range, use: set_log <log_level_number_0-4>";
    return true;
  }

  log_space::log_singletone::get_set_log_detalisation_level(true, l);
  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::ask_wallet_create_if_needed()
{
  std::string wallet_path;

  std::cout << "Specify wallet file name (e.g., wallet.bin). If the wallet doesn't exist, it will be created.\n";
  std::cout << "Wallet file name: ";

  std::getline(std::cin, wallet_path);

  wallet_path = string_tools::trim(wallet_path);

  bool keys_file_exists;
  bool wallet_file_exists;
  tools::wallet2::wallet_exists(wallet_path, keys_file_exists, wallet_file_exists);

  bool r;
  if(keys_file_exists)
  {
    m_wallet_file = wallet_path;
    r = true;
  }else
  {
    if(!wallet_file_exists)
    {
      std::cout << "The wallet doesn't exist, generating new one" << std::endl;
      m_generate_new = wallet_path;
      r = true;
    }else
    {
      fail_msg_writer() << "failed to open wallet \"" << wallet_path << "\". Keys file wasn't found";
      r = false;
    }
  }

  return r;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::try_connect_to_daemon()
{
  if (!m_wallet->check_connection())
  {
    fail_msg_writer() << "wallet failed to connect to daemon (" << m_daemon_address << "). " <<
      "Daemon either is not started or passed wrong port. " <<
      "Please, make sure that daemon is running or restart the wallet with correct daemon address.";
    return false;
  }
  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::new_wallet(const string &wallet_file, const std::string& password)
{
  m_wallet_file = wallet_file;

  m_wallet.reset(new tools::wallet2());
  m_wallet->callback(this);
  try
  {
    m_wallet->generate(wallet_file, password);
    message_writer(epee::log_space::console_color_white, true) << "Generated new wallet: " << m_wallet->get_account().get_public_address_str() << std::endl << "view key: " << string_tools::pod_to_hex(m_wallet->get_account().get_keys().m_view_secret_key);
  }
  catch (const std::exception& e)
  {
    fail_msg_writer() << "failed to generate new wallet: " << e.what();
    return false;
  }

  m_wallet->init(m_daemon_address);

  success_msg_writer() <<
    "**********************************************************************\n" <<
    "Your wallet has been generated.\n" <<
    "To start synchronizing with the daemon use \"refresh\" command.\n" <<
    "Use \"help\" command to see the list of available commands.\n" <<
    "Always use \"exit\" command when closing simplewallet to save\n" <<
    "current session's state. Otherwise, you will possibly need to synchronize \n" <<
    "your wallet again. Your wallet key is NOT under risk anyway.\n" <<
    "**********************************************************************";
  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::open_wallet(const string &wallet_file, const std::string& password)
{
  m_wallet_file = wallet_file;
  m_wallet.reset(new tools::wallet2());
  m_wallet->callback(this);

  try
  {
    m_wallet->load(m_wallet_file, password);
    message_writer(epee::log_space::console_color_white, true) << "Opened wallet: " << m_wallet->get_account().get_public_address_str();
  }
  catch (const std::exception& e)
  {
    fail_msg_writer() << "failed to load wallet: " << e.what();
    return false;
  }

  m_wallet->init(m_daemon_address);

  refresh(std::vector<std::string>());
  success_msg_writer() <<
    "**********************************************************************\n" <<
    "Use \"help\" command to see the list of available commands.\n" <<
    "**********************************************************************";
  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::close_wallet()
{
  bool r = m_wallet->deinit();
  if (!r)
  {
    fail_msg_writer() << "failed to deinit wallet";
    return false;
  }

  try
  {
    m_wallet->store();
  }
  catch (const std::exception& e)
  {
    fail_msg_writer() << e.what();
    return false;
  }

  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::save(const std::vector<std::string> &args)
{
  try
  {
    m_wallet->store();
    success_msg_writer() << "Wallet data saved";
  }
  catch (const std::exception& e)
  {
    fail_msg_writer() << e.what();
  }

  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::init(const boost::program_options::variables_map& vm)
{
  handle_command_line(vm);

  if (!m_daemon_address.empty() && !m_daemon_host.empty() && 0 != m_daemon_port)
  {
    fail_msg_writer() << "you can't specify daemon host or port several times";
    return false;
  }

  size_t c = 0;
  if(!m_generate_new.empty()) ++c;
  if(!m_wallet_file.empty()) ++c;
  if (1 != c)
  {
    if(!ask_wallet_create_if_needed())
      return false;
  }

  if (m_daemon_host.empty())
    m_daemon_host = "localhost";
  if (!m_daemon_port)
    m_daemon_port = cryptonote::config::rpc_default_port();
  if (m_daemon_address.empty())
    m_daemon_address = std::string("http://") + m_daemon_host + ":" + std::to_string(m_daemon_port);

  tools::password_container pwd_container;
  if (command_line::has_arg(vm, arg_password))
  {
    pwd_container.password(command_line::get_arg(vm, arg_password));
  }
  else
  {
    bool r = pwd_container.read_password();
    if (!r)
    {
      fail_msg_writer() << "failed to read wallet password";
      return false;
    }
  }

  if (!m_generate_new.empty())
  {
    bool r = new_wallet(m_generate_new, pwd_container.password());
    CHECK_AND_ASSERT_MES(r, false, "account creation failed");
  }
  else
  {
    bool r = open_wallet(m_wallet_file, pwd_container.password());
    CHECK_AND_ASSERT_MES(r, false, "could not open account");
  }

  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::deinit()
{
  if (!m_wallet.get())
    return true;

  return close_wallet();
}
//----------------------------------------------------------------------------------------------------
void simple_wallet::init_options(po::options_description& desc_params)
{
  command_line::add_arg(desc_params, arg_wallet_file);
  command_line::add_arg(desc_params, arg_generate_new_wallet);
  command_line::add_arg(desc_params, arg_password);
  command_line::add_arg(desc_params, arg_daemon_address);
  command_line::add_arg(desc_params, arg_daemon_host);
  command_line::add_arg(desc_params, arg_daemon_port);
  command_line::add_arg(desc_params, arg_command);
  command_line::add_arg(desc_params, arg_log_level);
  command_line::add_arg(desc_params, arg_testnet_on);
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::parse_command_line(int argc, char* argv[], po::variables_map& vm)
{
  string_tools::set_module_name_and_folder(argv[0]);

  po::options_description desc_general("General options");
  command_line::add_arg(desc_general, command_line::arg_help);
  command_line::add_arg(desc_general, command_line::arg_version);

  po::options_description desc_params("Wallet options");
  simple_wallet::init_options(desc_params);
  tools::wallet_rpc_server::init_options(desc_params);
  cryptonote_opt::init_options(desc_params);

  po::positional_options_description positional_options;
  positional_options.add(arg_command.name, -1);

  po::options_description desc_all;
  desc_all.add(desc_general).add(desc_params);
  
  return command_line::handle_error_helper(desc_all, [&]()
  {
    po::store(command_line::parse_command_line(argc, argv, desc_all, true), vm);

    if (command_line::get_arg(vm, arg_testnet_on) || cryptonote::config::testnet_only)
    {
      cryptonote::config::enable_testnet();
    }
    
    if (command_line::get_arg(vm, command_line::arg_help))
    {
      cryptonote::simple_wallet w;
      success_msg_writer() << tools::get_project_description("wallet");
      success_msg_writer() << "Usage: simplewallet [--wallet-file=<file>|--generate-new-wallet=<file>] [--daemon-address=<host>:<port>] [<COMMAND>]";
      success_msg_writer() << desc_all << '\n' << w.get_commands_str();
      return false;
    }
    else if (command_line::get_arg(vm, command_line::arg_version))
    {
      success_msg_writer() << tools::get_project_description("wallet");
      return false;
    }

    // parse positional options
    auto parser = po::command_line_parser(argc, argv).options(desc_all).positional(positional_options);
    po::store(parser.run(), vm);
    po::notify(vm);
    return true;
  });
}
//----------------------------------------------------------------------------------------------------
void simple_wallet::handle_command_line(const boost::program_options::variables_map& vm)
{
  m_wallet_file    = command_line::get_arg(vm, arg_wallet_file);
  m_generate_new   = command_line::get_arg(vm, arg_generate_new_wallet);
  m_daemon_address = command_line::get_arg(vm, arg_daemon_address);
  m_daemon_host    = command_line::get_arg(vm, arg_daemon_host);
  m_daemon_port    = command_line::get_arg(vm, arg_daemon_port);
}
//----------------------------------------------------------------------------------------------------
void simple_wallet::setup_logging(po::variables_map& vm)
{
  //set up logging options
  log_space::get_set_log_detalisation_level(true, LOG_LEVEL_2);
  //log_space::log_singletone::add_logger(LOGGER_CONSOLE, NULL, NULL, LOG_LEVEL_4);
  log_space::log_singletone::add_logger(LOGGER_FILE,
    log_space::log_singletone::get_default_log_file().c_str(),
    log_space::log_singletone::get_default_log_folder().c_str(), LOG_LEVEL_4);
  
  message_writer(log_space::console_color_white, true) << tools::get_project_description("wallet");
  message_writer(log_space::console_color_white, true) << "Logging to file " << log_space::log_singletone::get_default_log_file() << " in folder " << log_space::log_singletone::get_default_log_folder();
  
  if(command_line::has_arg(vm, arg_log_level))
  {
    LOG_PRINT_L0("Setting log level = " << command_line::get_arg(vm, arg_log_level));
    log_space::get_set_log_detalisation_level(true, command_line::get_arg(vm, arg_log_level));
  }
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::run()
{
  std::string addr_start = m_wallet->get_account().get_public_address_str().substr(0, 6);
  return m_pcmd_binder->run_handling("[wallet " + addr_start + "]: ", "");
}
//----------------------------------------------------------------------------------------------------
void simple_wallet::stop()
{
  m_pcmd_binder->stop_handling();
  m_wallet->stop();
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::process_command(const std::vector<std::string> &args)
{
  return m_pcmd_binder->process_command_vec(args);
}
//----------------------------------------------------------------------------------------------------


int main(int argc, char* argv[])
{
#ifdef WIN32
  _CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

  //TRY_ENTRY();
  
  po::variables_map vm;
  if (!simple_wallet::parse_command_line(argc, argv, vm))
    return 1;
  
  simple_wallet::setup_logging(vm);
  
  if (!cryptonote_opt::handle_command_line(vm))
  {
    return 1;
  }

  if(command_line::has_arg(vm, tools::wallet_rpc_server::arg_rpc_bind_port))
  {
    log_space::log_singletone::add_logger(LOGGER_CONSOLE, NULL, NULL, LOG_LEVEL_2);
    //runs wallet with rpc interface
    if(!command_line::has_arg(vm, arg_wallet_file) )
    {
      LOG_ERROR("Wallet file not set.");
      return 1;
    }
    if(!command_line::has_arg(vm, arg_daemon_address) )
    {
      LOG_ERROR("Daemon address not set.");
      return 1;
    }
    if(!command_line::has_arg(vm, arg_password) )
    {
      LOG_ERROR("Wallet password not set.");
      return 1;
    }

    std::string wallet_file     = command_line::get_arg(vm, arg_wallet_file);
    std::string wallet_password = command_line::get_arg(vm, arg_password);
    std::string daemon_address  = command_line::get_arg(vm, arg_daemon_address);
    std::string daemon_host = command_line::get_arg(vm, arg_daemon_host);
    int daemon_port = command_line::get_arg(vm, arg_daemon_port);
    if (daemon_host.empty())
      daemon_host = "localhost";
    if (!daemon_port)
      daemon_port = cryptonote::config::rpc_default_port();
    if (daemon_address.empty())
      daemon_address = std::string("http://") + daemon_host + ":" + std::to_string(daemon_port);

    tools::wallet2 wal;
    try
    {
      LOG_PRINT_L0("Loading wallet...");
      wal.load(wallet_file, wallet_password);
      wal.init(daemon_address);
      wal.refresh();
      LOG_PRINT_GREEN("Loaded ok", LOG_LEVEL_0);
    }
    catch (const std::exception& e)
    {
      LOG_ERROR("Wallet initialize failed: " << e.what());
      return 1;
    }
    tools::wallet_rpc_server wrpc(wal);
    bool r = wrpc.init(vm);
    CHECK_AND_ASSERT_MES(r, 1, "Failed to initialize wallet rpc server");

    tools::signal_handler::install([&wrpc, &wal] {
      wrpc.send_stop_signal();
      wal.store();
    });
    LOG_PRINT_L0("Starting wallet rpc server");
    wrpc.run();
    LOG_PRINT_L0("Stopped wallet rpc server");
    try
    {
      LOG_PRINT_L1("Storing wallet...");
      wal.store();
      LOG_PRINT_GREEN("Stored ok", LOG_LEVEL_1);
    }
    catch (const std::exception& e)
    {
      LOG_ERROR("Failed to store wallet: " << e.what());
      return 1;
    }
  }else
  {
    cryptonote::simple_wallet w;
    //runs wallet with console interface
    bool r = w.init(vm);
    CHECK_AND_ASSERT_MES(r, 1, "Failed to initialize wallet");

    std::vector<std::string> command = command_line::get_arg(vm, arg_command);
    if (!command.empty())
      w.process_command(command);

    tools::signal_handler::install([&w] {
      w.stop();
    });
    w.run();

    w.deinit();
  }
  return 1;
  //CATCH_ENTRY_L0("main", 1);
}
