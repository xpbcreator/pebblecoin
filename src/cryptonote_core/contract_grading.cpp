// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <cassert>

#include <boost/multiprecision/cpp_int.hpp>

#include "contract_grading.h"

namespace cryptonote
{
  uint64_t grade_amount(uint64_t full_amount, uint32_t grade, uint32_t fee_scale)
  {
    assert(grade <= GRADE_SCALE_MAX);
    assert(fee_scale <= GRADE_SCALE_MAX);
    
    // amount_left will be rounded down
    boost::multiprecision::uint128_t graded_amount = full_amount;
    graded_amount *= grade;
    graded_amount /= GRADE_SCALE_MAX;
    assert(graded_amount == (uint64_t)graded_amount);
    
    // calculate fee
    boost::multiprecision::uint128_t fee = graded_amount;
    fee *= fee_scale;
    fee /= GRADE_SCALE_MAX;
    assert(fee == (uint64_t)fee);
    // round fee taken up to prevent coins from being created via fee rounding
    while (fee_scale > 0 && fee * GRADE_SCALE_MAX / fee_scale < graded_amount)
    {
      fee += 1;
    }
    assert(fee <= graded_amount);
    
    boost::multiprecision::uint128_t amount_left = graded_amount - fee;
    assert(amount_left == amount_left.convert_to<uint64_t>());
    return amount_left.convert_to<uint64_t>();
  }
  
  uint64_t grade_contract_amount(uint64_t contract_amount, uint32_t grade, uint32_t fee_scale)
  {
    // coins resolve to the grade
    return grade_amount(contract_amount, grade, fee_scale);
  }

  uint64_t grade_backing_amount(uint64_t locked_amount, uint32_t grade, uint32_t fee_scale)
  {
    // the backing coins resolve to the other side of the contract
    assert(grade <= GRADE_SCALE_MAX);
    return grade_amount(locked_amount, GRADE_SCALE_MAX - grade, fee_scale);
  }
  
  
  uint64_t calculate_total_fee(uint64_t total_contract_coins, uint32_t fee_scale)
  {
    assert(fee_scale <= GRADE_SCALE_MAX);
    
    // round fee reward down
    boost::multiprecision::uint128_t fee = total_contract_coins;
    fee *= fee_scale;
    fee /= GRADE_SCALE_MAX;
    assert(fee == fee.convert_to<uint64_t>());
    
    return fee.convert_to<uint64_t>();
  }
}
