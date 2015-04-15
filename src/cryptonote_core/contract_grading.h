// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>

#define GRADE_SCALE_MAX 1000000000u // the amount that equals 100% when calculating contract grades/fees

namespace cryptonote
{
  uint64_t grade_backing_amount(uint64_t locked_amount, uint32_t grade, uint32_t fee_scale);
  uint64_t grade_contract_amount(uint64_t contract_amount, uint32_t grade, uint32_t fee_scale);
  
  uint64_t calculate_total_fee(uint64_t total_contract_coins, uint32_t fee_scale);
}
