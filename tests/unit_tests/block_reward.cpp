// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gtest/gtest.h"

#include "misc_language.h"

#include "cryptonote_config.h"
#include "cryptonote_core/cryptonote_basic_impl.h"
#include "common/finally.h"

using namespace cryptonote;

namespace cryptonote
{
  namespace detail
  {
    extern uint64_t standard_reward;
    extern uint64_t warmup_period_rewards[];
    extern size_t num_warmup_periods;
    uint64_t total_warmup_period();
    uint64_t warmup_period_length();
    
    // linear gradual cooldown over years
    extern size_t num_reward_eras;
    uint64_t reward_era_length();
    extern uint64_t penalty_per_era;
  }
}
namespace
{
  //--------------------------------------------------------------------------------------------------------------------
  class block_reward_and_already_generated_coins : public ::testing::Test
  {
  protected:
    static const size_t current_block_size = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE / 2;

    bool m_block_not_too_big;
    uint64_t m_block_reward;
  };

  #define TEST_COIN_GEN(height, expected_reward)                            \
    m_block_not_too_big = get_block_reward(0, current_block_size, 0, height, m_block_reward); \
    ASSERT_TRUE(m_block_not_too_big);                                                                       \
    ASSERT_EQ(((uint64_t)expected_reward), m_block_reward);

  TEST_F(block_reward_and_already_generated_coins, handles_first_values)
  {
    using namespace cryptonote::detail;
    
    TEST_COIN_GEN(0, 0);
    TEST_COIN_GEN(1, warmup_period_rewards[0] + ((warmup_period_rewards[1] - warmup_period_rewards[0])*1) / warmup_period_length());
    TEST_COIN_GEN(2, warmup_period_rewards[0] + ((warmup_period_rewards[1] - warmup_period_rewards[0])*2) / warmup_period_length());
    TEST_COIN_GEN(3, warmup_period_rewards[0] + ((warmup_period_rewards[1] - warmup_period_rewards[0])*3) / warmup_period_length());
    
    for (size_t i = 1; i < num_warmup_periods; i++)
    {
      TEST_COIN_GEN(i*warmup_period_length() - 1, warmup_period_rewards[i-1] + ((warmup_period_rewards[i] - warmup_period_rewards[i-1])*(warmup_period_length() - 1)) / warmup_period_length());
      TEST_COIN_GEN(i*warmup_period_length(), warmup_period_rewards[i]);
      TEST_COIN_GEN(i*warmup_period_length() + 1, warmup_period_rewards[i] + ((warmup_period_rewards[i+1] - warmup_period_rewards[i])*1) / warmup_period_length());
    }
    
    TEST_COIN_GEN(total_warmup_period(), standard_reward);
    
    // sanity checks:
    TEST_COIN_GEN(1, 993);
    TEST_COIN_GEN(2, 1985);
    TEST_COIN_GEN(3, 2977);

    TEST_COIN_GEN(warmup_period_length()-1, 300*COIN/10000 - 993);
    TEST_COIN_GEN(warmup_period_length(),   300*COIN/10000);
    TEST_COIN_GEN(warmup_period_length()+1, 300*COIN/10000 + 8928);
    
    TEST_COIN_GEN(warmup_period_length()*2-1, 300*COIN/1000 - 8929);
    TEST_COIN_GEN(warmup_period_length()*2,   300*COIN/1000);
    TEST_COIN_GEN(warmup_period_length()*2+1, 300*COIN/1000 + 89285);

    TEST_COIN_GEN(warmup_period_length()*3-1, 300*COIN/100 - 89286);
    TEST_COIN_GEN(warmup_period_length()*3,   300*COIN/100);
    TEST_COIN_GEN(warmup_period_length()*3+1, 300*COIN/100 + 892857);
    
    TEST_COIN_GEN(warmup_period_length()*4-1, 300*COIN/10 - 892858);
    TEST_COIN_GEN(warmup_period_length()*4,   300*COIN/10);
    TEST_COIN_GEN(warmup_period_length()*4+1, 300*COIN/10 + 8928571);
    
    TEST_COIN_GEN(warmup_period_length()*5-1, 300*COIN - 8928572);
    TEST_COIN_GEN(warmup_period_length()*5,   300*COIN);
    TEST_COIN_GEN(warmup_period_length()*5+1, 300*COIN);
  }

  TEST_F(block_reward_and_already_generated_coins, correctly_steps)
  {
    using namespace cryptonote::detail;
    
    auto ss = tools::scoped_set_var(cryptonote::config::dpos_switch_block, 0xffffffffffffffff);
    
    for (size_t into_era = 1; into_era < num_reward_eras; into_era++)
    {
      LOG_PRINT_L0("Testing into_era=" << into_era << ", length=" << reward_era_length() << ", standard = " << standard_reward << ", penalty_per=" << penalty_per_era);
      TEST_COIN_GEN(into_era*reward_era_length() - 1, standard_reward - penalty_per_era*(into_era - 1));
      TEST_COIN_GEN(into_era*reward_era_length(), standard_reward - penalty_per_era*into_era);
      TEST_COIN_GEN(into_era*reward_era_length() + 1, standard_reward - penalty_per_era*into_era);
    }
    
    // sanity checks:
    auto YEAR_HEIGHT = cryptonote::config::year_height();
    TEST_COIN_GEN(3 * YEAR_HEIGHT - 1, 300*COIN);
    TEST_COIN_GEN(3 * YEAR_HEIGHT,     200*COIN);
    TEST_COIN_GEN(3 * YEAR_HEIGHT + 1, 200*COIN);
    
    TEST_COIN_GEN(6 * YEAR_HEIGHT - 1, 200*COIN);
    TEST_COIN_GEN(6 * YEAR_HEIGHT,     100*COIN);
    TEST_COIN_GEN(6 * YEAR_HEIGHT + 1, 100*COIN);

    TEST_COIN_GEN(9 * YEAR_HEIGHT - 1, 100*COIN);
    TEST_COIN_GEN(9 * YEAR_HEIGHT,     0*COIN);
    TEST_COIN_GEN(9 * YEAR_HEIGHT + 1, 0*COIN);
  }
  
  //--------------------------------------------------------------------------------------------------------------------
  class block_reward_and_current_block_size : public ::testing::Test
  {
  protected:
    virtual void SetUp()
    {
      m_block_not_too_big = get_block_reward(0, 0, already_generated_coins, cryptonote::config::month_height(), m_standard_block_reward);
      ASSERT_TRUE(m_block_not_too_big);
      ASSERT_LT(CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE, m_standard_block_reward);
    }

    void do_test(size_t median_block_size, size_t current_block_size)
    {
      m_block_not_too_big = get_block_reward(median_block_size, current_block_size, already_generated_coins, cryptonote::config::month_height(), m_block_reward);
    }

    static const uint64_t already_generated_coins = 0;

    bool m_block_not_too_big;
    uint64_t m_block_reward;
    uint64_t m_standard_block_reward;
  };

  TEST_F(block_reward_and_current_block_size, handles_block_size_less_relevance_level)
  {
    do_test(0, CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE - 1);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, m_standard_block_reward);
  }

  TEST_F(block_reward_and_current_block_size, handles_block_size_eq_relevance_level)
  {
    do_test(0, CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, m_standard_block_reward);
  }

  TEST_F(block_reward_and_current_block_size, handles_block_size_gt_relevance_level)
  {
    do_test(0, CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE + 1);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_LT(m_block_reward, m_standard_block_reward);
  }

  TEST_F(block_reward_and_current_block_size, handles_block_size_less_2_relevance_level)
  {
    do_test(0, 2 * CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE - 1);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_LT(m_block_reward, m_standard_block_reward);
    ASSERT_LT(0, m_block_reward);
  }

  TEST_F(block_reward_and_current_block_size, handles_block_size_eq_2_relevance_level)
  {
    do_test(0, 2 * CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(0, m_block_reward);
  }

  TEST_F(block_reward_and_current_block_size, handles_block_size_gt_2_relevance_level)
  {
    do_test(0, 2 * CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE + 1);
    ASSERT_FALSE(m_block_not_too_big);
  }

  TEST_F(block_reward_and_current_block_size, fails_on_huge_median_size)
  {
#if !defined(NDEBUG)
    size_t huge_size = std::numeric_limits<uint32_t>::max() + UINT64_C(2);
    do_test(huge_size, huge_size + 1);
    ASSERT_FALSE(m_block_not_too_big);
#endif
  }

  TEST_F(block_reward_and_current_block_size, fails_on_huge_block_size)
  {
#if !defined(NDEBUG)
    size_t huge_size = std::numeric_limits<uint32_t>::max() + UINT64_C(2);
    do_test(huge_size - 2, huge_size);
    ASSERT_FALSE(m_block_not_too_big);
#endif
  }

  TEST_F(block_reward_and_current_block_size, dpos_reward_0)
  {
    auto ss = tools::scoped_set_var(cryptonote::config::dpos_switch_block, 1);
    
    do_test(0, CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE + 1);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(0, m_block_reward);
    
    do_test(0, 2 * CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE - 1);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(0, m_block_reward);
    
    do_test(0, 2 * CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(0, m_block_reward);
    
    do_test(0, 2 * CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE + 1);
    ASSERT_FALSE(m_block_not_too_big);
  }
  
  //--------------------------------------------------------------------------------------------------------------------
  class block_reward_and_last_block_sizes : public ::testing::Test
  {
  protected:
    virtual void SetUp()
    {
      m_last_block_sizes.push_back(3  * CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
      m_last_block_sizes.push_back(5  * CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
      m_last_block_sizes.push_back(7  * CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
      m_last_block_sizes.push_back(11 * CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
      m_last_block_sizes.push_back(13 * CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);

      m_last_block_sizes_median = 7 * CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE;

      m_block_not_too_big = get_block_reward(epee::misc_utils::median(m_last_block_sizes), 0, already_generated_coins, cryptonote::config::month_height(), m_standard_block_reward);
      ASSERT_TRUE(m_block_not_too_big);
      ASSERT_LT(CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE, m_standard_block_reward);
    }

    void do_test(size_t current_block_size)
    {
      m_block_not_too_big = get_block_reward(epee::misc_utils::median(m_last_block_sizes), current_block_size, already_generated_coins, cryptonote::config::month_height(), m_block_reward);
    }

    static const uint64_t already_generated_coins = 0;

    std::vector<size_t> m_last_block_sizes;
    uint64_t m_last_block_sizes_median;
    bool m_block_not_too_big;
    uint64_t m_block_reward;
    uint64_t m_standard_block_reward;
  };

  TEST_F(block_reward_and_last_block_sizes, handles_block_size_less_median)
  {
    do_test(m_last_block_sizes_median - 1);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, m_standard_block_reward);
  }

  TEST_F(block_reward_and_last_block_sizes, handles_block_size_eq_median)
  {
    do_test(m_last_block_sizes_median);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, m_standard_block_reward);
  }

  TEST_F(block_reward_and_last_block_sizes, handles_block_size_gt_median)
  {
    do_test(m_last_block_sizes_median + 1);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_LT(m_block_reward, m_standard_block_reward);
  }

  TEST_F(block_reward_and_last_block_sizes, handles_block_size_less_2_medians)
  {
    do_test(2 * m_last_block_sizes_median - 1);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_LT(m_block_reward, m_standard_block_reward);
    ASSERT_LT(0, m_block_reward);
  }

  TEST_F(block_reward_and_last_block_sizes, handles_block_size_eq_2_medians)
  {
    do_test(2 * m_last_block_sizes_median);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(0, m_block_reward);
  }

  TEST_F(block_reward_and_last_block_sizes, handles_block_size_gt_2_medians)
  {
    do_test(2 * m_last_block_sizes_median + 1);
    ASSERT_FALSE(m_block_not_too_big);
  }

  TEST_F(block_reward_and_last_block_sizes, calculates_correctly)
  {
    ASSERT_EQ(0, m_last_block_sizes_median % 8);

    do_test(m_last_block_sizes_median * 9 / 8);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, m_standard_block_reward * 63 / 64);

    // 3/2 = 12/8
    do_test(m_last_block_sizes_median * 3 / 2);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, m_standard_block_reward * 3 / 4);

    do_test(m_last_block_sizes_median * 15 / 8);
    ASSERT_TRUE(m_block_not_too_big);
    ASSERT_EQ(m_block_reward, m_standard_block_reward * 15 / 64);
  }
}
