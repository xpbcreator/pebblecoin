// Copyright (c) 2014-2015 The Cryptonote developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "include_base_utils.h"
#include "warnings.h"

#include "account.h"
#include "cryptonote_basic_impl.h"

using namespace std;

DISABLE_VS_WARNINGS(4244 4345)

namespace cryptonote
{
  //-----------------------------------------------------------------
  account_base::account_base()
  {
    set_null();
  }
  //-----------------------------------------------------------------
  void account_base::set_null()
  {
    m_keys = account_keys();
  }
  //-----------------------------------------------------------------
  void account_base::generate()
  {
    generate_keys(m_keys.m_account_address.m_spend_public_key, m_keys.m_spend_secret_key);
    generate_keys(m_keys.m_account_address.m_view_public_key, m_keys.m_view_secret_key);
    m_creation_timestamp = time(NULL);
  }
  //-----------------------------------------------------------------
  const account_keys& account_base::get_keys() const
  {
    return m_keys;
  }
  //-----------------------------------------------------------------
  std::string account_base::get_public_address_str()
  {
    //TODO: change this code into base 58
    return get_account_address_as_str(m_keys.m_account_address);
  }
  //-----------------------------------------------------------------
  bool operator ==(const cryptonote::account_public_address& a, const cryptonote::account_public_address& b)
  {
    return get_account_address_as_str(a) == get_account_address_as_str(b);
  }
  bool operator !=(const cryptonote::account_public_address& a, const cryptonote::account_public_address& b)
  {
    return !(a == b);
  }
  bool operator <(const cryptonote::account_public_address& a, const cryptonote::account_public_address& b)
  {
    return get_account_address_as_str(a) < get_account_address_as_str(b);
  }
}
