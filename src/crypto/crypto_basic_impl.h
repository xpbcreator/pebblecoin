// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "string_tools.h"

#include "crypto.h"

template <class T>
std::string dump_hash256(const T& v) {
  return epee::string_tools::pod_to_hex(v);
}

template <class T>
std::ostream &print256(std::ostream &o, const T &v) {
  return o << "<" << dump_hash256(v) << ">";
}

bool parse_hash256(const std::string str_hash, crypto::hash& hash);

namespace crypto {
  inline std::ostream &operator <<(std::ostream &o, const crypto::public_key &v) { return print256(o, v); }
  inline std::ostream &operator <<(std::ostream &o, const crypto::secret_key &v) { return print256(o, v); }
  inline std::ostream &operator <<(std::ostream &o, const crypto::key_derivation &v) { return print256(o, v); }
  inline std::ostream &operator <<(std::ostream &o, const crypto::key_image &v) { return print256(o, v); }
  inline std::ostream &operator <<(std::ostream &o, const crypto::signature &v) { return print256(o, v); }
  inline std::ostream &operator <<(std::ostream &o, const crypto::hash &v) { return print256(o, v); }
}
