// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "serialization.h"

#include <map>
#include <unordered_map>
#include <vector>

namespace {
  template <template <bool> class Archive, class Map>
  inline bool _do_serialize_impl(Archive<false> &ar, Map &m)
  {
    typedef typename Map::key_type K;
    typedef typename Map::mapped_type V;
    
    std::vector<std::pair<K, V> > result;
    if (!::do_serialize(ar, result))
      return false;
    
    m.clear();
    m.insert(result.begin(), result.end());
    return true;
  }

  template <template <bool> class Archive, class Map>
  inline bool _do_serialize_impl(Archive<true> &ar, Map &m)
  {
    typedef typename Map::key_type K;
    typedef typename Map::mapped_type V;
    
    std::vector<std::pair<K, V> > result(m.begin(), m.end());
    return ::do_serialize(ar, result);
  }
}

template <class Archive, class K, class V, class Compare, class Alloc>
bool do_serialize(Archive &ar, std::map<K, V, Compare, Alloc> &m)
{
  return _do_serialize_impl(ar, m);
}

template <class Archive, class K, class V, class Hash, class Pred, class Alloc>
bool do_serialize(Archive &ar, std::unordered_map<K, V, Hash, Pred, Alloc> &m)
{
  return _do_serialize_impl(ar, m);
}
