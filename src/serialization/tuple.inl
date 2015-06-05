// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <utility>

/*
namespace tuple_serialization_detail {
  template<unsigned int N>
  struct Serialize
  {
    template<class Archive, typename... Args>
    static bool do_serialize(Archive& ar, std::tuple<Args...>& t)
    {
      if (!::do_serialize(ar, std::get<N-1>(t)))
        return false;
      return Serialize<N-1>::do_serialize(ar, t);
    }
  };
  template<>
  struct Serialize<0>
  {
    template<class Archive, typename... Args>
    static bool do_serialize(Archive& ar, std::tuple<Args...>& t)
    {
      return true;
    }
  };
}

template<class Archive, typename... Args>
bool do_serialize(Archive& ar, std::tuple<Args...>& t)
{
  return tuple_serialization_detail::Serialize<sizeof...(Args)>::do_serialize(ar, t);
}
*/
