// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <set>
#include <vector>
#include <algorithm>
#include <numeric>

#include <boost/utility/value_init.hpp>

#include "common/functional.h"

#pragma once

// ---------------------------------------
// helpers
// ---------------------------------------
#define CONCATENATE_DETAIL(x, y) x##y
#define CONCATENATE(x, y) CONCATENATE_DETAIL(x, y)
#define MAKE_UNIQUE_LINE(x) CONCATENATE(x, __LINE__)

template<typename T>
inline void suppress_warning(T&& val)
{
}

namespace tools
{
  
template <typename T>
std::set<T> random_subset(const std::set<T>& s, int subset_size)
{
  std::set<T> result;
  
  if (subset_size <= 0)
    return result;
  
  std::vector<size_t> indices(s.size());
  std::iota(indices.begin(), indices.end(), 0);
  std::random_shuffle(indices.begin(), indices.end());
  
  std::vector<T> tmp(s.begin(), s.end());
  
  
  for (size_t i=0; i < indices.size() && i < (size_t)subset_size; i++)
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
const typename Map::mapped_type& const_get(const Map& m, const typename Map::key_type& k)
{
  static typename Map::mapped_type _default = boost::value_initialized<decltype(_default)>();
  
  auto it = m.find(k);
  if (it == m.end()) return _default;
  return it->second;
}

template <class Map>
std::vector<typename Map::key_type> map_keys(const Map& m)
{
  std::vector<typename Map::key_type> res;
  for (const auto& p : m)
    res.push_back(p.first);
  return res;
}

template <class Map>
std::vector<typename Map::mapped_type> map_values(const Map& m)
{
  std::vector<typename Map::mapped_type> res;
  for (const auto& p : m)
    res.push_back(p.second);
  return res;
}
  
// ---------------------------------------
// unpacking
// ---------------------------------------
#define UNPACK_PAIR(VAR1, VAR2, PAIR) \
    auto&& MAKE_UNIQUE_LINE(_unpack_pair) = PAIR; \
    auto& VAR1 = MAKE_UNIQUE_LINE(_unpack_pair).first; suppress_warning(VAR1); \
    auto& VAR2 = MAKE_UNIQUE_LINE(_unpack_pair).second; suppress_warning(VAR2);
  
// ---------------------------------------
// iteration
// ---------------------------------------
#define FOR_EACH_PAIR(VAR1, VAR2, CONTAINER)  \
    for (auto& MAKE_UNIQUE_LINE(_for_each_pair) : CONTAINER) { \
        UNPACK_PAIR(VAR1, VAR2, MAKE_UNIQUE_LINE(_for_each_pair));

#define END_FOR_EACH_PAIR()   }

#define FOR(VAR, CONTAINER) for (auto& VAR : CONTAINER)

#define FOR_ENUM(INDEX_VAR, VAR, CONTAINER)  \
    {  \
        size_t MAKE_UNIQUE_LINE(_for_enum_index) = 0; \
        for (auto& VAR : CONTAINER) { \
            const size_t INDEX_VAR = MAKE_UNIQUE_LINE(_for_enum_index); suppress_warning(INDEX_VAR); \
            MAKE_UNIQUE_LINE(_for_enum_index)++;
  
#define END_FOR_ENUM()  \
        } \
    }
  
#define FOR_ENUM_PAIR(INDEX_VAR, VAR1, VAR2, CONTAINER)  \
    FOR_ENUM(INDEX_VAR, MAKE_UNIQUE_LINE(_for_enum_pair), CONTAINER) { \
        UNPACK_PAIR(VAR1, VAR2, MAKE_UNIQUE_LINE(_for_enum_pair));

#define END_FOR_ENUM_PAIR() \
    } \
    END_FOR_ENUM()

}
