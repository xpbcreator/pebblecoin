// Copyright (c) 2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "common/types.h"
#include "common/stl-util.h"
#include "crypto/hash.h"
#include "cryptonote_protocol/cryptonote_protocol_defs.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/difficulty.h"
#include "packing.h"

namespace cryptonote
{
  //-----------------------------------------------
#define CORE_RPC_STATUS_OK   "OK"
#define CORE_RPC_STATUS_BUSY   "BUSY"
#define CORE_RPC_STATUS_NORELAY "Not relayed"

  struct COMMAND_RPC_GET_HEIGHT
  {
    struct request
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      uint64_t 	 height;
      std::string status;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(height)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };

  struct COMMAND_RPC_GET_BLOCKS_FAST
  {

    struct request
    {
      std::list<crypto::hash> block_ids; //*first 10 blocks id goes sequential, next goes in pow(2,n) offset, like 2, 4, 8, 16, 32, 64 and so on, and the last one is always genesis block */

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(block_ids)
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      std::list<block_complete_entry> blocks;
      uint64_t    start_height;
      uint64_t    current_height;
      std::string status;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(blocks)
        KV_SERIALIZE(start_height)
        KV_SERIALIZE(current_height)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };
  //-----------------------------------------------
  struct COMMAND_RPC_GET_TRANSACTIONS
  {
    struct request
    {
      std::list<std::string> txs_hashes;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(txs_hashes)
      END_KV_SERIALIZE_MAP()
    };


    struct response
    {
      std::list<std::string> txs_as_hex;  //transactions blobs as hex
      std::list<std::string> missed_tx;   //not found transactions
      std::string status;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(txs_as_hex)
        KV_SERIALIZE(missed_tx)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };

  //-----------------------------------------------
  struct COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES
  {
    struct request
    {
      crypto::hash txid;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_VAL_POD_AS_BLOB(txid)
      END_KV_SERIALIZE_MAP()
    };


    struct response
    {
      std::vector<uint64_t> o_indexes;
      std::string status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(o_indexes)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };
  //-----------------------------------------------
  struct COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS
  {
    struct request
    {
      //coin_type type;
      std::list<uint64_t> amounts;
      uint64_t            outs_count;
      BEGIN_KV_SERIALIZE_MAP()
        //KV_SERIALIZE(type)
        KV_SERIALIZE(amounts)
        KV_SERIALIZE(outs_count)
      END_KV_SERIALIZE_MAP()
    };

    PACK(struct out_entry
    {
      uint64_t global_amount_index;
      crypto::public_key out_key;
    })

    struct outs_for_amount
    {
      uint64_t amount;
      std::list<out_entry> outs;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(amount)
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(outs)
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      //coin_type type;
      std::vector<outs_for_amount> outs;
      std::string status;
      BEGIN_KV_SERIALIZE_MAP()
        //KV_SERIALIZE(type)
        KV_SERIALIZE(outs)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };
  //-----------------------------------------------
  struct COMMAND_RPC_GET_KEY_IMAGE_SEQS
  {
    struct request
    {
      std::vector<crypto::key_image> images;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB_FORCE(images) // MSVC 2012 bug thinks crypto::key_image is not POD type
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      std::map<crypto::key_image, uint64_t> image_seqs;
      std::string status;
      BEGIN_KV_SERIALIZE_MAP()
        if (is_store) {
          auto image_seq_keys = tools::map_keys(this_ref.image_seqs);
          auto image_seq_values = tools::map_values(this_ref.image_seqs);
          epee::serialization::selector<is_store>::serialize_stl_container_pod_val_as_blob(image_seq_keys, stg, hparent_section, "image_seq_keys");
          epee::serialization::selector<is_store>::serialize_stl_container_pod_val_as_blob(image_seq_values, stg, hparent_section, "image_seq_values");
        }
        else
        {
          std::vector<crypto::key_image> image_seq_keys;
          std::vector<uint64_t> image_seq_values;
          epee::serialization::selector<is_store>::serialize_stl_container_pod_val_as_blob(image_seq_keys, stg, hparent_section, "image_seq_keys");
          epee::serialization::selector<is_store>::serialize_stl_container_pod_val_as_blob(image_seq_values, stg, hparent_section, "image_seq_values");
          if (image_seq_keys.size() != image_seq_values.size())
            return false;
          // const_cast so can compile for serialize calls
          auto& non_const_this = const_cast<typename std::remove_const<this_type>::type&>(this_ref);
          non_const_this.image_seqs.clear();
          for (size_t i=0; i < image_seq_keys.size(); i++)
          {
            non_const_this.image_seqs[image_seq_keys[i]] = image_seq_values[i];
          }
        }
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };
  //-----------------------------------------------
  struct COMMAND_RPC_SEND_RAW_TX
  {
    struct request
    {
      std::string tx_as_hex;

      request() {}
      explicit request(const transaction &);

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(tx_as_hex)
      END_KV_SERIALIZE_MAP()
    };


    struct response
    {
      std::string status;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };
  //-----------------------------------------------
  struct COMMAND_RPC_START_MINING
  {
    struct request
    {
      std::string miner_address;
      uint64_t    threads_count;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(miner_address)
        KV_SERIALIZE(threads_count)
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      std::string status;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };
  //-----------------------------------------------
  struct COMMAND_RPC_GET_INFO
  {
    struct request
    {

      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      std::string status;
      uint64_t height;
      uint64_t difficulty;
      uint64_t tx_count;
      uint64_t tx_pool_size;
      uint64_t alt_blocks_count;
      uint64_t outgoing_connections_count;
      uint64_t incoming_connections_count;
      uint64_t white_peerlist_size;
      uint64_t grey_peerlist_size;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(height)
        KV_SERIALIZE(difficulty)
        KV_SERIALIZE(tx_count)
        KV_SERIALIZE(tx_pool_size)
        KV_SERIALIZE(alt_blocks_count)
        KV_SERIALIZE(outgoing_connections_count)
        KV_SERIALIZE(incoming_connections_count)
        KV_SERIALIZE(white_peerlist_size)
        KV_SERIALIZE(grey_peerlist_size)
      END_KV_SERIALIZE_MAP()
    };
  };

    
  //-----------------------------------------------
  struct COMMAND_RPC_STOP_MINING
  {
    struct request
    {

      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };


    struct response
    {
      std::string status;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };


  //
  struct COMMAND_RPC_GETBLOCKCOUNT
  {
    typedef std::list<std::string> request;

    struct response
    {
      uint64_t count;
      std::string status;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(count)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };

  };

  struct COMMAND_RPC_GETBLOCKHASH
  {
    typedef std::vector<uint64_t> request;

    typedef std::string response;
  };


  struct COMMAND_RPC_GETBLOCKTEMPLATE
  {
    struct request
    {
      uint64_t reserve_size;       //max 255 bytes
      std::string wallet_address;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(reserve_size)
        KV_SERIALIZE(wallet_address)
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      uint64_t difficulty;
      uint64_t height;
      uint64_t reserved_offset;
      blobdata blocktemplate_blob;
      std::string status;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(difficulty)
        KV_SERIALIZE(height)
        KV_SERIALIZE(reserved_offset)
        KV_SERIALIZE(blocktemplate_blob)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };

  struct COMMAND_RPC_SUBMITBLOCK
  {
    typedef std::vector<std::string> request;
    
    struct response
    {
      std::string status;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };
  
  struct block_header_responce
  {
      uint8_t major_version;
      uint8_t minor_version;
      uint64_t timestamp;
      std::string prev_hash;
      uint32_t nonce;
      bool orphan_status;
      uint64_t height;
      uint64_t depth;
      std::string hash;
      difficulty_type difficulty;
      uint64_t reward;
      uint64_t already_generated_coins;
      delegate_id_t signing_delegate_id;
    
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(major_version)
        KV_SERIALIZE(minor_version)
        KV_SERIALIZE(timestamp)
        KV_SERIALIZE(prev_hash)
        KV_SERIALIZE(nonce)
        KV_SERIALIZE(orphan_status)
        KV_SERIALIZE(height)
        KV_SERIALIZE(depth)
        KV_SERIALIZE(hash)
        KV_SERIALIZE(difficulty)
        KV_SERIALIZE(reward)
        KV_SERIALIZE(already_generated_coins)
        KV_SERIALIZE(signing_delegate_id)
      END_KV_SERIALIZE_MAP()
  };
  
  struct COMMAND_RPC_GET_LAST_BLOCK_HEADER
  {
    typedef std::list<std::string> request;

    struct response
    {
      std::string status;
      block_header_responce block_header;
      
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(block_header)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };

  };
  
  struct COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH
  {
    struct request
    {
      std::string hash;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(hash)
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      std::string status;
      block_header_responce block_header;
      
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(block_header)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };

  };

  struct COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT
  {
    struct request
    {
      uint64_t height;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(height)
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      std::string status;
      block_header_responce block_header;
      
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(block_header)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };

  };

  struct COMMAND_RPC_GETBOULDERHASH
  {
    struct request
    {
      std::string block_hashing_blob_hex;
      uint32_t version;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(block_hashing_blob_hex)
        KV_SERIALIZE(version)
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      std::string block_hash_hex;
      uint32_t version;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(block_hash_hex)
        KV_SERIALIZE(version)
      END_KV_SERIALIZE_MAP()
    };
  };
  
  struct COMMAND_RPC_GET_AUTOVOTE_DELEGATES
  {
    struct request
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      std::string status;
      std::vector<delegate_id_t> autovote_delegates;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(autovote_delegates)
      END_KV_SERIALIZE_MAP()
    };
  };
  
  struct delegate_info_responce
  {
    delegate_id_t delegate_id;
    std::string public_address;
    
    uint64_t total_votes;
    
    uint64_t processed_blocks;
    uint64_t missed_blocks;
    uint64_t fees_received;
    
    uint64_t vote_rank;
    uint64_t autoselect_rank;
    
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(delegate_id)
      KV_SERIALIZE(public_address)
      KV_SERIALIZE(total_votes)
      KV_SERIALIZE(processed_blocks)
      KV_SERIALIZE(missed_blocks)
      KV_SERIALIZE(fees_received)
      KV_SERIALIZE(vote_rank)
      KV_SERIALIZE(autoselect_rank)
    END_KV_SERIALIZE_MAP()
  };
  
  struct COMMAND_RPC_GET_DELEGATE_INFOS
  {
    struct request
    {
      std::vector<delegate_id_t> delegate_ids;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(delegate_ids)
      END_KV_SERIALIZE_MAP()
    };
    
    struct response
    {
      std::string status;
      std::vector<delegate_info_responce> delegate_infos;
      
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(delegate_infos)
      END_KV_SERIALIZE_MAP()
    };
  };
}
