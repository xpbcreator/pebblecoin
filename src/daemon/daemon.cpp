// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// node.cpp : Defines the entry point for the console application.
//

#if defined(WIN32)
#include <crtdbg.h>
#endif

#include <boost/program_options.hpp>

#include "include_base_utils.h"
#include "console_handler.h"

#include "cryptonote_genesis_config.h"
#include "common/types.h"
#include "common/ntp_time.h"
#include "crypto/hash_options.h"
#include "crypto/hash_cache.h"
#include "p2p/net_node.h"
#include "cryptonote_core/checkpoints_create.h"
#include "cryptonote_core/cryptonote_core.h"
#include "cryptonote_core/miner_opt.h"

#include "rpc/core_rpc_server.h"
#include "cryptonote_protocol/cryptonote_protocol_handler.h"
#include "daemon_options.h"
#include "daemon_commands_handler.h"

using namespace epee;
using namespace daemon_opt;
namespace po = boost::program_options;

bool command_line_preprocessor(const boost::program_options::variables_map& vm);

int main(int argc, char* argv[])
{
  string_tools::set_module_name_and_folder(argv[0]);
#ifdef WIN32
  _CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif
  log_space::get_set_log_detalisation_level(true, LOG_LEVEL_0);
  log_space::log_singletone::add_logger(LOGGER_CONSOLE, NULL, NULL);
  LOG_PRINT_L0("Starting...");

  TRY_ENTRY();

  po::options_description desc_cmd_only("Command line options");
  po::options_description desc_cmd_sett("Command line options and settings options");

  command_line::add_arg(desc_cmd_only, command_line::arg_help);
  command_line::add_arg(desc_cmd_only, command_line::arg_version);
  command_line::add_arg(desc_cmd_only, arg_os_version);
  command_line::add_arg(desc_cmd_only, command_line::arg_data_dir);
  command_line::add_arg(desc_cmd_only, arg_config_file);

  command_line::add_arg(desc_cmd_sett, arg_log_file);
  command_line::add_arg(desc_cmd_sett, arg_log_level);
  command_line::add_arg(desc_cmd_sett, arg_console);
  command_line::add_arg(desc_cmd_sett, arg_testnet_on);

  cryptonote_opt::init_options(desc_cmd_sett);
  core_t::init_options(desc_cmd_sett);
  rpc_server_t::init_options(desc_cmd_sett);
  node_server_t::init_options(desc_cmd_sett);
  cryptonote::miner::init_options(desc_cmd_sett);
  crypto::init_options(desc_cmd_sett);
  
  po::options_description desc_options("Allowed options");
  desc_options.add(desc_cmd_only).add(desc_cmd_sett);
  po::variables_map vm;
  bool r = command_line::handle_error_helper(desc_options, [&]()
  {
    po::store(po::parse_command_line(argc, argv, desc_options), vm);

    if (command_line::get_arg(vm, arg_testnet_on) || cryptonote::config::testnet_only)
    {
      cryptonote::config::enable_testnet();
    }
    
    if (command_line::get_arg(vm, command_line::arg_help))
    {
      std::cout << tools::get_project_description("daemon") << ENDL << ENDL;
      std::cout << desc_options << std::endl;
      return false;
    }

    std::string data_dir = command_line::get_data_dir(vm);
    std::string config = command_line::get_arg(vm, arg_config_file);

    boost::filesystem::path data_dir_path(data_dir);
    boost::filesystem::path config_path(config);
    if (!config_path.has_parent_path())
    {
      config_path = data_dir_path / config_path;
    }

    boost::system::error_code ec;
    if (boost::filesystem::exists(config_path, ec))
    {
      po::store(po::parse_config_file<char>(config_path.string<std::string>().c_str(), desc_cmd_sett), vm);
    }

    // see if there's testnet in the config file
    if (command_line::get_arg(vm, arg_testnet_on) || cryptonote::config::testnet_only)
    {
      cryptonote::config::enable_testnet();
    }
    
    po::notify(vm);

    return true;
  });
  if (!r)
    return 1;
  
  //set up logging options
  boost::filesystem::path log_file_path(command_line::get_arg(vm, arg_log_file));
  if (log_file_path.empty())
    log_file_path = log_space::log_singletone::get_default_log_file();
  std::string log_dir;
  log_dir = log_file_path.has_parent_path() ? log_file_path.parent_path().string() : log_space::log_singletone::get_default_log_folder();

  log_space::log_singletone::add_logger(LOGGER_FILE, log_file_path.filename().string().c_str(), log_dir.c_str());
  LOG_PRINT_L0(tools::get_project_description("daemon"));

  if (command_line_preprocessor(vm))
  {
    return 0;
  }
  
  LOG_PRINT("Module folder: " << argv[0], LOG_LEVEL_0);
  
  if (!crypto::process_options(vm, command_line::has_arg(vm, miner_opt::arg_start_mining)))
    return 1;
  
  if (!cryptonote_opt::handle_command_line(vm))
    return 1;
    
  bool res = true;
  cryptonote::checkpoints checkpoints;
  res = cryptonote::create_checkpoints(checkpoints);
  CHECK_AND_ASSERT_MES(res, 1, "Failed to initialize checkpoints");

  //create objects and link them
  LOG_PRINT_L0("Initializing global hash cache...");
  res = crypto::g_hash_cache.init(command_line::get_data_dir(vm));
  CHECK_AND_ASSERT_MES(res, 1, "Failed to initialize global hash cache.");
  LOG_PRINT_L0("Global hash cache initialized OK...");
  
  tools::ntp_time ntp(60*60);
  core_t ccore(NULL, ntp);
  ccore.set_checkpoints(std::move(checkpoints));
  protocol_handler_t cprotocol(ccore, NULL);
  node_server_t p2psrv(cprotocol);
  rpc_server_t rpc_server(ccore, p2psrv);
  cprotocol.set_p2p_endpoint(&p2psrv);
  ccore.set_cryptonote_protocol(&cprotocol);
  daemon_cmmands_handler dch(p2psrv);
  
  //initialize objects
  LOG_PRINT_L0("Initializing shared boulderhash state...");
  crypto::g_boulderhash_state = crypto::pc_malloc_state();
  LOG_PRINT_L0("Shared boulderhash state initialized OK");
  
  LOG_PRINT_L0("Initializing boulderhash threadpool...");
  crypto::pc_init_threadpool(vm);
  
  LOG_PRINT_L0("Initializing p2p server...");
  res = p2psrv.init(vm);
  CHECK_AND_ASSERT_MES(res, 1, "Failed to initialize p2p server.");
  LOG_PRINT_L0("P2p server initialized OK");

  LOG_PRINT_L0("Initializing cryptonote protocol...");
  res = cprotocol.init(vm);
  CHECK_AND_ASSERT_MES(res, 1, "Failed to initialize cryptonote protocol.");
  LOG_PRINT_L0("Cryptonote protocol initialized OK");

  LOG_PRINT_L0("Initializing core rpc server...");
  res = rpc_server.init(vm);
  CHECK_AND_ASSERT_MES(res, 1, "Failed to initialize core rpc server.");
  LOG_PRINT_GREEN("Core rpc server initialized OK on port: " << rpc_server.get_binded_port(), LOG_LEVEL_0);

  //initialize core here
  LOG_PRINT_L0("Initializing core...");
  res = ccore.init(vm);
  CHECK_AND_ASSERT_MES(res, 1, "Failed to initialize core");
  LOG_PRINT_L0("Core initialized OK");

  // start components
  if(!command_line::has_arg(vm, arg_console))
  {
    dch.start_handling();
  }

  LOG_PRINT_L0("Starting core rpc server...");
  res = rpc_server.run(2, false);
  CHECK_AND_ASSERT_MES(res, 1, "Failed to initialize core rpc server.");
  LOG_PRINT_L0("Core rpc server started ok");

  tools::signal_handler::install([&dch, &p2psrv] {
    dch.stop_handling();
    p2psrv.send_stop_signal();
  });

  LOG_PRINT_L0("Starting p2p net loop...");
  p2psrv.run();
  LOG_PRINT_L0("p2p net loop stopped");

  //stop components
  LOG_PRINT_L0("Stopping core rpc server...");
  rpc_server.send_stop_signal();
  rpc_server.timed_wait_server_stop(5000);

  //deinitialize components
  LOG_PRINT_L0("Deinitializing core...");
  ccore.deinit();
  LOG_PRINT_L0("Deinitializing rpc server ...");
  rpc_server.deinit();
  LOG_PRINT_L0("Deinitializing cryptonote_protocol...");
  cprotocol.deinit();
  LOG_PRINT_L0("Deinitializing p2p...");
  p2psrv.deinit();

  ccore.set_cryptonote_protocol(NULL);
  cprotocol.set_p2p_endpoint(NULL);

  LOG_PRINT_L0("Stopping boulderhash threadpool...");
  crypto::pc_stop_threadpool();

  LOG_PRINT_L0("Deinitializing shared boulderhash state...");
  crypto::pc_free_state(crypto::g_boulderhash_state);

  LOG_PRINT_L0("Deinitializing global hash cache...");
  crypto::g_hash_cache.deinit();
  
  LOG_PRINT("Node stopped.", LOG_LEVEL_0);
  return 0;

  CATCH_ENTRY_L0("main", 1);
}

bool command_line_preprocessor(const boost::program_options::variables_map& vm)
{
  bool exit = false;
  
  if (command_line::get_arg(vm, command_line::arg_version))
  {
    std::cout << tools::get_project_description("daemon") << ENDL;
    exit = true;
  }
  if (command_line::get_arg(vm, arg_os_version))
  {
    std::cout << "OS: " << tools::get_os_version_string() << ENDL;
    exit = true;
  }

  if (exit)
  {
    return true;
  }

  int new_log_level = command_line::get_arg(vm, arg_log_level);
  if(new_log_level < LOG_LEVEL_MIN || new_log_level > LOG_LEVEL_MAX)
  {
    LOG_PRINT_L0("Wrong log level value: ");
  }
  else if (log_space::get_set_log_detalisation_level(false) != new_log_level)
  {
    log_space::get_set_log_detalisation_level(true, new_log_level);
    LOG_PRINT_L0("LOG_LEVEL set to " << new_log_level);
  }
  
  return false;
}
