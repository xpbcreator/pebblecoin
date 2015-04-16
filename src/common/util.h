// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once 

#if defined(WIN32)
#include <windows.h>
#else
#include <signal.h>
#endif

#include <mutex>
#include <system_error>

#include "misc_language.h"

namespace tools
{
  std::string get_default_data_dir();
  std::string get_os_version_string();
  bool create_directories_if_necessary(const std::string& path);
  std::error_code replace_file(const std::string& replacement_name, const std::string& replaced_name);
  std::string get_project_description(const std::string& module="");

  #ifdef WIN32
  std::string get_special_folder_path(int nfolder, bool iscreate);
  #endif

  class signal_handler
  {
  public:
    template<typename T>
    static bool install(T t)
    {
#if defined(WIN32)
      bool r = TRUE == ::SetConsoleCtrlHandler(&win_handler, TRUE);
      if (r)
      {
        m_handler = t;
      }
      return r;
#else
      signal(SIGINT, posix_handler);
      signal(SIGTERM, posix_handler);
      m_handler = t;
      return true;
#endif
    }

  private:
#if defined(WIN32)
    static BOOL WINAPI win_handler(DWORD type);
#else
    static void posix_handler(int /*type*/);
#endif

    static void handle_signal()
    {
      static std::mutex m_mutex;
      std::unique_lock<std::mutex> lock(m_mutex);
      m_handler();
    }

  private:
    static std::function<void(void)> m_handler;
  };
}
