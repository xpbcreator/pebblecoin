// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <utility>

#include "votingrecord.h"

#include "cryptonote_core/blockchain_storage.h"

VotingRecord::VotingRecord()
    : info(new cryptonote::bs_delegate_info)
    , isTopDelegate(false)
    , isAutoselected(false)
    , isUserSelected(false)
    , isThisWallet(false)
{
}

VotingRecord::VotingRecord(const cryptonote::bs_delegate_info& theInfo,
                           bool isTop, bool isAuto, bool isUser, bool isThisWallet)
    : info(new cryptonote::bs_delegate_info(theInfo))
    , isTopDelegate(isTop)
    , isAutoselected(isAuto)
    , isUserSelected(isUser)
    , isThisWallet(isThisWallet)
{
}

VotingRecord::VotingRecord(const VotingRecord& other)
    : info(new cryptonote::bs_delegate_info(*other.info))
    , isTopDelegate(other.isTopDelegate)
    , isAutoselected(other.isAutoselected)
    , isUserSelected(other.isUserSelected)
    , isThisWallet(other.isThisWallet)
{
}

VotingRecord::~VotingRecord() { }

/*VotingRecord::VotingRecord(VotingRecord&& other) noexcept
    : info(std::move(other.info))
    , isTopDelegate(other.isTopDelegate)
    , isAutoselected(other.isAutoselected)
    , isUserSelected(other.isUserSelected)
{
}

VotingRecord::~VotingRecord()
{
    info.reset(nullptr);
}

VotingRecord& VotingRecord::operator= (const VotingRecord& other)
{
    VotingRecord tmp(other);
    *this = std::move(tmp);
    return *this;
}

VotingRecord& VotingRecord::operator= (VotingRecord&& other) noexcept
{
    std::swap(info, other.info);
    std::swap(isTopDelegate, other.isTopDelegate);
    std::swap(isAutoselected, other.isAutoselected);
    std::swap(isUserSelected, other.isUserSelected);
    return *this;
}*/
