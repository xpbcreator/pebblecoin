// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <set>
#include <vector>
#include <algorithm>
#include <numeric>

#pragma once

namespace tools
{
  
template <typename T>
std::set<T> random_subset(const std::set<T>& s, int subset_size)
{
  std::vector<size_t> indices(s.size());
  std::iota(indices.begin(), indices.end(), 0);
  std::random_shuffle(indices.begin(), indices.end());
  
  std::vector<T> tmp(s.begin(), s.end());
  
  std::set<T> result;
  
  for (int i=0; i < indices.size() && i < subset_size; i++)
  {
    result.insert(tmp[indices[i]]);
  }
  
  return result;
}

template <typename T>
std::set<T> set_and(const std::set<T>& a, const std::set<T>& b)
{
  std::set<T> result;
  std::set_intersection(a.begin(), a.end(),
                        b.begin(), b.end(),
                        std::inserter(result, result.end()));
  return result;
}

template <typename T>
std::set<T> set_or(const std::set<T>& a, const std::set<T>& b)
{
  std::set<T> result;
  std::set_union(a.begin(), a.end(),
                 b.begin(), b.end(),
                 std::inserter(result, result.end()));
  return result;
}
  
template <typename T>
std::set<T> set_sub(const std::set<T>& a, const std::set<T>& b)
{
  std::set<T> result;
  std::set_difference(a.begin(), a.end(),
                      b.begin(), b.end(),
                      std::inserter(result, result.end()));
  return result;
}
  
template <typename Container, typename Item>
bool contains(const Container& c, const Item& i)
{
  return std::find(c.begin(), c.end(), i) != c.end();
}

template <class Map>
typename Map::mapped_type const_get(const Map& m, const typename Map::key_type& k)
{
  auto it = m.find(k);
  if (it == m.end()) return typename Map::mapped_type();
  return it->second;
}

}
