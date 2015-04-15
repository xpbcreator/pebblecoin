// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#define VANILLA_TRANSACTION_VERSION                     1
#define DPOS_TRANSACTION_VERSION                        2
#define CURRENCY_TRANSACTION_VERSION                    3
#define CONTRACT_TRANSACTION_VERSION                    4
#define CURRENT_TRANSACTION_VERSION                     2

#define CURRENT_BLOCK_MAJOR_VERSION                     2
#define CURRENT_BLOCK_MINOR_VERSION                     0
#define DPOS_BLOCK_MAJOR_VERSION                        2
#define DPOS_BLOCK_MINOR_VERSION                        0
#define POW_BLOCK_MAJOR_VERSION                         1
#define POW_BLOCK_MINOR_VERSION                         0

#define CURRENCY_XPB      77     // the currency for XPB
#define CURRENCY_INVALID  252    // an invalid/unsupplied currency
#define BACKED_BY_INVALID 253    // an invalid/unsupplied backed by currency
#define CURRENCY_N_A      254    // currency doesn't apply
#define BACKED_BY_N_A     255    // backed_by currency doesn't apply
