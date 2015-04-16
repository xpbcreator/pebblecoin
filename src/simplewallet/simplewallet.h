// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <memory>
#include <vector>
#include <string>

namespace boost {
  namespace program_options {
    class options_description;
    class variables_map;
  }
}

namespace epee {
  class console_handlers_binder;
  namespace net_utils {
    namespace http {
      class http_simple_client;
    }
  }
}

#include "wallet/i_wallet2_callback.h"

namespace tools {
  class wallet2;
}

namespace cryptonote
{
  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  class simple_wallet : public tools::i_wallet2_callback
  {
  public:
    typedef std::vector<std::string> command_type;

    simple_wallet();
    ~simple_wallet();
    
    bool init(const boost::program_options::variables_map& vm);
    bool deinit();
    bool run();
    void stop();

    //wallet *create_wallet();
    bool process_command(const std::vector<std::string> &args);
    std::string get_commands_str();
    
    static void init_options(boost::program_options::options_description& desc_params);
    static bool parse_command_line(int argc, char* argv[], boost::program_options::variables_map& vm);
    static void setup_logging(boost::program_options::variables_map& vm);

  private:
    void init_http_client();
    void destroy_http_client();
    
    void handle_command_line(const boost::program_options::variables_map& vm);

    bool run_console_handler();

    bool new_wallet(const std::string &wallet_file, const std::string& password);
    bool open_wallet(const std::string &wallet_file, const std::string& password);
    bool close_wallet();

    bool help(const std::vector<std::string> &args = std::vector<std::string>());
    bool start_mining(const std::vector<std::string> &args);
    bool stop_mining(const std::vector<std::string> &args);
    bool refresh(const std::vector<std::string> &args);
    bool show_balance(const std::vector<std::string> &args = std::vector<std::string>());
    bool show_incoming_transfers(const std::vector<std::string> &args);
    bool show_payments(const std::vector<std::string> &args);
    bool show_blockchain_height(const std::vector<std::string> &args);
    bool transfer(const std::vector<std::string> &args, uint64_t currency_id);
    bool transfer_currency(const std::vector<std::string> &args);
    bool mint(const std::vector<std::string> &args);
    bool remint(const std::vector<std::string> &args);
    bool register_delegate(const std::vector<std::string> &args);
    bool print_address(const std::vector<std::string> &args = std::vector<std::string>());
    bool save(const std::vector<std::string> &args);
    bool set_log(const std::vector<std::string> &args);
    bool debug_batches(const std::vector<std::string> &args);
    bool list_delegates(const std::vector<std::string> &args);
    bool enable_autovote(const std::vector<std::string> &args);
    bool disable_autovote(const std::vector<std::string> &args);
    bool add_delegates(const std::vector<std::string> &args);
    bool remove_delegates(const std::vector<std::string> &args);
    bool set_delegates(const std::vector<std::string> &args);
    
    uint64_t get_daemon_blockchain_height(std::string& err);
    bool try_connect_to_daemon();
    bool ask_wallet_create_if_needed();
    
    //----------------- i_wallet2_callback ---------------------
    virtual void on_new_block_processed(uint64_t height, const cryptonote::block& block);
    virtual void on_money_received(uint64_t height, const tools::wallet2_transfer_details& td);
    virtual void on_money_spent(uint64_t height, const tools::wallet2_transfer_details& td);
    virtual void on_skip_transaction(uint64_t height, const cryptonote::transaction& tx);
    //----------------------------------------------------------

    friend class refresh_progress_reporter_t;

    class refresh_progress_reporter_t
    {
    public:
      refresh_progress_reporter_t(cryptonote::simple_wallet& simple_wallet);
      ~refresh_progress_reporter_t();
      
      void update(uint64_t height, bool force = false);

    private:
      class impl;
      impl* m_pimpl;
    };

  private:
    std::string m_wallet_file;
    std::string m_generate_new;
    std::string m_import_path;

    std::string m_daemon_address;
    std::string m_daemon_host;
    int m_daemon_port;

    epee::console_handlers_binder *m_pcmd_binder;

    std::unique_ptr<tools::wallet2> m_wallet;
    epee::net_utils::http::http_simple_client *m_phttp_client;
    refresh_progress_reporter_t m_refresh_progress_reporter;
  };
}
