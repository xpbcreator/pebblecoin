// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once 

BOOST_CLASS_VERSION(cryptonote::blockchain_storage::delegate_info, 4)

namespace boost
{
  namespace serialization
  {
    template<class archive_t>
    void serialize(archive_t & ar, cryptonote::blockchain_storage::transaction_chain_entry& te, const unsigned int version)
    {
      ar & te.tx;
      ar & te.m_keeper_block_height;
      ar & te.m_blob_size;
      ar & te.m_global_output_indexes;
    }

    template<class archive_t>
    void serialize(archive_t & ar, cryptonote::blockchain_storage::currency_info& ci, const unsigned int version)
    {
      ar & ci.currency;
      ar & ci.description;
      ar & ci.decimals;
      ar & ci.total_amount_minted;
      ar & ci.remint_key_history;
    }
    
    template<class archive_t>
    void serialize(archive_t & ar, cryptonote::blockchain_storage::contract_info& ci, const unsigned int version)
    {
      ar & ci.contract;
      ar & ci.description;
      ar & ci.grading_key;
      ar & ci.fee_scale;
      ar & ci.expiry_block;
      ar & ci.default_grade;
      
      ar & ci.total_amount_minted;
      ar & ci.is_graded;
      ar & ci.grade;
    }

    template<class archive_t>
    void serialize(archive_t & ar, cryptonote::blockchain_storage::delegate_info& x, const unsigned int version)
    {
      ar & x.delegate_id;
      ar & x.public_address;
      ar & x.address_as_string;
      ar & x.total_votes;
      
      if (version < 2)
        return;
        
      ar & x.processed_blocks;
      ar & x.missed_blocks;
      
      if (version < 3)
        return;
      
      ar & x.fees_received;
      
      if (version < 4)
        return;
      
      ar & x.cached_vote_rank;
      ar & x.cached_autoselect_rank;
    }
    
    template<class archive_t>
    void serialize(archive_t & ar, cryptonote::blockchain_storage::vote_instance& x, const unsigned int version)
    {
      ar & x.voting_for_height;
      ar & x.expected_vote;
      ar & x.votes;
    }
  }
}
