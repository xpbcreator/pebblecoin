// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "serialization.h"

#include <utility>

template <class Archive, class K, class V>
bool do_serialize(Archive &ar, std::pair<K, V> &p)
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
