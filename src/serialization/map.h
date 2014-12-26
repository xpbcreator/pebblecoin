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
    
    uint64_t size;
    if (!::do_serialize(ar, size))
      return false;
    
    m.clear();
    std::pair<K, V> item;
    for (uint64_t i=0; i < size; i++)
    {
      if (!::do_serialize(ar, item))
        return false;
      
      m.insert(item);
    }
    
    return true;
  }

  template <template <bool> class Archive, class Map>
  inline  bool _do_serialize_impl(Archive<true> &ar, Map &m)
  {
    typedef typename Map::key_type K;
	typedef typename Map::mapped_type V;
    
    uint64_t size = m.size();
    if (!::do_serialize(ar, size))
      return false;
    
    for (auto it = m.begin(); it != m.end(); ++it)
    {
      std::pair<K, V> item = *it;
      if (!::do_serialize(ar, item))
        return false;
    }
    
    return true;
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

