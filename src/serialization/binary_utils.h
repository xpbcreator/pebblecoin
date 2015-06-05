// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <sstream>

#include "include_base_utils.h"

#include "serialization.h"
#include "binary_archive.h"

namespace serialization {

template <class T>
bool _parse_binary(const std::string &blob, T &v)
{
  std::istringstream istr(blob);
  binary_archive<false> iar(istr);
  return ::serialization::serialize(iar, v);
}

template <class T>
bool parse_binary(const std::string &blob, T &v)
{
  if (_parse_binary(blob, v))
    return true;
  
  LOG_ERROR("Failed to parse binary, trying compatibility approach...");
  ::serialization::detail::compat_old_map_pair_serialize = true;
  bool r = _parse_binary(blob, v);
  ::serialization::detail::compat_old_map_pair_serialize = false;
  if (r) {
    LOG_PRINT_GREEN("Compatibility loading succeeded", LOG_LEVEL_0);
  }
  return r;
}

template<class T>
bool dump_binary(const T& v, std::string& blob)
{
  std::stringstream ostr;
  binary_archive<true> oar(ostr);
  bool success = ::serialization::serialize(oar, v);
  blob = ostr.str();
  return success && ostr.good();
};

} // namespace serialization
