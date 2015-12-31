// Copyright (c) 2015-2016 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

namespace sqlite3 {
  /// strings go through as-is
  std::string store_string(const std::string& s) { return s; }
  std::string load_string(const std::string& s) { return s; }
  
  /// simple pod serialization
  template<typename T>
  typename std::enable_if<std::is_pod<T>::value, std::string>::type
  store_pod(const T& t) {
    std::string buff;
    buff.assign(reinterpret_cast<const char*>(&t), sizeof(T));
    return buff;
  }
  template<typename T>
  typename std::enable_if<std::is_pod<T>::value, T>::type
  load_pod(const std::string& buff) {
    if (buff.size() != sizeof(T)) {
      throw std::runtime_error("Invalid-sized string " + std::to_string(buff.size()) +
                               " in database for POD of size " + std::to_string(sizeof(T)));
    }
    
    return *reinterpret_cast<const T*>(buff.data());
  }
}
