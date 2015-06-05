// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <utility>

template <class Archive, class K, class V>
bool _do_serialize_pair_impl_old(Archive &ar, std::pair<K, V> &p)
{
  return ::do_serialize(ar, p.first) && ::do_serialize(ar, p.second);
}

template <class Archive, class K, class V>
bool _do_serialize_pair_impl_new(Archive &ar, std::pair<K, V> &p)
{
  size_t s = 2;
  ar.begin_array(s);
  if (s != 2)
    return false;
  
  if (!::do_serialize(ar, p.first))
    return false;
  
  ar.delimit_array();
  
  if (!::do_serialize(ar, p.second))
    return false;
  
  ar.end_array();
  
  return true;
}

template <class Archive, class K, class V>
bool do_serialize(Archive &ar, std::pair<K, V> &p)
{
  if (::serialization::detail::compat_old_map_pair_serialize)
  {
    return _do_serialize_pair_impl_old(ar, p);
  }
  else
  {
    return _do_serialize_pair_impl_new(ar, p);
  }
}
