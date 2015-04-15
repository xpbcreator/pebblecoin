// Copyright (c) 2015 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VOTINGRECORD_H
#define VOTINGRECORD_H

#include <memory>

namespace cryptonote {
    struct bs_delegate_info;
}

/** UI model for a voting record - essentially a container for cryptonote::bs_delegate_info .
 */
class VotingRecord
{
public:
    VotingRecord();
    VotingRecord(const cryptonote::bs_delegate_info& theInfo, bool isTop, bool isAuto, bool isUser, bool isThisWallet);
    VotingRecord(const VotingRecord& other);
    ~VotingRecord();
    
    /*VotingRecord(VotingRecord&& other) noexcept;
    VotingRecord& operator= (const VotingRecord& other);
    VotingRecord& operator= (VotingRecord&& other) noexcept;*/
    
    /** The latest stats */
    std::unique_ptr<cryptonote::bs_delegate_info> info;

    /** Whether the delegate is currently a top delegate */
    bool isTopDelegate;
    
    /** Whether the delegate is being autoselected */
    bool isAutoselected;
    
    /** Whether the delegate is being user-selected */
    bool isUserSelected;
    
    /** If the record represents this wallet */
    bool isThisWallet;
};

#endif // VOTINGRECORD_H
