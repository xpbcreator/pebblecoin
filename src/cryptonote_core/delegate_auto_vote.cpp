// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <cmath>

#include "blockchain_storage.h"

#include "delegate_auto_vote.h"

namespace cryptonote
{
  // from http://www.evanmiller.org/how-not-to-sort-by-average-rating.html
  double ci_lower_bound(uint64_t pos, uint64_t n)
  {
    const static double z = 1.96; // 95% confidence interval
    
    if (pos > n)
    {
      throw std::runtime_error("Can't have more positive votes than total votes");
    }
    
    if (n == 0)
    {
      return 0.0;
    }
    
    double p_hat = 1.0 * pos / n;
    
    return (p_hat + z*z/(2*n) - z * sqrt((p_hat*(1-p_hat)+z*z/(4*n))/n))/(1+z*z/n);
  }
  
  double get_delegate_rank(const bs_delegate_info& info)
  {
    return ci_lower_bound(info.processed_blocks, info.processed_blocks + info.missed_blocks);
  }
}
