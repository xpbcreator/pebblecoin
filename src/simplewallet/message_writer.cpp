// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "string_tools.h"

#include "crypto/hash.h"
#include "crypto/crypto_basic_impl.h"
#include "cryptonote_protocol/blobdatatype.h"
#include "wallet/wallet_errors.h"

#include "message_writer.h"


using namespace std;
using namespace epee;
using namespace cryptonote;


message_writer::message_writer(epee::log_space::console_colors color, bool bright,
                               std::string&& prefix, int log_level)
    : m_flush(true)
    , m_color(color)
    , m_bright(bright)
    , m_log_level(log_level)
{
  m_oss << prefix;
}

message_writer::message_writer(message_writer&& rhs)
    : m_flush(std::move(rhs.m_flush))
#if defined(_MSC_VER)
    , m_oss(std::move(rhs.m_oss))
#else
    // GCC bug: http://gcc.gnu.org/bugzilla/show_bug.cgi?id=54316
    , m_oss(rhs.m_oss.str(), ios_base::out | ios_base::ate)
#endif
    , m_color(std::move(rhs.m_color))
    , m_log_level(std::move(rhs.m_log_level))
{
  rhs.m_flush = false;
}

message_writer::~message_writer()
{
  if (m_flush)
  {
    m_flush = false;

    LOG_PRINT(m_oss.str(), m_log_level)

    if (epee::log_space::console_color_default == m_color)
    {
      std::cout << m_oss.str();
    }
    else
    {
      epee::log_space::set_console_color(m_color, m_bright);
      std::cout << m_oss.str();
      epee::log_space::reset_console_color();
    }
    std::cout << std::endl;
  }
}

message_writer success_msg_writer(bool color)
{
  return message_writer(color ? epee::log_space::console_color_green : epee::log_space::console_color_default, false, std::string(), LOG_LEVEL_2);
}

message_writer fail_msg_writer()
{
  return message_writer(epee::log_space::console_color_red, true, "Error: ", LOG_LEVEL_0);
}

void capturing_exceptions(const std::function<void()>&& f)
{
  try {
    f();
  }
  catch (const tools::error::daemon_busy&)
  {
    fail_msg_writer() << "daemon is busy. Please try later";
  }
  catch (const tools::error::no_connection_to_daemon&)
  {
    fail_msg_writer() << "no connection to daemon. Please, make sure daemon is running.";
  }
  catch (const tools::error::wallet_rpc_error& e)
  {
    LOG_ERROR("Unknown RPC error: " << e.to_string());
    fail_msg_writer() << "RPC error \"" << e.what() << '"';
  }
  catch (const tools::error::get_random_outs_error&)
  {
    fail_msg_writer() << "failed to get random outputs to mix";
  }
  catch (const tools::error::not_enough_money& e)
  {
    fail_msg_writer() << "not enough money to transfer with given mixin counts, for coin type " << e.coin_type()
        << ", available only " << print_money(e.available()) << ", transaction amount " << print_money(e.tx_amount() + e.fee())
        << " = " << print_money(e.tx_amount()) << " + " << print_money(e.fee()) << " (fee)"
        << ", filtered " << print_money(e.scanty_outs_amount()) << " from scanty outs";
  }
  catch (const tools::error::not_enough_outs_to_mix& e)
  {
    auto writer = fail_msg_writer();
    writer << "not enough outputs for specified mixin_count = " << e.mixin_count() << ":";
    BOOST_FOREACH(const auto& outs_for_amount, e.scanty_outs())
    {
      writer << "\noutput amount = " << print_money(outs_for_amount.amount) << ", fount outputs to mix = " << outs_for_amount.outs.size();
    }
  }
  catch (const tools::error::tx_not_constructed&)
  {
    fail_msg_writer() << "transaction was not constructed";
  }
  catch (const tools::error::tx_rejected& e)
  {
    fail_msg_writer() << "transaction " << get_transaction_hash(e.tx()) << " was rejected by daemon with status \""
        << e.status() << '"';
  }
  catch (const tools::error::tx_sum_overflow& e)
  {
    fail_msg_writer() << e.what();
  }
  catch (const tools::error::tx_too_big& e)
  {
    cryptonote::transaction tx = e.tx();
    fail_msg_writer() << "transaction " << get_transaction_hash(e.tx()) << " is too big. Transaction size: "
        << get_object_blobsize(e.tx()) << " bytes, transaction size limit: " << e.tx_size_limit() << " bytes";
  }
  catch (const tools::error::zero_destination&)
  {
    fail_msg_writer() << "one of destinations is zero";
  }
  catch (const tools::error::transfer_error& e)
  {
    LOG_ERROR("unknown transfer error: " << e.to_string());
    fail_msg_writer() << "unknown transfer error: " << e.what();
  }
  catch (const tools::error::wallet_internal_error& e)
  {
    LOG_ERROR("internal error: " << e.to_string());
    fail_msg_writer() << "internal error: " << e.what();
  }
  catch (const std::exception& e)
  {
    LOG_ERROR("unexpected error: " << e.what());
    fail_msg_writer() << "unexpected error: " << e.what();
  }
  catch (...)
  {
    LOG_ERROR("Unknown error");
    fail_msg_writer() << "unknown error";
  }
}
