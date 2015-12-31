// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

namespace boost
{
  namespace serialization
  {
    //BOOST_CLASS_VERSION(odetool::net_adress, 1)
    template <class Archive, class ver_type>
    inline void serialize(Archive &a,  nodetool::net_address& na, const ver_type ver)
    {
      // work around packed struct
      auto ip = na.ip;
      auto port = na.port;
      
      a & ip;
      a & port;
      
      na.ip = ip;
      na.port = port;
    }


    template <class Archive, class ver_type>
    inline void serialize(Archive &a,  nodetool::peerlist_entry& pl, const ver_type ver)
    {
      // work around packed struct
      auto adr = pl.adr;
      auto id = pl.id;
      auto last_seen = pl.last_seen;
      
      a & adr;
      a & id;
      a & last_seen;
      
      pl.adr = adr;
      pl.id = id;
      pl.last_seen = last_seen;
    }    
  }
}
