// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <functional>

#pragma once

namespace tools
{
  class finally
  {
  public:
    finally(const std::function<void()>& f) : m_f(f) { }
    ~finally() { m_f(); }
    
  private:
    std::function<void()> m_f;
  };
  
  template <typename A, typename B>
  finally scoped_set_var(A& var, const B& tmp_val)
  {
    A initial_value = var;
    var = tmp_val;
    return finally([&var, initial_value]() { var = initial_value; });
  }
}
