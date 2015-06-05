// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <cstring>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <vector>
#include <boost/foreach.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include "common/boost_serialization_helper.h"
#include "crypto/crypto.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/cryptonote_basic_impl.h"
#include "cryptonote_core/account_boost_serialization.h"
#include "cryptonote_core/cryptonote_boost_serialization.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/tx_tester.h"
#include "cryptonote_core/nulls.h"
#include "cryptonote_core/keypair.h"
#include "serialization/serialization.h"
#include "serialization/binary_archive.h"
#include "serialization/json_archive.h"
#include "serialization/debug_archive.h"
#include "serialization/binary_utils.h"
#include "gtest/gtest.h"
using namespace std;

struct Struct
{
  int32_t a;
  int32_t b;
  char blob[8];
};

template <class Archive>
struct serializer<Archive, Struct>
{
  static bool serialize(Archive &ar, Struct &s) {
    ar.begin_object();
    ar.tag("a");
    ar.serialize_int(s.a);
    ar.tag("b");
    ar.serialize_int(s.b);
    ar.tag("blob");
    ar.serialize_blob(s.blob, sizeof(s.blob));
    ar.end_object();
    return true;
  }
};

struct Struct1
{
  vector<boost::variant<Struct, int32_t>> si;
  vector<int16_t> vi;

  BEGIN_SERIALIZE_OBJECT()
    FIELD(si)
    FIELD(vi)
  END_SERIALIZE()
  /*template <bool W, template <bool> class Archive>
  bool do_serialize(Archive<W> &ar)
  {
    ar.begin_object();
    ar.tag("si");
    ::do_serialize(ar, si);
    ar.tag("vi");
    ::do_serialize(ar, vi);
    ar.end_object();
  }*/
};

struct Blob
{
  uint64_t a;
  uint32_t b;

  bool operator==(const Blob& rhs) const
  {
    return a == rhs.a;
  }
};

VARIANT_TAG(binary_archive, Struct, 0xe0);
VARIANT_TAG(binary_archive, int, 0xe1);
VARIANT_TAG(json_archive, Struct, "struct");
VARIANT_TAG(json_archive, int, "int");
VARIANT_TAG(debug_archive, Struct1, "struct1");
VARIANT_TAG(debug_archive, Struct, "struct");
VARIANT_TAG(debug_archive, int, "int");

BLOB_SERIALIZER(Blob);

bool try_parse(const string &blob)
{
  Struct1 s1;
  return serialization::parse_binary(blob, s1);
}

TEST(Serialization, BinaryArchiveInts) {
  uint64_t x = 0xff00000000, x1;

  ostringstream oss;
  binary_archive<true> oar(oss);
  oar.serialize_int(x);
  ASSERT_TRUE(oss.good());
  ASSERT_EQ(8, oss.str().size());
  ASSERT_EQ(string("\0\0\0\0\xff\0\0\0", 8), oss.str());

  istringstream iss(oss.str());
  binary_archive<false> iar(iss);
  iar.serialize_int(x1);
  ASSERT_EQ(8, iss.tellg());
  ASSERT_TRUE(iss.good());

  ASSERT_EQ(x, x1);
}

TEST(Serialization, BinaryArchiveVarInts) {
  uint64_t x = 0xff00000000, x1;

  ostringstream oss;
  binary_archive<true> oar(oss);
  oar.serialize_varint(x);
  ASSERT_TRUE(oss.good());
  ASSERT_EQ(6, oss.str().size());
  ASSERT_EQ(string("\x80\x80\x80\x80\xF0\x1F", 6), oss.str());

  istringstream iss(oss.str());
  binary_archive<false> iar(iss);
  iar.serialize_varint(x1);
  ASSERT_TRUE(iss.good());
  ASSERT_EQ(x, x1);
}

TEST(Serialization, Test1) {
  ostringstream str;
  binary_archive<true> ar(str);

  Struct1 s1;
  s1.si.push_back(0);
  {
    Struct s;
    s.a = 5;
    s.b = 65539;
    std::memcpy(s.blob, "12345678", 8);
    s1.si.push_back(s);
  }
  s1.si.push_back(1);
  s1.vi.push_back(10);
  s1.vi.push_back(22);

  string blob;
  ASSERT_TRUE(serialization::dump_binary(s1, blob));
  ASSERT_TRUE(try_parse(blob));

  ASSERT_EQ('\xE0', blob[6]);
  blob[6] = '\xE1';
  ASSERT_FALSE(try_parse(blob));
  blob[6] = '\xE2';
  ASSERT_FALSE(try_parse(blob));
}

TEST(Serialization, Overflow) {
  Blob x = { 0xff00000000 };
  Blob x1;

  string blob;
  ASSERT_TRUE(serialization::dump_binary(x, blob));
  ASSERT_EQ(sizeof(Blob), blob.size());

  ASSERT_TRUE(serialization::parse_binary(blob, x1));
  ASSERT_EQ(x, x1);

  vector<Blob> bigvector;
  ASSERT_FALSE(serialization::parse_binary(blob, bigvector));
  ASSERT_EQ(0, bigvector.size());
}

TEST(Serialization, serializes_vector_uint64_as_varint)
{
  std::vector<uint64_t> v;
  string blob;

  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(1, blob.size());

  // +1 byte
  v.push_back(0);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(2, blob.size());

  // +1 byte
  v.push_back(1);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(3, blob.size());

  // +2 bytes
  v.push_back(0x80);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(5, blob.size());

  // +2 bytes
  v.push_back(0xFF);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(7, blob.size());

  // +2 bytes
  v.push_back(0x3FFF);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(9, blob.size());

  // +3 bytes
  v.push_back(0x40FF);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(12, blob.size());

  // +10 bytes
  v.push_back(0xFFFFFFFFFFFFFFFF);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(22, blob.size());
}

TEST(Serialization, serializes_vector_int64_as_fixed_int)
{
  std::vector<int64_t> v;
  string blob;

  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(1, blob.size());

  // +8 bytes
  v.push_back(0);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(9, blob.size());

  // +8 bytes
  v.push_back(1);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(17, blob.size());

  // +8 bytes
  v.push_back(0x80);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(25, blob.size());

  // +8 bytes
  v.push_back(0xFF);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(33, blob.size());

  // +8 bytes
  v.push_back(0x3FFF);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(41, blob.size());

  // +8 bytes
  v.push_back(0x40FF);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(49, blob.size());

  // +8 bytes
  v.push_back(0xFFFFFFFFFFFFFFFF);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(57, blob.size());
}

template<typename T>
bool blob_serialization_preserves_value(const T& cval)
{
  std::string blob;
  if (!serialization::dump_binary(cval, blob))
  {
    std::cout << "failed to dump binary" << std::endl;
    return false;
  }
  
  T parsed;
  if (!serialization::parse_binary(blob, parsed))
  {
    std::cout << "failed to parse binary" << std::endl;
    return false;
  }
  
  return cval == parsed;
}

template<typename T>
bool boost_serialization_preserves_value(const T& cval)
{
  T val = cval;
  
  std::stringstream ss;
  boost::archive::binary_oarchive o(ss);
  o << val;
  
  boost::archive::binary_iarchive i(ss);
  T loaded_val;
  i >> loaded_val;
  
  return cval == loaded_val;
}

template<typename T>
bool both_serialization_preserves_value(const T& cval)
{
  if (!blob_serialization_preserves_value(cval))
  {
    std::cout << "blob failed to preserve value" << std::endl;
    return false;
  }
  if (!boost_serialization_preserves_value(cval))
  {
    std::cout << "boost failed to preserve value" << std::endl;
    return false;
  }
  return true;
}

TEST(Serialization, serialization_preserves_value__string)
{
  ASSERT_TRUE(both_serialization_preserves_value(std::string("One two three")));
  ASSERT_TRUE(both_serialization_preserves_value(std::string(30, 'x')));
}

TEST(Serialization, serialization_preserves_value__crypto)
{
  auto h = crypto::cn_fast_hash("ABCDE", 5);
  ASSERT_TRUE(both_serialization_preserves_value(h));
  auto kp = cryptonote::keypair::generate();
  ASSERT_TRUE(both_serialization_preserves_value(kp.pub));
  ASSERT_TRUE(both_serialization_preserves_value(kp.sec));

  crypto::key_derivation derivation;
  crypto::generate_key_derivation(kp.pub, kp.sec, derivation);
  ASSERT_TRUE(both_serialization_preserves_value(derivation));
  
  crypto::key_image im;
  crypto::generate_key_image(kp.pub, kp.sec, im);
  ASSERT_TRUE(both_serialization_preserves_value(im));
  
  crypto::signature sig;
  crypto::generate_signature(h, kp.pub, kp.sec, sig);
  ASSERT_TRUE(both_serialization_preserves_value(sig));

  // vector of sigs checked in "serializes_transacion_signatures_correctly"
}

TEST(Serialization, serialization_preserves_value__map)
{
  std::map<uint64_t, uint64_t> m;
  m[100] = 200;
  ASSERT_TRUE(both_serialization_preserves_value(m));
  m[500] = 700;
  ASSERT_TRUE(both_serialization_preserves_value(m));
  m[900] = 10000;
  ASSERT_TRUE(both_serialization_preserves_value(m));
  
  std::unordered_map<std::string, bool> n;
  n["goorag"] = false;
  ASSERT_TRUE(both_serialization_preserves_value(m));
  n["maer"] = true;
  ASSERT_TRUE(both_serialization_preserves_value(m));
}

/*TEST(Serialization, serialization_preserves_value__variant)
{
}*/

TEST(Serialization, serialization_preserves_value__pair)
{
  ASSERT_TRUE(both_serialization_preserves_value(std::make_pair(100, 200)));
  ASSERT_TRUE(both_serialization_preserves_value(std::make_pair((uint64_t)9999, std::string("hurga"))));
  crypto::hash h;
  crypto::public_key k;
  ASSERT_TRUE(both_serialization_preserves_value(std::make_pair(h, k)));
  h = crypto::cn_fast_hash("this a str", 10);
  ASSERT_TRUE(both_serialization_preserves_value(std::make_pair(h, k)));
  k = cryptonote::keypair::generate().pub;
  ASSERT_TRUE(both_serialization_preserves_value(std::make_pair(h, k)));
}

TEST(Serialization, serialization_preserves_value__vector)
{
  {
    std::vector<int> v;
    ASSERT_TRUE(both_serialization_preserves_value(v));
    for (int i=0; i < 20; i++) {
      v.push_back(i * 10);
      ASSERT_TRUE(both_serialization_preserves_value(v));
    }
  }
  {
    std::vector<std::string> v;
    ASSERT_TRUE(both_serialization_preserves_value(v));
    for (int i=0; i < 20; i++) {
      v.push_back("mm.... ");
      ASSERT_TRUE(both_serialization_preserves_value(v));
    }
  }
}
/*
TEST(Serialization, serialization_preserves_value__tuple)
{
  { auto t = std::make_tuple(); ASSERT_TRUE(both_serialization_preserves_value(t)); }
  { auto t = std::make_tuple(1); ASSERT_TRUE(both_serialization_preserves_value(t)); }
  { auto t = std::make_tuple(std::string("hi")); ASSERT_TRUE(both_serialization_preserves_value(t)); }
  { auto t = std::make_tuple(9, (uint64_t)43); ASSERT_TRUE(both_serialization_preserves_value(t)); }
  { auto t = std::make_tuple(std::string("a"), std::string("b"), std::string("c"));
    ASSERT_TRUE(both_serialization_preserves_value(t)); }
  { auto t = std::make_tuple(1, 2, 3, 4, 5); ASSERT_TRUE(both_serialization_preserves_value(t)); }
  { auto t = std::make_tuple(100, 'z', std::string("cor"), std::make_pair(1, 2));
    ASSERT_TRUE(both_serialization_preserves_value(t)); }
}*/

namespace
{
  template<typename T>
  std::vector<T> linearize_vector2(const std::vector< std::vector<T> >& vec_vec)
  {
    std::vector<T> res;
    BOOST_FOREACH(const auto& vec, vec_vec)
    {
      res.insert(res.end(), vec.begin(), vec.end());
    }
    return res;
  }
}

TEST(Serialization, serializes_transacion_signatures_correctly)
{
  using namespace cryptonote;

  transaction tx;
  transaction tx1;
  string blob;

  // Empty tx
  tx.set_null();
  ASSERT_TRUE(serialization::dump_binary(tx, blob));
  ASSERT_EQ(5, blob.size()); // 5 bytes + 0 bytes extra + 0 bytes signatures
  ASSERT_TRUE(serialization::parse_binary(blob, tx1));
  ASSERT_EQ(tx, tx1);
  ASSERT_EQ(linearize_vector2(tx.signatures), linearize_vector2(tx1.signatures));

  // Miner tx without signatures
  txin_gen txin_gen1;
  txin_gen1.height = 0;
  tx.set_null();
  tx.version = VANILLA_TRANSACTION_VERSION;
  tx.add_in(txin_gen1, CP_XPB);
  ASSERT_TRUE(serialization::dump_binary(tx, blob));
  ASSERT_EQ(7, blob.size()); // 5 bytes + 2 bytes vin[0] + 0 bytes extra + 0 bytes signatures
  ASSERT_TRUE(serialization::parse_binary(blob, tx1));
  ASSERT_EQ(tx, tx1);
  ASSERT_EQ(linearize_vector2(tx.signatures), linearize_vector2(tx1.signatures));

  // Miner tx with empty signatures 2nd vector
  tx.signatures.resize(1);
  ASSERT_TRUE(serialization::dump_binary(tx, blob));
  ASSERT_EQ(7, blob.size()); // 5 bytes + 2 bytes vin[0] + 0 bytes extra + 0 bytes signatures
  ASSERT_TRUE(serialization::parse_binary(blob, tx1));
  ASSERT_EQ(tx, tx1);
  ASSERT_EQ(linearize_vector2(tx.signatures), linearize_vector2(tx1.signatures));

  // Miner tx with one signature
  tx.signatures[0].resize(1);
  ASSERT_FALSE(serialization::dump_binary(tx, blob));

  // Miner tx with 2 empty vectors
  tx.signatures.resize(2);
  tx.signatures[0].resize(0);
  tx.signatures[1].resize(0);
  ASSERT_FALSE(serialization::dump_binary(tx, blob));

  // Miner tx with 2 signatures
  tx.signatures[0].resize(1);
  tx.signatures[1].resize(1);
  ASSERT_FALSE(serialization::dump_binary(tx, blob));

  // Two txin_gen, no signatures
  tx.add_in(txin_gen1, CP_XPB);
  tx.signatures.resize(0);
  ASSERT_TRUE(serialization::dump_binary(tx, blob));
  ASSERT_EQ(9, blob.size()); // 5 bytes + 2 * 2 bytes vins + 0 bytes extra + 0 bytes signatures
  ASSERT_TRUE(serialization::parse_binary(blob, tx1));
  ASSERT_EQ(tx, tx1);
  ASSERT_EQ(linearize_vector2(tx.signatures), linearize_vector2(tx1.signatures));

  // Two txin_gen, signatures vector contains only one empty element
  tx.signatures.resize(1);
  ASSERT_FALSE(serialization::dump_binary(tx, blob));

  // Two txin_gen, signatures vector contains two empty elements
  tx.signatures.resize(2);
  ASSERT_TRUE(serialization::dump_binary(tx, blob));
  ASSERT_EQ(9, blob.size()); // 5 bytes + 2 * 2 bytes vins + 0 bytes extra + 0 bytes signatures
  ASSERT_TRUE(serialization::parse_binary(blob, tx1));
  ASSERT_EQ(tx, tx1);
  ASSERT_EQ(linearize_vector2(tx.signatures), linearize_vector2(tx1.signatures));

  // Two txin_gen, signatures vector contains three empty elements
  tx.signatures.resize(3);
  ASSERT_FALSE(serialization::dump_binary(tx, blob));

  // Two txin_gen, signatures vector contains two non empty elements
  tx.signatures.resize(2);
  tx.signatures[0].resize(1);
  tx.signatures[1].resize(1);
  ASSERT_FALSE(serialization::dump_binary(tx, blob));

  // A few bytes instead of signature
  tx.clear_ins();
  tx.add_in(txin_gen1, CP_XPB);
  tx.signatures.clear();
  ASSERT_TRUE(serialization::dump_binary(tx, blob));
  blob.append(std::string(sizeof(crypto::signature) / 2, 'x'));
  ASSERT_FALSE(serialization::parse_binary(blob, tx1));

  // blob contains one signature
  blob.append(std::string(sizeof(crypto::signature) / 2, 'y'));
  ASSERT_FALSE(serialization::parse_binary(blob, tx1));

  // Not enough signature vectors for all inputs
  txin_to_key txin_to_key1;
  txin_to_key1.key_offsets.resize(2);
  txin_vote txin_vote1;
  txin_vote1.ink.key_offsets.resize(2);
  tx.version = DPOS_TRANSACTION_VERSION;
  tx.clear_ins();
  tx.add_in(txin_to_key1, CP_XPB);
  tx.add_in(txin_vote1, CP_XPB);
  tx.signatures.resize(1);
  tx.signatures[0].resize(2);
  ASSERT_FALSE(serialization::dump_binary(tx, blob));

  // Too much signatures for two inputs
  tx.signatures.resize(3);
  tx.signatures[0].resize(2);
  tx.signatures[1].resize(2);
  tx.signatures[2].resize(2);
  ASSERT_FALSE(serialization::dump_binary(tx, blob));

  // First signatures vector contains too little elements
  tx.signatures.resize(2);
  tx.signatures[0].resize(1);
  tx.signatures[1].resize(2);
  ASSERT_FALSE(serialization::dump_binary(tx, blob));

  // First signatures vector contains too much elements
  tx.signatures.resize(2);
  tx.signatures[0].resize(3);
  tx.signatures[1].resize(2);
  ASSERT_FALSE(serialization::dump_binary(tx, blob));

  // There are signatures for each input
  tx.signatures.resize(2);
  tx.signatures[0].resize(2);
  tx.signatures[1].resize(2);
  ASSERT_TRUE(serialization::dump_binary(tx, blob));
  ASSERT_TRUE(serialization::parse_binary(blob, tx1));
  ASSERT_EQ(tx, tx1);
  ASSERT_EQ(linearize_vector2(tx.signatures), linearize_vector2(tx1.signatures));

  // Blob doesn't contain enough data
  blob.resize(blob.size() - sizeof(crypto::signature) / 2);
  ASSERT_FALSE(serialization::parse_binary(blob, tx1));

  // Blob contains too much data
  blob.resize(blob.size() + sizeof(crypto::signature));
  ASSERT_FALSE(serialization::parse_binary(blob, tx1));

  // Blob contains one excess signature
  blob.resize(blob.size() + sizeof(crypto::signature) / 2);
  ASSERT_FALSE(serialization::parse_binary(blob, tx1));
}

bool _get_hash(const cryptonote::transaction& tx, crypto::hash& h)
{
  return get_transaction_hash(tx, h);
}
bool _get_hash(const cryptonote::block& bl, crypto::hash& h)
{
  return get_block_hash(bl, h);
}

crypto::hash g_ignore;

template<typename T>
bool blob_serialization_preserves_hash(const T& tx, crypto::hash& result=g_ignore)
{
  using namespace cryptonote;
  
  crypto::hash pre_hash;
  if (!_get_hash(tx, pre_hash))
    return false;
  
  // blob serialization
  blobdata tx_blob;
  if (!t_serializable_object_to_blob(tx, tx_blob))
    return false;
  
  T loaded_from_blob;
  if (!t_serializable_object_from_blob(tx_blob, loaded_from_blob))
    return false;
  
  crypto::hash post_blob_hash;
  if (!_get_hash(loaded_from_blob, post_blob_hash))
    return false;
  
  result = pre_hash;
  return pre_hash == post_blob_hash;
}

template<typename T>
bool boost_serialization_preserves_hash(const T& tx, crypto::hash& result=g_ignore)
{
  crypto::hash pre_hash;
  if (!_get_hash(tx, pre_hash))
    return false;

  std::stringstream ss;
  boost::archive::binary_oarchive o(ss);
  o << tx;
  
  boost::archive::binary_iarchive i(ss);
  T loaded_from_boost;
  i >> loaded_from_boost;
  
  crypto::hash post_boost_hash;
  if (!_get_hash(loaded_from_boost, post_boost_hash))
    return false;

  result = pre_hash;
  return pre_hash == post_boost_hash;
}

template<typename T>
bool both_serialization_preserves_hash(const T& tx, crypto::hash& result=g_ignore)
{
  return blob_serialization_preserves_hash(tx, result) && boost_serialization_preserves_hash(tx, result);
}

TEST(Serialization, serialization_preserves_id_hash)
{
  using namespace cryptonote;
  
  transaction tx;
  tx_tester txt(tx);
  tx.set_null();
  ASSERT_TRUE(both_serialization_preserves_hash(tx));
  
  auto keypair = keypair::generate();
  
  tx.version = VANILLA_TRANSACTION_VERSION;
  tx.add_out(tx_out(999, txout_to_key(keypair.pub)), CP_XPB);
  ASSERT_TRUE(both_serialization_preserves_hash(tx));
  tx.add_in(txin_gen(), CP_XPB);
  ASSERT_TRUE(both_serialization_preserves_hash(tx));
  tx.add_in(txin_to_key(), CP_XPB);
  ASSERT_TRUE(both_serialization_preserves_hash(tx));
  
  tx.set_null();
  tx.version = VANILLA_TRANSACTION_VERSION;
  txt.vout.push_back(tx_out(999, txout_to_key(keypair.pub)));
  ASSERT_FALSE(both_serialization_preserves_hash(tx)); // mismatch in coin_type and vout
  txt.vout_coin_types.push_back(coin_type(CURRENCY_XPB, NotContract, BACKED_BY_N_A));
  ASSERT_TRUE(both_serialization_preserves_hash(tx));
  txt.vin.push_back(txin_gen());
  ASSERT_FALSE(both_serialization_preserves_hash(tx));
  txt.vin.push_back(txin_to_key());
  ASSERT_FALSE(both_serialization_preserves_hash(tx));
  txt.vin_coin_types.push_back(coin_type(CURRENCY_XPB, NotContract, BACKED_BY_N_A));
  ASSERT_FALSE(both_serialization_preserves_hash(tx));
  txt.vin_coin_types.push_back(coin_type(CURRENCY_XPB, NotContract, BACKED_BY_N_A));
  ASSERT_TRUE(both_serialization_preserves_hash(tx));
  
  tx.set_null();
  tx.version = VANILLA_TRANSACTION_VERSION;
  txt.vout.push_back(tx_out(999, txout_to_key(keypair.pub)));
  txt.vout_coin_types.push_back(coin_type(CURRENCY_XPB + 43, NotContract, BACKED_BY_N_A));
  ASSERT_FALSE(both_serialization_preserves_hash(tx)); // invalid currency

  tx.set_null();
  tx.version = VANILLA_TRANSACTION_VERSION;
  txt.vout.push_back(tx_out(999, txout_to_key(keypair.pub)));
  txt.vout_coin_types.push_back(coin_type(CURRENCY_XPB, NotContract, CURRENCY_XPB));
  ASSERT_FALSE(both_serialization_preserves_hash(tx)); // invalid backed_by

  tx.set_null();
  tx.version = VANILLA_TRANSACTION_VERSION;
  txt.vout.push_back(tx_out(999, txout_to_key(keypair.pub)));
  txt.vout_coin_types.push_back(coin_type(CURRENCY_XPB, BackingCoin, BACKED_BY_N_A));
  ASSERT_FALSE(both_serialization_preserves_hash(tx)); // invalid contract_type

  tx.set_null();
  tx.version = VANILLA_TRANSACTION_VERSION;
  txt.vout.push_back(tx_out(999, txout_to_key(keypair.pub)));
  txt.vout_coin_types.push_back(coin_type(CURRENCY_XPB, ContractCoin, BACKED_BY_N_A));
  ASSERT_FALSE(both_serialization_preserves_hash(tx)); // invalid contract_type
/*
  tx.set_null();
  tx.version = CURRENCY_TRANSACTION_VERSION;
  txt.vout.push_back(tx_out(999, txout_to_key(keypair.pub)));
  txt.vout_coin_types.push_back(coin_type(453, NotContract, 6789));
  ASSERT_FALSE(both_serialization_preserves_hash(tx)); // invalid backed_by
  
  tx.set_null();
  tx.version = CURRENCY_TRANSACTION_VERSION;
  txt.vout.push_back(tx_out(999, txout_to_key(keypair.pub)));
  txt.vout_coin_types.push_back(coin_type(453, BackingCoin, 6789));
  ASSERT_FALSE(both_serialization_preserves_hash(tx)); // invalid contract_type
  
  tx.set_null();
  tx.version = CURRENCY_TRANSACTION_VERSION;
  txt.vout.push_back(tx_out(999, txout_to_key(keypair.pub)));
  txt.vout_coin_types.push_back(coin_type(453, NotContract, BACKED_BY_N_A));
  ASSERT_TRUE(both_serialization_preserves_hash(tx));
  
  tx.set_null();
  tx.version = CURRENCY_TRANSACTION_VERSION;
  
  {
    txin_mint in_m;
    in_m.currency = 20;
    in_m.description = "hoorg";
    in_m.decimals = 4;
    in_m.amount = 3000;
    in_m.remint_key = keypair.pub;
    tx.add_in(in_m, coin_type(20, NotContract, BACKED_BY_N_A));
  }
  ASSERT_TRUE(both_serialization_preserves_hash(tx));
  
  {
    txin_remint re_m;
    re_m.currency = 20;
    re_m.amount = 4000;
    re_m.new_remint_key = keypair.pub;
    tx.add_in(re_m, coin_type(20, NotContract, BACKED_BY_N_A));
  }
  ASSERT_TRUE(both_serialization_preserves_hash(tx));
  
  tx.version = CONTRACT_TRANSACTION_VERSION;
  {
    txin_create_contract c;
    c.contract = 400;
    c.description = "A contract";
    c.grading_key = keypair.pub;
    c.fee_scale = 4;
    c.expiry_block = 9000;
    c.default_grade = 44555;
    tx.add_in(c, coin_type(400, BackingCoin, 555));
  }
  ASSERT_TRUE(both_serialization_preserves_hash(tx));
  
  {
    txin_mint_contract m;
    m.contract = 5;
    m.backed_by_currency = 30;
    m.amount = 4000;
    tx.add_in(m, coin_type(1, NotContract, 3));
  }

  ASSERT_TRUE(both_serialization_preserves_hash(tx));
  {
    txin_grade_contract g;
    g.contract = 400;
    g.grade = 100;
    g.fee_amounts[20] = 400;
    g.fee_amounts[0] = 90000;
    tx.add_in(g, coin_type(6, ContractCoin, 10000));
  }
  ASSERT_TRUE(both_serialization_preserves_hash(tx));

  {
    txin_resolve_bc_coins g;
    g.contract = 400;
    g.is_backing_coins = 1;
    g.backing_currency = 4343;
    g.source_amount = 10000;
    g.graded_amount = 4;
    tx.add_in(g, coin_type(1, BackingCoin, 20));
  }
  ASSERT_TRUE(both_serialization_preserves_hash(tx));

  {
    txin_fuse_bc_coins g;
    g.contract = 400;
    g.backing_currency = 4343;
    g.amount = 3456;
    tx.add_in(g, coin_type(1, BackingCoin, 33345));
  }
  ASSERT_TRUE(both_serialization_preserves_hash(tx));*/
  
  tx.set_null();
  tx.version = VANILLA_TRANSACTION_VERSION;
  {
    txin_register_delegate g;
    g.delegate_id = 400;
    g.registration_fee = 130034;
    tx.add_in(g, CP_XPB);
  }
  ASSERT_FALSE(both_serialization_preserves_hash(tx)); // invalid tx version
  
  tx.version = DPOS_TRANSACTION_VERSION;
  ASSERT_TRUE(both_serialization_preserves_hash(tx));
  
  {
    txin_vote v;
    v.ink = txin_to_key();
    v.seq = 33;
    v.votes.insert(30);
    v.votes.insert(44);
    tx.add_in(v, CP_XPB);
  }
  ASSERT_TRUE(both_serialization_preserves_hash(tx));
  
  tx.set_null();
  tx.version = DPOS_TRANSACTION_VERSION;
  txt.vout.push_back(tx_out(999, txout_to_key(keypair.pub)));
  txt.vout_coin_types.push_back(coin_type(CURRENCY_XPB + 43, NotContract, BACKED_BY_N_A));
  ASSERT_FALSE(both_serialization_preserves_hash(tx)); // invalid currency

  tx.set_null();
  tx.version = DPOS_TRANSACTION_VERSION;
  txt.vout.push_back(tx_out(999, txout_to_key(keypair.pub)));
  txt.vout_coin_types.push_back(coin_type(CURRENCY_XPB, NotContract, CURRENCY_XPB));
  ASSERT_FALSE(both_serialization_preserves_hash(tx)); // invalid backed_by

  tx.set_null();
  tx.version = DPOS_TRANSACTION_VERSION;
  txt.vout.push_back(tx_out(999, txout_to_key(keypair.pub)));
  txt.vout_coin_types.push_back(coin_type(CURRENCY_XPB, BackingCoin, BACKED_BY_N_A));
  ASSERT_FALSE(both_serialization_preserves_hash(tx)); // invalid contract_type

  tx.set_null();
  tx.version = DPOS_TRANSACTION_VERSION;
  txt.vout.push_back(tx_out(999, txout_to_key(keypair.pub)));
  txt.vout_coin_types.push_back(coin_type(CURRENCY_XPB, ContractCoin, BACKED_BY_N_A));
  ASSERT_FALSE(both_serialization_preserves_hash(tx)); // invalid contract_type
}

TEST(Serialization, dpos_block_serialization)
{
  using namespace cryptonote;
  
  block b1, b2;
  b1.major_version = DPOS_BLOCK_MAJOR_VERSION;
  b1.minor_version = DPOS_BLOCK_MINOR_VERSION;
  b1.timestamp = 134455555;
  b1.prev_id = null_hash;
  b1.nonce = 0;
  b1.miner_tx = transaction();
  b1.miner_tx.set_null();
  b1.tx_hashes.clear();
  b1.signing_delegate_id = 444;
  b1.dpos_sig = crypto::signature();
  
  crypto::hash h1, h2;
  ASSERT_TRUE(both_serialization_preserves_hash(b1, h1));
  
  // nonce should not be hashed, shouldn't affect it
  b2 = b1;
  b2.nonce = 155;
  ASSERT_TRUE(both_serialization_preserves_hash(b2, h2));
  ASSERT_EQ(b1, b2);
  ASSERT_EQ(h1, h2);
  
  // signingdelegate  &dpos sig should affect it
  b2 = b1;
  b2.signing_delegate_id += 1;
  ASSERT_TRUE(both_serialization_preserves_hash(b2, h2));
  ASSERT_NE(b1, b2);
  ASSERT_NE(h1, h2);
  
  // signingdelegate  &dpos sig should affect it
  b2 = b1;
  (reinterpret_cast<char *>(&b2.dpos_sig))[0] += 1;
  ASSERT_TRUE(both_serialization_preserves_hash(b2, h2));
  ASSERT_NE(b1, b2);
  ASSERT_NE(h1, h2);
}
