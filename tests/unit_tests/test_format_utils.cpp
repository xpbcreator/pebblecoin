// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <vector>

#include "gtest/gtest.h"

#include "common/util.h"
#include "cryptonote_config.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/nulls.h"


TEST(parse_tx_extra, handles_empty_extra)
{
  std::vector<uint8_t> extra;;
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_TRUE(tx_extra_fields.empty());
}

TEST(parse_tx_extra, handles_padding_only_size_1)
{
  const uint8_t extra_arr[] = {0};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(1, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_padding), tx_extra_fields[0].type());
  ASSERT_EQ(1, boost::get<cryptonote::tx_extra_padding>(tx_extra_fields[0]).size);
}

TEST(parse_tx_extra, handles_padding_only_size_2)
{
  const uint8_t extra_arr[] = {0, 0};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(1, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_padding), tx_extra_fields[0].type());
  ASSERT_EQ(2, boost::get<cryptonote::tx_extra_padding>(tx_extra_fields[0]).size);
}

TEST(parse_tx_extra, handles_padding_only_max_size)
{
  std::vector<uint8_t> extra(TX_EXTRA_NONCE_MAX_COUNT, 0);
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(1, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_padding), tx_extra_fields[0].type());
  ASSERT_EQ(TX_EXTRA_NONCE_MAX_COUNT, boost::get<cryptonote::tx_extra_padding>(tx_extra_fields[0]).size);
}

TEST(parse_tx_extra, handles_padding_only_exceed_max_size)
{
  std::vector<uint8_t> extra(TX_EXTRA_NONCE_MAX_COUNT + 1, 0);
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_FALSE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
}

TEST(parse_tx_extra, handles_invalid_padding_only)
{
  std::vector<uint8_t> extra(2, 0);
  extra[1] = 42;
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_FALSE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
}

TEST(parse_tx_extra, handles_pub_key_only)
{
  const uint8_t extra_arr[] = {1, 30, 208, 98, 162, 133, 64, 85, 83, 112, 91, 188, 89, 211, 24, 131, 39, 154, 22, 228,
    80, 63, 198, 141, 173, 111, 244, 183, 4, 149, 186, 140, 230};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(1, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_pub_key), tx_extra_fields[0].type());
}

TEST(parse_tx_extra, handles_extra_nonce_only)
{
  const uint8_t extra_arr[] = {2, 1, 42};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(1, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_nonce), tx_extra_fields[0].type());
  cryptonote::tx_extra_nonce extra_nonce = boost::get<cryptonote::tx_extra_nonce>(tx_extra_fields[0]);
  ASSERT_EQ(1, extra_nonce.nonce.size());
  ASSERT_EQ(42, extra_nonce.nonce[0]);
}

TEST(parse_tx_extra, handles_pub_key_and_padding)
{
  const uint8_t extra_arr[] = {1, 30, 208, 98, 162, 133, 64, 85, 83, 112, 91, 188, 89, 211, 24, 131, 39, 154, 22, 228,
    80, 63, 198, 141, 173, 111, 244, 183, 4, 149, 186, 140, 230, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(2, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_pub_key), tx_extra_fields[0].type());
  ASSERT_EQ(typeid(cryptonote::tx_extra_padding), tx_extra_fields[1].type());
}

TEST(parse_and_validate_tx_extra, is_valid_tx_extra_parsed)
{
  cryptonote::transaction tx = AUTO_VAL_INIT(tx);
  cryptonote::account_base acc;
  acc.generate();
  cryptonote::blobdata b = "dsdsdfsdfsf";
  ASSERT_TRUE(cryptonote::construct_miner_tx(0, 0, 10000000000000, 1000, DEFAULT_FEE, acc.get_keys().m_account_address, tx, b, 1));
  crypto::public_key tx_pub_key = cryptonote::get_tx_pub_key_from_extra(tx);
  ASSERT_NE(tx_pub_key, cryptonote::null_pkey);
}
TEST(parse_and_validate_tx_extra, fails_on_big_extra_nonce)
{
  cryptonote::transaction tx = AUTO_VAL_INIT(tx);
  cryptonote::account_base acc;
  acc.generate();
  cryptonote::blobdata b(TX_EXTRA_NONCE_MAX_COUNT + 1, 0);
  ASSERT_FALSE(cryptonote::construct_miner_tx(0, 0, 10000000000000, 1000, DEFAULT_FEE, acc.get_keys().m_account_address, tx, b, 1));
}
TEST(parse_and_validate_tx_extra, fails_on_wrong_size_in_extra_nonce)
{
  cryptonote::transaction tx = AUTO_VAL_INIT(tx);
  tx.extra.resize(20, 0);
  tx.extra[0] = TX_EXTRA_NONCE;
  tx.extra[1] = 255;
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_FALSE(cryptonote::parse_tx_extra(tx.extra, tx_extra_fields));
}
TEST(validate_parse_amount_case, validate_parse_amount)
{
  uint64_t res = 0;
  bool r = cryptonote::parse_amount(res, "0.0001");
  ASSERT_TRUE(r);
  ASSERT_EQ(res, 10000);

  r = cryptonote::parse_amount(res, "100.0001");
  ASSERT_TRUE(r);
  ASSERT_EQ(res, 10000010000);

  r = cryptonote::parse_amount(res, "000.0000");
  ASSERT_TRUE(r);
  ASSERT_EQ(res, 0);

  r = cryptonote::parse_amount(res, "0");
  ASSERT_TRUE(r);
  ASSERT_EQ(res, 0);


  r = cryptonote::parse_amount(res, "   100.0001    ");
  ASSERT_TRUE(r);
  ASSERT_EQ(res, 10000010000);

  r = cryptonote::parse_amount(res, "   100.0000    ");
  ASSERT_TRUE(r);
  ASSERT_EQ(res, 10000000000);

  r = cryptonote::parse_amount(res, "   100. 0000    ");
  ASSERT_FALSE(r);

  r = cryptonote::parse_amount(res, "100. 0000");
  ASSERT_FALSE(r);

  r = cryptonote::parse_amount(res, "100 . 0000");
  ASSERT_FALSE(r);

  r = cryptonote::parse_amount(res, "100.00 00");
  ASSERT_FALSE(r);

  r = cryptonote::parse_amount(res, "1 00.00 00");
  ASSERT_FALSE(r);
}

#if defined(_MSC_VER) && _MSC_VER < 1900

// no initializer lists

#else

TEST(nth_sorted_item_after, nth_sorted_item_after_vanilla)
{
  std::vector<int> stuff = {0, 5, 6, 15, 20, 29};
  
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 0, 0), 0);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 0, 1), 5);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 0, 2), 6);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 0, 3), 15);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 0, 4), 20);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 0, 5), 29);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 0, 6), 0);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 0, 7), 5);
  
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 1, 0), 5);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 1, 1), 6);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 1, 2), 15);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 1, 3), 20);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 1, 4), 29);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 1, 5), 0);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 1, 6), 5);
  
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 21, 0), 29);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 22, 1), 0);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 23, 2), 5);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 24, 3), 6);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 25, 4), 15);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 26, 5), 20);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 27, 6), 29);
  
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 29, 0), 29);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 29, 1), 0);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 29, 2), 5);
  
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 50, 0), 0);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 50, 1), 5);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 50, 2), 6);
  
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, -50, 0), 0);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, -50, 1), 5);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, -50, 2), 6);
}

TEST(nth_sorted_item_after, nth_sorted_item_after_extract_sort)
{
  std::vector<std::pair<int, int> > stuff = {{5, 9}, {0, 9}, {29, 9}, {20, 9}, {15, 9}, {6, 9}};
  
  auto ex = [](std::pair<int, int> item) { return item.first; };
  
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 0, 0, ex), 0);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 0, 1, ex), 5);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 0, 2, ex), 6);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 0, 3, ex), 15);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 0, 4, ex), 20);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 0, 5, ex), 29);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 0, 6, ex), 0);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 0, 7, ex), 5);
  
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 1, 0, ex), 5);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 1, 1, ex), 6);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 1, 2, ex), 15);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 1, 3, ex), 20);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 1, 4, ex), 29);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 1, 5, ex), 0);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 1, 6, ex), 5);
  
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 21, 0, ex), 29);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 22, 1, ex), 0);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 23, 2, ex), 5);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 24, 3, ex), 6);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 25, 4, ex), 15);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 26, 5, ex), 20);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 27, 6, ex), 29);
  
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 29, 0, ex), 29);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 29, 1, ex), 0);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 29, 2, ex), 5);
  
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 50, 0, ex), 0);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 50, 1, ex), 5);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, 50, 2, ex), 6);
  
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, -50, 0, ex), 0);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, -50, 1, ex), 5);
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, -50, 2, ex), 6);
}

TEST(nth_sorted_item_after, nth_sorted_item_after_str)
{
  std::vector<std::string> stuff = {"a", "b", "c", "d", "e", "f"};
  
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("a"), 0), "a");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("a"), 1), "b");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("a"), 2), "c");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("a"), 3), "d");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("a"), 4), "e");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("a"), 5), "f");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("a"), 6), "a");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("a"), 7), "b");
  
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("b"), 0), "b");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("b"), 1), "c");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("b"), 2), "d");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("b"), 3), "e");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("b"), 4), "f");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("b"), 5), "a");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("b"), 6), "b");

  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("ea"), 0), "f");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("eb"), 1), "a");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("ec"), 2), "b");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("ed"), 3), "c");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("ee"), 4), "d");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("ef"), 5), "e");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("eg"), 6), "f");
  
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("f"), 0), "f");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("f"), 1), "a");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("f"), 2), "b");
  
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("g"), 0), "a");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("g"), 1), "b");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("g"), 2), "c");
  
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("_"), 0), "a");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("_"), 1), "b");
  ASSERT_EQ(cryptonote::nth_sorted_item_after(stuff, std::string("_"), 2), "c");
}

#endif
