// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <set>
#include <unordered_set>
#include <vector>

namespace {
  template <template <bool> class Archive, class Set>
  inline bool _do_serialize_impl_set(Archive<false> &ar, Set &s)
  {
    typedef typename Set::value_type V;
    
    std::vector<V> result;
    if (!::do_serialize(ar, result))
      return false;
    
    s.clear();
    s.insert(result.begin(), result.end());
    return true;
  }

  template <template <bool> class Archive, class Set>
  inline bool _do_serialize_impl_set(Archive<true> &ar, Set &s)
  {
    typedef typename Set::value_type V;
    
    std::vector<V> result(s.begin(), s.end());
    return ::do_serialize(ar, result);
  }
}

template <class Archive, class V, class Compare, class Alloc>
bool do_serialize(Archive &ar, std::set<V, Compare, Alloc> &s)
{
  return _do_serialize_impl_set(ar, s);
}

template <class Archive, class V, class Hash, class Pred, class Alloc>
bool do_serialize(Archive &ar, std::unordered_set<V, Hash, Pred, Alloc> &s)
{
  return _do_serialize_impl_set(ar, s);
}
