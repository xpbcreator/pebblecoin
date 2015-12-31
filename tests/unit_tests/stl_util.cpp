// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(_MSC_VER) && _MSC_VER < 1900

// no initializer list support

#else

#include <list>

#include "gtest/gtest.h"

#include "common/stl-util.h"

using namespace tools;

#define S std::set<int>

const S A = S({1, 2, 3, 4, 5});
const S no_A = S({6, 7, 8, 9, 10});
const S some_A = S({3, 4, 5, 6, 7});
const S in_A = S({2, 3, 4});
const S has_A = S({1, 2, 3, 4, 5, 6, 7, 8});
const S is_A = S({1, 2, 3, 4, 5});
const S empty = S();

TEST(stl_util, set_or)
{
  const auto& f = set_or<int>;
  
  ASSERT_EQ(f(A, no_A),    S({1, 2, 3, 4, 5, 6, 7, 8, 9, 10}));
  ASSERT_EQ(f(A, some_A),  S({1, 2, 3, 4, 5, 6, 7}));
  ASSERT_EQ(f(A, in_A),    A);
  ASSERT_EQ(f(A, has_A),   S({1, 2, 3, 4, 5, 6, 7, 8}));
  ASSERT_EQ(f(A, is_A),    A);
  ASSERT_EQ(f(A, empty),   A);
  ASSERT_EQ(f(no_A, A),    S({1, 2, 3, 4, 5, 6, 7, 8, 9, 10}));
  ASSERT_EQ(f(some_A, A),  S({1, 2, 3, 4, 5, 6, 7}));
  ASSERT_EQ(f(in_A, A),    A);
  ASSERT_EQ(f(has_A, A),   S({1, 2, 3, 4, 5, 6, 7, 8}));
  ASSERT_EQ(f(is_A, A),    A);
  ASSERT_EQ(f(empty, A),   A);
}

TEST(stl_util, set_and)
{
  const auto& f = set_and<int>;
  
  ASSERT_EQ(f(A, no_A),    empty);
  ASSERT_EQ(f(A, some_A),  S({3, 4, 5}));
  ASSERT_EQ(f(A, in_A),    in_A);
  ASSERT_EQ(f(A, has_A),   A);
  ASSERT_EQ(f(A, is_A),    A);
  ASSERT_EQ(f(A, empty),   empty);
  ASSERT_EQ(f(no_A, A),    empty);
  ASSERT_EQ(f(some_A, A),  S({3, 4, 5}));
  ASSERT_EQ(f(in_A, A),    in_A);
  ASSERT_EQ(f(has_A, A),   A);
  ASSERT_EQ(f(is_A, A),    A);
  ASSERT_EQ(f(empty, A),   empty);
}

TEST(stl_util, set_sub)
{
  const auto& f = set_sub<int>;
  
  ASSERT_EQ(f(A, no_A),    A);
  ASSERT_EQ(f(A, some_A),  S({1, 2}));
  ASSERT_EQ(f(A, in_A),    S({1, 5}));
  ASSERT_EQ(f(A, has_A),   empty);
  ASSERT_EQ(f(A, is_A),    empty);
  ASSERT_EQ(f(A, empty),   A);
  ASSERT_EQ(f(no_A, A),    no_A);
  ASSERT_EQ(f(some_A, A),  S({6, 7}));
  ASSERT_EQ(f(in_A, A),    empty);
  ASSERT_EQ(f(has_A, A),   S({6, 7, 8}));
  ASSERT_EQ(f(is_A, A),    empty);
  ASSERT_EQ(f(empty, A),   empty);
}

TEST(stl_util, contains)
{
  const auto& f = contains<std::set<int>, int>;
  
  ASSERT_TRUE(f(A, 1));
  ASSERT_TRUE(f(A, 2));
  ASSERT_TRUE(f(A, 3));
  ASSERT_TRUE(f(A, 4));
  ASSERT_TRUE(f(A, 5));
  ASSERT_FALSE(f(A, 6));
  ASSERT_FALSE(f(A, 7));
  
  ASSERT_FALSE(f(empty, 6));
  ASSERT_FALSE(f(empty, 7));
  
  std::list<int> hah({1, 2, 3, 4, 5});
  
  ASSERT_TRUE(contains(hah, 1));
  ASSERT_TRUE(contains(hah, 2));
  ASSERT_TRUE(contains(hah, 3));
  ASSERT_TRUE(contains(hah, 4));
  ASSERT_TRUE(contains(hah, 5));
  ASSERT_FALSE(contains(hah, 6));
  ASSERT_FALSE(contains(hah, 7));
}

TEST(stl_util, random_subset)
{
  ASSERT_EQ(random_subset(A, 10), A);
  ASSERT_EQ(random_subset(A, 10), A);
  ASSERT_EQ(random_subset(A, 10), A);
  
  ASSERT_EQ(set_and(A, random_subset(A, 3)).size(), 3);
  ASSERT_EQ(set_and(A, random_subset(A, 3)).size(), 3);
  ASSERT_EQ(set_and(A, random_subset(A, 3)).size(), 3);

  ASSERT_EQ(set_and(A, random_subset(A, 1)).size(), 1);
  ASSERT_EQ(set_and(A, random_subset(A, 1)).size(), 1);
  ASSERT_EQ(set_and(A, random_subset(A, 1)).size(), 1);
  
  ASSERT_EQ(set_and(A, random_subset(A, 0)).size(), 0);
  ASSERT_EQ(set_and(A, random_subset(A, 0)).size(), 0);
  ASSERT_EQ(set_and(A, random_subset(A, 0)).size(), 0);

  ASSERT_EQ(set_and(A, random_subset(A, -4)).size(), 0);
  ASSERT_EQ(set_and(A, random_subset(A, -4)).size(), 0);
  ASSERT_EQ(set_and(A, random_subset(A, -4)).size(), 0);

  ASSERT_EQ(random_subset(empty, -4), empty);
  ASSERT_EQ(random_subset(empty, 0), empty);
  ASSERT_EQ(random_subset(empty, 7), empty);
  ASSERT_EQ(random_subset(empty, 10), empty);
  ASSERT_EQ(random_subset(empty, 20), empty);
}

#endif
