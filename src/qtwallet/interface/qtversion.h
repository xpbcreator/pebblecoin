// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PEBBLECOIN_BCVERSION_H
#define PEBBLECOIN_BCVERSION_H

#include <string>

#include "version.h"

//
// client versioning
//

static const int CLIENT_VERSION =
                           1000000 * PROJECT_VERSION_MAJOR
                         +   10000 * PROJECT_VERSION_MINOR
                         +     100 * PROJECT_VERSION_REVISION
                         +       1 * PROJECT_VERSION_BUILD;

extern const std::string CLIENT_NAME;
extern const std::string CLIENT_BUILD;
extern const std::string CLIENT_DATE;

#endif
