// Copyright (c) 2015-2016 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//#define SQLITE3_MAP_DEBUG

#include "sqlite3/sqlite3_map_impl.h"
#include "sqlite3/sqlite3_map_ser.h"

#include "cryptonote_core/account_boost_serialization.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/cryptonote_basic_impl.h"
#include "cryptonote_core/cryptonote_boost_serialization.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/keypair.h"
#include "common/boost_serialization_helper.h"

#include "packing.h"

#include "gtest/gtest.h"

#include <boost/filesystem.hpp>


using namespace sqlite3;

void clear_file(const char *filename) {
  if (boost::filesystem::exists(filename)) {
    boost::filesystem::remove(filename);
  }
}

TEST(sqlite3, mem_store_load_strs)
{
  auto make_map = []() -> sqlite3_map<std::string, std::string> {
    return sqlite3_map<std::string, std::string>("strs.dat", load_string, store_string, load_string, store_string);
  };
  
  clear_file("strs.dat");
  
  {
    auto str_map = make_map();
    ASSERT_TRUE(str_map.empty());
    ASSERT_EQ(str_map.size(), 0);
    ASSERT_FALSE(str_map.contains("foo"));
    ASSERT_EQ(str_map.load("foo"), "");
    
    str_map.store("foo", "bar");

    ASSERT_FALSE(str_map.empty());
    ASSERT_EQ(str_map.size(), 1);
    ASSERT_EQ(str_map.load("foo"), "bar");
    ASSERT_TRUE(str_map.contains("foo"));
    ASSERT_EQ(str_map.count("foo"), 1);
    
    str_map.store("foo2", "bar2");
    
    ASSERT_FALSE(str_map.empty());
    ASSERT_EQ(str_map.size(), 2);
    ASSERT_EQ(str_map.load("foo"), "bar");
    ASSERT_EQ(str_map.load("foo2"), "bar2");
    ASSERT_TRUE(str_map.contains("foo"));
    ASSERT_TRUE(str_map.contains("foo2"));
    ASSERT_EQ(str_map.count("foo"), 1);
    ASSERT_EQ(str_map.count("foo2"), 1);
  }
  
  // should remember values
  {
    auto str_map = make_map();
    
    ASSERT_FALSE(str_map.empty());
    ASSERT_EQ(str_map.size(), 2);
    ASSERT_EQ(str_map.load("foo"), "bar");
    ASSERT_EQ(str_map.load("foo2"), "bar2");
    ASSERT_TRUE(str_map.contains("foo"));
    ASSERT_TRUE(str_map.contains("foo2"));
    ASSERT_EQ(str_map.count("foo"), 1);
    ASSERT_EQ(str_map.count("foo2"), 1);
    
    // can update value
    str_map.store("foo", "beelzebub");
    ASSERT_EQ(str_map.load("foo"), "beelzebub");
    
    ASSERT_EQ(str_map.size(), 2);
    ASSERT_EQ(str_map.load("foo"), "beelzebub");
    ASSERT_EQ(str_map.load("foo2"), "bar2");
    ASSERT_TRUE(str_map.contains("foo"));
    ASSERT_TRUE(str_map.contains("foo2"));
    ASSERT_EQ(str_map.count("foo"), 1);
    ASSERT_EQ(str_map.count("foo2"), 1);
  }
  {
    auto str_map = make_map();
    
    ASSERT_EQ(str_map.size(), 2);
    ASSERT_EQ(str_map.load("foo"), "beelzebub");
    ASSERT_EQ(str_map.load("foo2"), "bar2");
    ASSERT_TRUE(str_map.contains("foo"));
    ASSERT_TRUE(str_map.contains("foo2"));
    ASSERT_EQ(str_map.count("foo"), 1);
    ASSERT_EQ(str_map.count("foo2"), 1);
  }
}

TEST(sqlite3, erase_clear)
{
  auto make_map = []() {
    return sqlite3_map<std::string, std::string>("strs.dat", load_string, store_string, load_string, store_string);
  };
  
  clear_file("strs.dat");

  // map starts & ends empty, should be able to go through it all and have it work out
  for (int i=0; i < 5; i++) {
    {
      auto str_map = make_map();
      
      ASSERT_EQ(str_map.size(), 0);
      ASSERT_TRUE(str_map.empty());
      
      // can erase non-existent elements
      str_map.erase("whatever");
      
      ASSERT_EQ(str_map.size(), 0);
      ASSERT_TRUE(str_map.empty());

      str_map.store("foo", "bar");
      str_map.store("baz", "13");
      str_map.store("quux", "16");
      
      ASSERT_EQ(str_map.size(), 3);
      ASSERT_FALSE(str_map.empty());
      ASSERT_TRUE(str_map.contains("foo"));
      ASSERT_TRUE(str_map.contains("baz"));
      ASSERT_TRUE(str_map.contains("quux"));
      
      str_map.erase("quux");
      
      ASSERT_EQ(str_map.size(), 2);
      ASSERT_FALSE(str_map.empty());
      ASSERT_TRUE(str_map.contains("foo"));
      ASSERT_TRUE(str_map.contains("baz"));
      ASSERT_FALSE(str_map.contains("quux"));
    }
    
    {
      auto str_map = make_map();
      
      ASSERT_EQ(str_map.size(), 2);
      ASSERT_FALSE(str_map.empty());
      
      str_map.erase("abc");
    
      ASSERT_EQ(str_map.size(), 2);
      ASSERT_FALSE(str_map.empty());
      ASSERT_TRUE(str_map.contains("foo"));
    
      str_map.erase("foo");
      
      ASSERT_EQ(str_map.size(), 1);
      ASSERT_FALSE(str_map.empty());
      ASSERT_FALSE(str_map.contains("foo"));
    }
    
    {
      auto str_map = make_map();
      
      str_map.store("a", "1");
      str_map.store("b", "2");
      str_map.store("c", "3");
      str_map.store("d", "4");
      str_map.store("e", "5");
      str_map.store("f", "6");
      str_map.store("g", "7");
    
      ASSERT_EQ(str_map.size(), 8);
      ASSERT_FALSE(str_map.empty());
    }
    
    {
      auto str_map = make_map();
      
      ASSERT_EQ(str_map.size(), 8);
      ASSERT_FALSE(str_map.empty());
      
      str_map.clear();
      
      ASSERT_EQ(str_map.size(), 0);
      ASSERT_TRUE(str_map.empty());
      ASSERT_FALSE(str_map.contains("foo"));
      ASSERT_FALSE(str_map.contains("c"));
    }
    
    {
      auto str_map = make_map();
      ASSERT_EQ(str_map.size(), 0);
      ASSERT_TRUE(str_map.empty());
    }
  }
}

TEST(sqlite3, pod_serialization)
{
  {
    clear_file("foo.dat");
    {
      sqlite3_map<int, double> map("foo.dat", load_pod<int>, store_pod<int>, load_pod<double>, store_pod<double>);
      map.store(5, 13.5);
      map.store(10, 12);
      map.store(19, 11.6);
    }
    {
      sqlite3_map<int, double> map("foo.dat", load_pod<int>, store_pod<int>, load_pod<double>, store_pod<double>);
      ASSERT_EQ(map.size(), 3);
      
      ASSERT_EQ(map.load(5), 13.5);
      ASSERT_EQ(map.load(10), 12);
      ASSERT_EQ(map.load(19), 11.6);
      
      ASSERT_TRUE(map.contains(5));
      ASSERT_TRUE(map.contains(10));
      ASSERT_TRUE(map.contains(19));
      
      ASSERT_EQ(map.count(5), 1);
      ASSERT_EQ(map.count(10), 1);
      ASSERT_EQ(map.count(19), 1);
    }
  }
  
  {
    PACK(struct stuff {
      uint8_t a;
      uint32_t b;
    });
    
    static_assert(sizeof(stuff) == 5, "packed struct should have size 5");
    
    clear_file("foo.dat");
    {
      sqlite3_map<int, stuff> map("foo.dat", load_pod<int>, store_pod<int>, load_pod<stuff>, store_pod<stuff>);
      map.store(100, stuff{20, 80});
      map.store(55, stuff{44, 11});
    }
    {
      sqlite3_map<int, stuff> map("foo.dat", load_pod<int>, store_pod<int>, load_pod<stuff>, store_pod<stuff>);
      ASSERT_EQ(map.size(), 2);
      ASSERT_EQ(map.load(100).a, 20);
      ASSERT_EQ(map.load(100).b, 80);
      ASSERT_EQ(map.load(55).a, 44);
      ASSERT_EQ(map.load(55).b, 11);
    }
  }
}

TEST(sqlite3, boost_serialization)
{
  using namespace cryptonote;
  
  // build the tx
  transaction tx;
  {
    tx.set_null();
    auto keypair = keypair::generate();
    tx.version = VANILLA_TRANSACTION_VERSION;
    tx.add_out(tx_out(999, txout_to_key(keypair.pub)), CP_XPB);
    tx.add_in(txin_gen(), CP_XPB);
    tx.add_in(txin_to_key(), CP_XPB);
  }
  auto txhash = get_transaction_hash(tx);
  
  clear_file("txs.dat");
  
  auto make_map = []() {
    return sqlite3_map<crypto::hash, transaction>("txs.dat",
                                                  load_pod<crypto::hash>, store_pod<crypto::hash>,
                                                  tools::boost_unserialize_from_string<transaction>,
                                                  tools::boost_serialize_to_string<transaction>);
  };
  
  {
    auto txmap = make_map();
    
    txmap.store(txhash, tx);
    
    ASSERT_EQ(get_transaction_hash(txmap.load(txhash)), txhash);
  }
  
  {
    auto txmap = make_map();
    ASSERT_EQ(get_transaction_hash(txmap.load(txhash)), txhash);
  }
}

TEST(sqlite3, iterator)
{
  auto make_map = [](const char *fn="strs.dat") {
    return sqlite3_map<std::string, std::string>(fn, load_string, store_string, load_string, store_string);
  };
  
  clear_file("strs.dat");
  
  // both for in-mem and on-disk dbs
  for (int i=0; i < 2; i++)
  {
    auto str_map = make_map(i == 0 ? "strs.dat" : nullptr);
    
    ASSERT_EQ(str_map.begin(), str_map.begin());
    ASSERT_EQ(str_map.end(), str_map.end());
    // begin == end since both are invalid
    ASSERT_EQ(str_map.begin(), str_map.end());
    
    str_map.store("foo", "bar");
    
    ASSERT_EQ(str_map.begin(), str_map.begin());
    ASSERT_EQ(str_map.end(), str_map.end());
    ASSERT_NE(str_map.begin(), str_map.end());
    
    {
      auto it = str_map.begin();
      ASSERT_EQ(it->first, "foo");
      ASSERT_EQ(it->second, "bar");
      ASSERT_EQ(*it, decltype(str_map)::value_type("foo", "bar"));
      
      auto it_copy = it;
      ASSERT_EQ(it_copy, it);
      ASSERT_EQ(it_copy->first, "foo");
      ASSERT_EQ(it_copy->second, "bar");
      ASSERT_EQ(*it_copy, decltype(str_map)::value_type("foo", "bar"));
      
      ++it;
      ASSERT_EQ(it, str_map.end());
      
      ASSERT_NE(it_copy, it);
      ASSERT_EQ(it_copy->first, "foo");
      ASSERT_EQ(it_copy->second, "bar");
      ASSERT_EQ(*it_copy, decltype(str_map)::value_type("foo", "bar"));
      
      auto it_move = std::move(it_copy);
      ASSERT_NE(it_move, it);
      ASSERT_EQ(it_move->first, "foo");
      ASSERT_EQ(it_move->second, "bar");
      ASSERT_EQ(*it_move, decltype(str_map)::value_type("foo", "bar"));
    }
    
    {
      // copying invalid iterator still results in invalid iterator
      auto it = str_map.end();
      auto it_copy = it;
      ASSERT_EQ(it, str_map.end());
      ASSERT_EQ(it_copy, str_map.end());
    }
  }
}

TEST(sqlite3, looping)
{
  auto make_map = []() {
    return sqlite3_map<std::string, std::string>("strs.dat", load_string, store_string, load_string, store_string);
  };
  
  clear_file("strs.dat");
  
  {
    auto str_map = make_map();
    
    str_map.store("zarg", "zzhhh");
    str_map.store("bar", "baz");
    str_map.store("foo", "bar");
  }
  {
    auto str_map = make_map();
    
    std::set<std::string> found;
    
    std::vector<std::string> keys;
    std::vector<std::string> values;
    
    for (auto& item : str_map) {
      found.insert(item.first);
    }
    
    ASSERT_EQ(found.size(), 3);
    ASSERT_TRUE(found.count("zarg") > 0);
    ASSERT_TRUE(found.count("bar") > 0);
    ASSERT_TRUE(found.count("foo") > 0);
  }
}

TEST(sqlite3, find)
{
  auto make_map = []() {
    return sqlite3_map<std::string, std::string>("strs.dat", load_string, store_string, load_string, store_string);
  };
  
  clear_file("strs.dat");
  
  {
    auto str_map = make_map();
    
    str_map.store("foo", "bar");
    str_map.store("bar", "baz");
 
    {
      auto it_foo = str_map.find("foo");
      auto it_bar = str_map.find("bar");
      auto it_none = str_map.find("gorm");
      
      ASSERT_EQ(it_foo, it_foo);
      ASSERT_EQ(it_bar, it_bar);
      ASSERT_EQ(it_none, it_none);
      
      ASSERT_NE(it_foo, str_map.end());
      ASSERT_NE(it_bar, str_map.end());
      ASSERT_EQ(it_none, str_map.end());
      
      ASSERT_EQ(*it_foo, decltype(str_map)::value_type("foo", "bar"));
      ASSERT_EQ(*it_bar, decltype(str_map)::value_type("bar", "baz"));
    }
  }
}

TEST(sqlite3, insert)
{
  auto make_map = []() {
    return sqlite3_map<std::string, std::string>("strs.dat", load_string, store_string, load_string, store_string);
  };
  
  clear_file("strs.dat");
  
  {
    auto str_map = make_map();

    {
      // insert
      auto r = str_map.insert(std::make_pair("foo", "bar"));
      ASSERT_TRUE(r.second);
      ASSERT_EQ(r.first, str_map.find("foo"));
      ASSERT_EQ(r.first->second, "bar");
    }
    
    {
      // no overwrite
      auto r = str_map.insert(std::make_pair("foo", "NOT_INSERT"));
      ASSERT_FALSE(r.second);
      ASSERT_EQ(r.first, str_map.find("foo"));
      ASSERT_EQ(r.first->second, "bar");
    }
  }
  
  {
    auto str_map = make_map();
    
    ASSERT_EQ(str_map.load("foo"), "bar");
  }
}

TEST(sqlite3, erase_iterator)
{
  auto make_map = []() {
    return sqlite3_map<std::string, std::string>("strs.dat", load_string, store_string, load_string, store_string);
  };
  
  clear_file("strs.dat");
  
  {
    auto str_map = make_map();
    
    str_map.store("1", "zzza");
    str_map.store("2", "zzzb");
    str_map.store("3", "zzzc");
    str_map.store("4", "zzzd");
    str_map.store("5", "zzze");
    
    ASSERT_EQ(str_map.size(), 5);
    
    str_map.erase(str_map.find("3"));
    
    ASSERT_EQ(str_map.size(), 4);
    ASSERT_EQ(str_map.find("3"), str_map.end());
    ASSERT_EQ(str_map.load("3"), "");
  }
  
  {
    auto str_map = make_map();
    str_map.erase(str_map.find("5"));
  }
  
  {
    auto str_map = make_map();
    ASSERT_EQ(str_map.size(), 3);
    ASSERT_EQ(str_map.load("1"), "zzza");
    ASSERT_EQ(str_map.load("2"), "zzzb");
    ASSERT_EQ(str_map.load("3"), "");
    ASSERT_EQ(str_map.load("4"), "zzzd");
    ASSERT_EQ(str_map.load("5"), "");
  }
}

TEST(sqlite3, in_mem_db)
{
  auto make_map = []() {
    return sqlite3_map<std::string, std::string>(nullptr, load_string, store_string, load_string, store_string);
  };
  
  {
    auto str_map = make_map();
    
    str_map.store("1", "zzza");
    str_map.store("2", "zzzb");
    str_map.store("3", "zzzc");
    str_map.store("4", "zzzd");
    str_map.store("5", "zzze");
    
    ASSERT_EQ(str_map.size(), 5);
    
    ASSERT_EQ(str_map.load("1"), "zzza");
    ASSERT_EQ(str_map.load("2"), "zzzb");
    ASSERT_EQ(str_map.load("3"), "zzzc");
    ASSERT_EQ(str_map.load("4"), "zzzd");
    ASSERT_EQ(str_map.load("5"), "zzze");
    
    ASSERT_EQ(str_map.count("1"), 1);
    ASSERT_EQ(str_map.count("2"), 1);
    ASSERT_EQ(str_map.count("3"), 1);
    ASSERT_EQ(str_map.count("4"), 1);
    ASSERT_EQ(str_map.count("5"), 1);

    ASSERT_EQ(str_map.contains("1"), true);
    ASSERT_EQ(str_map.contains("2"), true);
    ASSERT_EQ(str_map.contains("3"), true);
    ASSERT_EQ(str_map.contains("4"), true);
    ASSERT_EQ(str_map.contains("5"), true);
    
    ASSERT_NE(str_map.find("1"), str_map.end());
    ASSERT_NE(str_map.find("2"), str_map.end());
    ASSERT_NE(str_map.find("3"), str_map.end());
    ASSERT_NE(str_map.find("4"), str_map.end());
    ASSERT_NE(str_map.find("5"), str_map.end());
  }
  
  {
    auto str_map = make_map();
    ASSERT_EQ(str_map.size(), 0);
  }
}

TEST(sqlite3, in_mem_block_db)
{
  using namespace cryptonote;
  
  auto make_map = []() {
    return sqlite3_map<crypto::hash, block>(nullptr, load_pod<crypto::hash>, store_pod<crypto::hash>,
                                            tools::boost_unserialize_from_string<block>,
                                            tools::boost_serialize_to_string<block>);
  };
  
  {
    auto m1 = make_map();
    auto m2 = make_map();
    auto m3 = make_map();
    
    block b = boost::value_initialized<block>();
    b.nonce = 10;
    auto h = get_block_hash(b);
    
    m1.store(h, b);
    
    ASSERT_EQ(m1.size(), 1);
    ASSERT_EQ(get_block_hash(m1.load(h)), h);
    ASSERT_EQ(m1.count(h), 1);
    ASSERT_EQ(m1.contains(h), true);
    ASSERT_NE(m1.find(h), m1.end());
    
    ASSERT_EQ(m2.size(), 0);
    ASSERT_EQ(m3.size(), 0);
  }
}

TEST(sqlite3, reopen)
{
  auto make_map = [](const char *fn) {
    return sqlite3_map<std::string, std::string>(fn, load_string, store_string, load_string, store_string);
  };
  
  clear_file("strs1.dat");
  clear_file("strs2.dat");
  clear_file("strs3.dat");
  
  {
    auto map = make_map("strs1.dat");
    map.store("1", "one");
    map.store("2", "two");
    map.store("3", "three");
  }
  {
    auto map = make_map("strs2.dat");
    map.store("1", "eins");
    map.store("2", "zwei");
    map.store("4", "drei");
  }
  {
    auto map = make_map("strs3.dat");
    map.store("1", "un");
    map.store("2", "deux");
    map.store("5", "trois");
  }
  
  {
    auto map = make_map("strs1.dat");
    ASSERT_EQ(map.load("1"), "one");
    ASSERT_EQ(map.load("2"), "two");
    ASSERT_EQ(map.load("3"), "three");
    
    ASSERT_FALSE(map.contains("4"));
    ASSERT_FALSE(map.contains("5"));
    ASSERT_EQ(map.load("4"), "");
    ASSERT_EQ(map.load("5"), "");
    
    map.reopen("strs2.dat");
    ASSERT_EQ(map.load("1"), "eins");
    ASSERT_EQ(map.load("2"), "zwei");
    ASSERT_EQ(map.load("4"), "drei");

    ASSERT_FALSE(map.contains("3"));
    ASSERT_FALSE(map.contains("5"));
    ASSERT_EQ(map.load("3"), "");
    ASSERT_EQ(map.load("5"), "");
    
    map.reopen("strs3.dat");
    ASSERT_EQ(map.load("1"), "un");
    ASSERT_EQ(map.load("2"), "deux");
    ASSERT_EQ(map.load("5"), "trois");
    
    ASSERT_FALSE(map.contains("3"));
    ASSERT_FALSE(map.contains("4"));
    ASSERT_EQ(map.load("3"), "");
    ASSERT_EQ(map.load("4"), "");
  }
  
  // ensure can change map without errors
  {
    auto map = make_map(nullptr);
    map.store("1", "one");
    
    {
      map.reopen(nullptr);
      ASSERT_FALSE(map.contains("1"));
      ASSERT_EQ(map.load("1"), "");
      
      map.store("1", "newww");
    }
    
    {
      clear_file("zomga.dat");
      map.reopen("zomga.dat");
      
      ASSERT_FALSE(map.contains("1"));
      ASSERT_EQ(map.load("1"), "");
    }
  }
}

TEST(sqlite3, autocommit)
{
  auto make_map = [](const char *fn="strs.dat") {
    return sqlite3_map<std::string, std::string>(fn, load_string, store_string, load_string, store_string);
  };
  
  clear_file("strs.dat");
  
  {
    auto str_map = make_map();
    
    str_map.set_autocommit(true, true);
    
    str_map.store("1", "one"); // autocommitted
    
    str_map.set_autocommit(false, false);
    
    str_map.store("2", "two"); // shouldn't be stored
    ASSERT_TRUE(str_map.contains("2"));
    ASSERT_EQ(str_map.load("2"), "two");
  }
  
  {
    auto str_map = make_map();
    
    ASSERT_TRUE(str_map.contains("1"));
    ASSERT_FALSE(str_map.contains("2"));
    
    ASSERT_EQ(str_map.load("1"), "one");
    ASSERT_EQ(str_map.load("2"), "");
    
    str_map.set_autocommit(false, false);
    str_map.store("3", "three");
    str_map.store("4", "four");
    str_map.commit();
    str_map.store("5", "five");
    ASSERT_TRUE(str_map.contains("5"));
    ASSERT_EQ(str_map.load("5"), "five");
    
    // reopen should clear it too
    str_map.reopen("strs.dat");
    ASSERT_FALSE(str_map.contains("2"));
    ASSERT_FALSE(str_map.contains("5"));
    
    ASSERT_EQ(str_map.load("1"), "one");
    ASSERT_EQ(str_map.load("2"), "");
    ASSERT_EQ(str_map.load("3"), "three");
    ASSERT_EQ(str_map.load("4"), "four");
    ASSERT_EQ(str_map.load("5"), "");
  }
  
  {
    auto str_map = make_map();
    
    ASSERT_FALSE(str_map.contains("2"));
    ASSERT_FALSE(str_map.contains("5"));
    
    ASSERT_EQ(str_map.load("1"), "one");
    ASSERT_EQ(str_map.load("2"), "");
    ASSERT_EQ(str_map.load("3"), "three");
    ASSERT_EQ(str_map.load("4"), "four");
    ASSERT_EQ(str_map.load("5"), "");
  }
}

TEST(sqlite3, stresstest)
{
  auto make_map = [](const char *fn="strs.dat") {
    return sqlite3_map<std::string, std::string>(fn, load_string, store_string, load_string, store_string);
  };
  
  // both for in-mem and on-disk dbs
  for (int i=0; i < 2; i++)
  {
    for (int autocommit=0; autocommit < 2; autocommit++) {
      clear_file("strs.dat");
      {
        auto str_map = make_map(i == 0 ? "strs.dat" : nullptr);
        
        std::cout << "Testing on map " << (i == 0 ? "strs.dat" : "(in-memory)") << ", autocommit=" << autocommit << std::endl;
        
        if (autocommit) {
          str_map.set_autocommit(true, true);
        } else {
          str_map.set_autocommit(false, false);
        }
        
        std::cout << "storing 0 to 499..." << std::endl;
        for (int a=0; a < 500; a++) {
          str_map.store(std::to_string(a), std::string((a+50)*5, '.'));
        }
        
        std::cout << str_map.size() << std::endl;
        ASSERT_EQ(str_map.size(), 500) << " size was not 500";
        // make sure .end() really is invalid...
        auto it_end = str_map.end();
        for (int a=0; a < 500; a++) {
          ASSERT_EQ(str_map.load(std::to_string(a)), std::string((a+50)*5, '.')) << " load() failed on " << std::to_string(a);
          ASSERT_TRUE(str_map.contains(std::to_string(a))) << " did not contain " << std::to_string(a);
          ASSERT_EQ(str_map.count(std::to_string(a)), 1) << " did not count()=1 of " << std::to_string(a);
          ASSERT_NE(str_map.find(std::to_string(a)), str_map.end()) << " did not find() " << std::to_string(a);
          ASSERT_NE(str_map.find(std::to_string(a)), it_end) << " did not find() " << std::to_string(a);
          ASSERT_EQ(it_end, str_map.end()) << " did not find() " << std::to_string(a);
          ASSERT_EQ(*str_map.find(std::to_string(a)), std::make_pair(std::to_string(a), std::string((a+50)*5, '.'))) <<
              "*find() did not match for " << std::to_string(a);
        }
      }
      {
        auto str_map = make_map(i == 0 ? "strs.dat" : nullptr);
        str_map.clear();
        ASSERT_EQ(str_map.size(), 0) << " size was not 0 after clearing";
        auto it_end = str_map.end();
        for (int a=0; a < 500; a++) {
          ASSERT_EQ(str_map.load(std::to_string(a)), "") << " load() wasn't empty on " << std::to_string(a);
          ASSERT_TRUE(!str_map.contains(std::to_string(a))) << " contained " << std::to_string(a);
          ASSERT_EQ(str_map.count(std::to_string(a)), 0) << " not count()=0 of " << std::to_string(a);
          ASSERT_EQ(str_map.find(std::to_string(a)), str_map.end()) << " did find() " << std::to_string(a);
          ASSERT_EQ(str_map.find(std::to_string(a)), it_end) << " did find() " << std::to_string(a);
          ASSERT_EQ(it_end, str_map.end()) << " it_end was not end " << std::to_string(a);
        }
      }
    }
  }
}

TEST(sqlite3, speedtest)
{
  struct somestuff {
    char hah[100];
  };
  
  auto make_map = [](const char *fn="strs.dat") {
    return sqlite3_map<int, somestuff>(fn, load_pod<int>, store_pod<int>, load_pod<somestuff>, store_pod<somestuff>);
  };
  
  clear_file("strs.dat");
  {
    auto str_map = make_map("strs.dat");
    str_map.set_autocommit(false, true);
    
    LOG_PRINT_L0("Storing 100,000 items...");
    for (int a=0; a < 100000; a++) {
      somestuff f;
      memset(f.hah, a % 256, sizeof(f.hah));
      str_map.store(a, f);
    }
  }
  
  {
    auto str_map = make_map("strs.dat");
    
    LOG_PRINT_L0("Contains? each of them and more");
    for (int a=-50000; a < 150000; a++) {
      if (a >= 0 && a < 100000) {
        ASSERT_TRUE(str_map.contains(a));
      } else {
        ASSERT_FALSE(str_map.contains(a));
      }
    }
  }
}
