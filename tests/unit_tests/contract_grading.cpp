// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gtest/gtest.h"

#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/contract_grading.h"

template <class Iterable>
bool sumitall(const Iterable& v, uint64_t& result, const std::string descr)
{
  result = 0;
  BOOST_FOREACH(const auto& amt, v) {
    if (!cryptonote::add_amount(result, amt)) {
      std::cout << "Overflow summing " << descr << std::endl;
      return false;
    }
  }
  
  return true;
}

bool check_all_ok(std::vector<uint64_t> backing_amounts, std::vector<uint64_t> contract_amounts,
                  uint32_t grade, uint32_t fee_scale,
                  uint64_t& total_from_backing, uint64_t& total_from_contracted, uint64_t& total_fee)
{
  total_from_backing = total_from_contracted = total_fee = 0;
  
  if (!(fee_scale <= GRADE_SCALE_MAX)) {
    std::cout << "Invalid fee_scale" << std::endl;
    return false;
  }
  if (!(grade <= GRADE_SCALE_MAX)) {
    std::cout << "Invalid grade" << std::endl;
    return false;
  }
  
  uint64_t total_backing, total_contract;
  if (!sumitall(backing_amounts, total_backing, "backing_amounts"))
    return false;
  if (!sumitall(contract_amounts, total_contract, "total_contract"))
    return false;
  
  if (total_backing != total_contract)
  {
    std::cout << "total_backing != total_contract" << std::endl;
    return false;
  }
  
  std::vector<uint64_t> backing_results, contract_results;
  BOOST_FOREACH(const auto& amt, backing_amounts) {
    uint64_t unbacking_amt = cryptonote::grade_backing_amount(amt, grade, fee_scale);

    if (unbacking_amt > amt) {
      std::cout << "Unbacking more than was given" << std::endl;
      return false;
    }
    
    backing_results.push_back(unbacking_amt);
  }
  BOOST_FOREACH(const auto& amt, contract_amounts) {
    uint64_t spendable_amount = cryptonote::grade_contract_amount(amt, grade, fee_scale);
    
    if (spendable_amount > amt) {
      std::cout << "Unbacking more to spend than was given" << std::endl;
      return false;
    }
    
    contract_results.push_back(spendable_amount);
  }
  
  if (!sumitall(backing_results, total_from_backing, "total_from_backing"))
    return false;
  if (!sumitall(contract_results, total_from_contracted, "total_from_contracted"))
    return false;
  
  uint64_t total_results = total_from_backing;
  if (!cryptonote::add_amount(total_results, total_from_contracted)) {
    std::cout << "Overflow adding total from backing to total from contracted" << std::endl;
    return false;
  }
  
  total_fee = cryptonote::calculate_total_fee(total_backing, fee_scale);
  if (!cryptonote::add_amount(total_results, total_fee)) {
    std::cout << "Overflow adding total fee to total results" << std::endl;
    return false;
  }
  
  if (total_results > total_backing) {
    std::cout << "VERY BAD: Created " << total_results - total_backing << " coins!" << std::endl;
    return false;
  }

  if (total_results < total_backing) {
    uint64_t destroyed = total_backing - total_results;
    // should lose at most 2 coins (1 for grade, 1 for fee) per entry
    if (destroyed > backing_amounts.size()*2 + contract_amounts.size()*2) {
      std::cout << "Destroyed too many coins: " << total_results - total_backing << std::endl;
      return false;
    }
  }
  
  /*using namespace cryptonote;
  std::cout << "OK: total=" << print_money(total_backing) << ", destroyed=" << print_money(total_backing - total_results)
            << ", num_backing=" << backing_amounts.size() << ", num_contracts=" << contract_amounts.size()
            << ", grade=" << print_grade_scale(grade)
            << ", fee_scale=" << print_grade_scale(fee_scale)
            << ", backing_got=" << print_money(total_from_backing) << ", contracted_got=" << print_money(total_from_contracted)
            << ", fee_got=" << print_money(total_fee) << std::endl;*/
  return true;
}

bool check_all_ok(std::vector<uint64_t> backing_amounts, std::vector<uint64_t> contract_amounts,
                  uint32_t grade, uint32_t fee_scale)
{
  uint64_t total_from_backing, total_from_contracted, total_fee;
  return check_all_ok(backing_amounts, contract_amounts, grade, fee_scale,
                      total_from_backing, total_from_contracted, total_fee);
}

std::vector<uint64_t> split_amount(uint64_t amount, size_t num_elements) {
  if (amount / num_elements == 0)
    throw std::runtime_error("Too few elements");
  
  std::vector<uint64_t> result;
  uint64_t total = 0;
  for (size_t i = 0; i < num_elements; i++) {
    result.push_back(amount / num_elements);
    total += amount / num_elements;
  }
  if (total < amount) {
    result.push_back(amount - total);
    total += amount - total;
  }
  uint64_t sanity;
  if (!sumitall(result, sanity, "sanity"))
    throw std::runtime_error("Overflow when doing sanity check in split_amount");
  if (sanity != amount)
    throw std::runtime_error("Sanity check failed in split_amount");
  
  return result;
}

#if defined(_MSC_VER) && _MSC_VER < 1900

// doesn't support initializer lists

#else

TEST(contract_grading, all_contract_grading_ok)
{
  std::vector<uint64_t> grades = {
    0, 1, 3,
    GRADE_SCALE_MAX / 4,
    GRADE_SCALE_MAX / 3,
    GRADE_SCALE_MAX / 2,
    GRADE_SCALE_MAX * 3 / 4,
    GRADE_SCALE_MAX - 3, GRADE_SCALE_MAX - 1, GRADE_SCALE_MAX
  };
  std::vector<uint64_t> total_amounts = {
    1000, 76513,
    std::numeric_limits<uint64_t>::max() - 50,
    std::numeric_limits<uint64_t>::max() - 1,
    std::numeric_limits<uint64_t>::max()
  };
  std::vector<size_t> splits = {
    1, 2, 3, 10, 23,
    193
  };
  std::vector<size_t> fees = {
    0, 1, 3,
    GRADE_SCALE_MAX / 4,
    GRADE_SCALE_MAX / 3,
    GRADE_SCALE_MAX / 2,
    GRADE_SCALE_MAX * 3 / 4,
    GRADE_SCALE_MAX - 3, GRADE_SCALE_MAX - 1, GRADE_SCALE_MAX
  };
  BOOST_FOREACH(const auto& total, total_amounts) {
    BOOST_FOREACH(const auto& split_left, splits) {
      if (total / split_left == 0) continue;
      const auto& left = split_amount(total, split_left);
      BOOST_FOREACH(const auto& split_right, splits) {
        if (total / split_right == 0) continue;
        const auto& right = split_amount(total, split_right);
        BOOST_FOREACH(const auto& grade, grades) {
          BOOST_FOREACH(const auto& fee, fees) {
            ASSERT_TRUE(check_all_ok(left, right, grade, fee));
          }
        }
      }
    }
  }
}

struct contract_test_case {
  uint64_t total_amount;
  size_t split_left, split_right;
  uint32_t grade, fee;
  uint64_t expect_left, expect_right, expect_fee;
};

TEST(contract_grading, amount_checks)
{
  std::vector<contract_test_case> test_cases = {
    // straightforward split amount
    {1000, 1, 1, 0,                   0, 1000, 0,    0},
    {1000, 1, 1, GRADE_SCALE_MAX / 2, 0, 500,  500,  0},
    {1000, 1, 1, GRADE_SCALE_MAX,     0, 0,    1000, 0},
    
    // straightforward extract fee
    {1000, 1, 1, 0,                   GRADE_SCALE_MAX / 2, 500, 0,     500},
    {1000, 1, 1, GRADE_SCALE_MAX / 2, GRADE_SCALE_MAX / 2, 250,  250,  500},
    {1000, 1, 1, GRADE_SCALE_MAX,     GRADE_SCALE_MAX / 2, 0,    500,  500},
    {1000, 1, 1, 0,                   GRADE_SCALE_MAX / 10, 900, 0,     100},
    {1000, 1, 1, GRADE_SCALE_MAX / 2, GRADE_SCALE_MAX / 10, 450,  450,  100},
    {1000, 1, 1, GRADE_SCALE_MAX,     GRADE_SCALE_MAX / 10, 0,    900,  100},
    
    // simple rounding: round grade down and fee taken up
    {1000, 1, 1, GRADE_SCALE_MAX / 3, 0,                    666, 333, 0},
    {1000, 1, 1, GRADE_SCALE_MAX / 3, GRADE_SCALE_MAX / 10, 599, 299, 100},
    {1000, 1, 1, GRADE_SCALE_MAX / 3, GRADE_SCALE_MAX / 2,  333, 166, 500},
  };
  
  uint64_t tl, tr, fee;
    
  BOOST_FOREACH(const auto& test_case, test_cases) {
    ASSERT_TRUE(check_all_ok(split_amount(test_case.total_amount, test_case.split_left),
                             split_amount(test_case.total_amount, test_case.split_right),
                             test_case.grade, test_case.fee,
                             tl, tr, fee));
    ASSERT_EQ(test_case.expect_left, tl);
    ASSERT_EQ(test_case.expect_right, tr);
    ASSERT_EQ(test_case.expect_fee, fee);
  }
}

#endif
