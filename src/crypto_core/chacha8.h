// Copyright (c) 2014-2015 The Cryptonote developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stdint.h>
#include <stddef.h>

void chacha8(const void* data, size_t length, const uint8_t* key, const uint8_t* iv, char* cipher);
