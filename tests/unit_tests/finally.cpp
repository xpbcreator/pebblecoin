// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <string>
#include <memory>

#include "gtest/gtest.h"

#include "common/finally.h"

using namespace tools;

static int g_value = 100;

TEST(finally, it_runs)
{
  bool finally_ran = false;
  {
    finally f([&]() { finally_ran = true; });
  }
  ASSERT_TRUE(finally_ran);
}

TEST(scoped_set_var, local_int)
{
  int foo = 300;
  
  {
    auto ss = scoped_set_var(foo, 400);
    ASSERT_EQ(foo, 400);
  }
  ASSERT_EQ(foo, 300);
}

TEST(scoped_set_var, nested_scopes)
{
  int foo = 200;
  
  {
    auto ss = scoped_set_var(foo, 250);
    {
      auto s2 = scoped_set_var(foo, 150);
      ASSERT_EQ(foo, 150);
    }
    ASSERT_EQ(foo, 250);
  }
  ASSERT_EQ(foo, 200);
}

TEST(scoped_set_var, global)
{
  {
    auto ss = scoped_set_var(g_value, 300);
    ASSERT_EQ(g_value, 300);
  }
  ASSERT_EQ(g_value, 100);
}

TEST(scoped_set_var, strs)
{
  std::string hark = "Hello";
  {
    auto ss = scoped_set_var(hark, "Nublar");
    ASSERT_EQ(hark, "Nublar");
  }
  ASSERT_EQ(hark, "Hello");
}
