// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gtest/gtest.h"

#include "common/functional.h"
#include "cryptonote_core/visitors.h"

#include <string>
#include <vector>

struct inty {
  int i;
};

struct longy {
  long l;
};

struct stringy {
  std::string s;
};

typedef boost::variant<inty, longy, stringy> test_variant;

TEST(functional, visitor_called)
{
  struct whatever_visitor : boost::static_visitor<bool> {
    int num_calls;
    int num_strings;
    
    whatever_visitor() : num_calls(0), num_strings(0) { }
    
    bool operator()(const inty& i) {
      num_calls++;
      return true;
    }
    bool operator()(const longy& i) {
      num_calls++;
      return true;
    }
    bool operator()(const stringy& i) {
      num_calls++;
      num_strings++;
      return true;
    }
  };
  
  std::vector<test_variant> v;
  v.push_back(inty());
  v.push_back(longy());
  v.push_back(stringy());
  v.push_back(stringy());
  v.push_back(stringy());
  whatever_visitor visitor;
  ASSERT_TRUE(tools::all_apply_visitor(visitor, v));

  ASSERT_EQ(visitor.num_calls, v.size());
  ASSERT_EQ(visitor.num_strings, 3);
}

TEST(functional, visitor_right_index)
{
  struct whatever_visitor : boost::static_visitor<bool> {
    int index;
    
    int int_index, long_index, str_index;
    
    void set_index(int i) {
      index = i;
    }
    bool operator()(const inty& i) {
      int_index = index;
      return true;
    }
    bool operator()(const longy& i) {
      long_index = index;
      return true;
    }
    bool operator()(const stringy& i) {
      str_index = index;
      return true;
    }
  };
  
  std::vector<test_variant> v;
  v.push_back(stringy());
  v.push_back(longy());
  v.push_back(inty());
  whatever_visitor visitor;
  ASSERT_TRUE(tools::all_apply_visitor(visitor, v));
  ASSERT_EQ(visitor.str_index, 0);
  ASSERT_EQ(visitor.long_index, 1);
  ASSERT_EQ(visitor.int_index, 2);
}

TEST(functional, visitor_stops_on_false)
{
  struct whatever_visitor : boost::static_visitor<bool> {
    int num_calls;
    int num_strings;
    
    whatever_visitor() : num_calls(0), num_strings(0) { }
    
    bool operator()(const inty& i) {
      num_calls++;
      return true;
    }
    bool operator()(const longy& i) {
      num_calls++;
      return true;
    }
    bool operator()(const stringy& i) {
      num_calls++;
      return false;
    }
  };
  
  std::vector<test_variant> v;
  v.push_back(inty());
  v.push_back(inty());
  v.push_back(longy());
  v.push_back(longy());
  v.push_back(stringy());
  v.push_back(inty());
  v.push_back(inty());
  v.push_back(longy());
  v.push_back(longy());
  whatever_visitor visitor;
  ASSERT_FALSE(tools::all_apply_visitor(visitor, v));
  
  ASSERT_EQ(visitor.num_calls, 5);
}

TEST(functional, any_visitor_stops_on_true)
{
  struct whatever_visitor : boost::static_visitor<bool> {
    int num_calls;
    int num_strings;
    
    whatever_visitor() : num_calls(0), num_strings(0) { }
    
    bool operator()(const inty& i) {
      num_calls++;
      return false;
    }
    bool operator()(const longy& i) {
      num_calls++;
      return false;
    }
    bool operator()(const stringy& i) {
      num_calls++;
      return true;
    }
  };
  
  std::vector<test_variant> v;
  v.push_back(inty());
  v.push_back(inty());
  v.push_back(longy());
  v.push_back(longy());
  v.push_back(stringy());
  v.push_back(inty());
  v.push_back(inty());
  v.push_back(longy());
  v.push_back(longy());
  whatever_visitor visitor;
  ASSERT_TRUE(tools::any_apply_visitor(visitor, v));
  
  ASSERT_EQ(visitor.num_calls, 5);
}

TEST(functional, mapping_works)
{
  struct whatever_visitor : boost::static_visitor<bool> {
    int num_calls;
    int num_strings;
    
    whatever_visitor() : num_calls(0), num_strings(0) { }
    
    bool operator()(const inty& i) {
      num_calls++;
      return true;
    }
    bool operator()(const longy& i) {
      num_calls++;
      return true;
    }
    bool operator()(const stringy& i) {
      num_calls++;
      num_strings++;
      return true;
    }
  };
  
  struct hah {
    test_variant v;
    hah(const test_variant& v_in) : v(v_in) { }
  };
  
  std::vector<hah> v;
  v.push_back(hah(inty()));
  v.push_back(hah(longy()));
  v.push_back(hah(stringy()));
  v.push_back(hah(stringy()));
  v.push_back(hah(stringy()));
  {
    whatever_visitor visitor;
    ASSERT_TRUE(tools::all_apply_visitor(visitor, v, [](const hah& h) -> const test_variant& { return h.v; }));
  
    ASSERT_EQ(visitor.num_calls, v.size());
    ASSERT_EQ(visitor.num_strings, 3);
  }
  {
    struct something_else {
      test_variant t;
      something_else() : t(longy()) { }
      const test_variant& operator()(const hah& h) const { return t; }
    };
    auto h = something_else();
    
    whatever_visitor visitor;
    ASSERT_TRUE(tools::all_apply_visitor(visitor, v, h));
    
    ASSERT_EQ(visitor.num_calls, v.size());
    ASSERT_EQ(visitor.num_strings, 0);
  }
}

TEST(functional, reverse_works)
{
  struct whatever_visitor : boost::static_visitor<bool> {
    int n;
    
    whatever_visitor() : n(5) { }
    
    bool operator()(const inty& i) {
      n += 3;
      return true;
    }
    bool operator()(const longy& i) {
      n *= 4;
      return true;
    }
    bool operator()(const stringy& i) {
      n -= 2;
      return true;
    }
  };
  
  std::vector<test_variant> v;
  v.push_back(inty());
  v.push_back(longy());
  // forwards
  {
    whatever_visitor visitor;
    ASSERT_TRUE(tools::all_apply_visitor(visitor, v));
  
    ASSERT_EQ(visitor.n, (5 + 3) * 4);
  }
  // backwards
  {
    whatever_visitor visitor;
    ASSERT_TRUE(tools::all_apply_visitor(visitor, v, tools::identity(), true));
    
    ASSERT_EQ(visitor.n, (5 * 4) + 3);
  }
}
