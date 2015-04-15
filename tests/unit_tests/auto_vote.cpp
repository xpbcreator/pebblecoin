// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gtest/gtest.h"

#include "cryptonote_core/delegate_auto_vote.h"
#include "cryptonote_core/blockchain_storage.h"

namespace cryptonote
{
  double ci_lower_bound(uint64_t pos, uint64_t n);
}
using namespace cryptonote;

TEST(auto_vote, ci_checks)
{
  ASSERT_TRUE(ci_lower_bound(0, 0) == 0);
  
  // % matters more than absolute # of votes
  ASSERT_TRUE(ci_lower_bound(600, 1000) > ci_lower_bound(5500, 10000));
  
  // more ratings matter more
  ASSERT_TRUE(ci_lower_bound(1, 1) < ci_lower_bound(580, 600));
  ASSERT_TRUE(ci_lower_bound(10, 20) < ci_lower_bound(20, 40));
  ASSERT_TRUE(ci_lower_bound(10, 20) < ci_lower_bound(100, 200));
  ASSERT_TRUE(ci_lower_bound(20, 20) < ci_lower_bound(190, 200));
}
