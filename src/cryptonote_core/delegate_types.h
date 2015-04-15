// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <cstdint>
#include <set>

namespace cryptonote
{
  typedef uint16_t delegate_id_t;
  typedef std::set<delegate_id_t> delegate_votes;
}
