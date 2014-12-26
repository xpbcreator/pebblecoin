// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "serialization.h"

#include <utility>

template <template <bool> class Archive, class K, class V>
bool do_serialize(Archive<false> &ar, std::pair<K, V> &p)
{
  return ::do_serialize(ar, p.first) && ::do_serialize(ar, p.second);
}

template <template <bool> class Archive, class K, class V>
bool do_serialize(Archive<true> &ar, std::pair<K, V> &p)
{
  return ::do_serialize(ar, p.first) && ::do_serialize(ar, p.second);
}
