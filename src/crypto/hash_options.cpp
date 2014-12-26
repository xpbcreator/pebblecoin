// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "hash_options.h"
#include "hash_cache.h"

#include "cryptonote_config.h"
#include "cryptonote_core/miner.h"

#include "string_tools.h"


namespace hashing_opt
{
  const command_line::arg_descriptor<bool>        arg_disable_signed_hashes = {"disable-signed-hashes", "Don't use the signed work hashes. Must enable-boulderhash", false};
  const command_line::arg_descriptor<bool>        arg_enable_boulderhash = {"enable-boulderhash", "Enable boulderhash to not have to rely on signed hashes (+13gb RAM)", true};
  const command_line::arg_descriptor<std::string> arg_hash_signing_priv_key = {"hash-signing-key", "Provide private key to sign proof-of-work hashes", "", true};
  const command_line::arg_descriptor<uint32_t>    arg_worker_threads =  {"worker-threads", "Specify boulderhash worker threadpool size (default: nproc)", 0, true};
  const command_line::arg_descriptor<uint32_t>    arg_states_per_thread =  {"states-per-thread", "Specify number of boulderhash states each worker thread should generate (default: 1)", 0, true};
  
  bool _use_signed_hashes = true;
#ifdef SMALL_BOULDERHASH
  bool _do_boulderhash = true;
#else
  bool _do_boulderhash = false;
#endif
}
using namespace hashing_opt;


namespace crypto
{
  void init_options(boost::program_options::options_description& desc)
  {
    command_line::add_arg(desc, arg_disable_signed_hashes);
    command_line::add_arg(desc, arg_enable_boulderhash);
    command_line::add_arg(desc, arg_hash_signing_priv_key);
    command_line::add_arg(desc, arg_worker_threads);
    command_line::add_arg(desc, arg_states_per_thread);
  }
  
  static bool set_hash_signing_key(boost::program_options::variables_map& vm)
  {
    if (!command_line::has_arg(vm, arg_hash_signing_priv_key))
      return true;
    
    secret_key prvk;
    if (!epee::string_tools::hex_to_pod(command_line::get_arg(vm, arg_hash_signing_priv_key), prvk))
    {
      LOG_PRINT_RED_L0("Wrong hash signing priv key set");
      return false;
    }
    
    return crypto::g_hash_cache.set_hash_signing_key(prvk);
  }
  
  bool process_options(boost::program_options::variables_map& vm)
  {
    if (!set_hash_signing_key(vm))
      return false;
    
    bool disable_signed = command_line::get_arg(vm, arg_disable_signed_hashes);
    bool enable_boulder = command_line::get_arg(vm, arg_enable_boulderhash);
    
    if (disable_signed && !enable_boulder)
    {
      LOG_PRINT_RED_L0("Must either enable boulderhash, or enable using signed hashes");
      return false;
    }
    
    if (!enable_boulder && command_line::has_arg(vm, miner_opt::arg_start_mining))
    {
      LOG_PRINT_RED_L0("Must enable boulderhash to mine");
      return false;
    }
  
    hashing_opt::_use_signed_hashes = !disable_signed;
    hashing_opt::_do_boulderhash = enable_boulder;
    
    return true;
  }
}

