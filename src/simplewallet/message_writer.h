// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <functional>
#include <string>
#include <sstream>

#include "include_base_utils.h"

#include "common/types.h"

namespace crypto { POD_CLASS hash; }

class message_writer
{
public:
  message_writer(epee::log_space::console_colors color = epee::log_space::console_color_default, bool bright = false,
                 std::string&& prefix = std::string(), int log_level = LOG_LEVEL_2);

  message_writer(message_writer&& rhs);

  template<typename T>
  std::ostream& operator<<(const T& val)
  {
    m_oss << val;
    return m_oss;
  }

  ~message_writer();

private:
  message_writer(message_writer& rhs);
  message_writer& operator=(message_writer& rhs);
  message_writer& operator=(message_writer&& rhs);

private:
  bool m_flush;
  std::stringstream m_oss;
  epee::log_space::console_colors m_color;
  bool m_bright;
  int m_log_level;
};

message_writer success_msg_writer(bool color = false);
message_writer fail_msg_writer();

void capturing_exceptions(const std::function<void()>&& f);
