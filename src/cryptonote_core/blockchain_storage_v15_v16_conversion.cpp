// Copyright (c) 2015-2016 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blockchain_storage.h"

#include "common/stl-util.h"

#include "sqlite3/sqlite3_map_impl.h"

#include "include_base_utils.h"
#include "warnings.h"


using namespace std;
using namespace epee;
using namespace cryptonote;


DISABLE_VS_WARNINGS(4267)


//------------------------------------------------------------------
//------------------------------------------------------------------
//------------------------------------------------------------------
uint64_t blockchain_storage::v15_ram_converter::get_check_count() const
{
  return m_blocks.size() + m_blocks_index.size() + m_transactions.size() + bs.m_spent_keys.size() + m_alternative_chains.size() + bs.m_outputs.size() + m_invalid_blocks.size() + bs.m_current_block_cumul_sz_limit + bs.m_currencies.size() + bs.m_used_currency_descriptions.size() + bs.m_contracts.size() + bs.m_delegates.size() + bs.m_vote_histories.size();
}
//------------------------------------------------------------------
void blockchain_storage::v15_ram_converter::print_sizes() const
{
  LOG_PRINT_L0("Old blockchain storage:" << ENDL <<
               "m_blocks: " << m_blocks.size() << ENDL  <<
               "m_blocks_index: " << m_blocks_index.size() << ENDL  <<
               "m_transactions: " << m_transactions.size() << ENDL  <<
               "m_spent_keys: " << bs.m_spent_keys.size() << ENDL  <<
               "m_alternative_chains: " << m_alternative_chains.size() << ENDL  <<
               "m_outputs: " << bs.m_outputs.size() << ENDL  <<
               "m_invalid_blocks: " << m_invalid_blocks.size() << ENDL  <<
               "m_current_block_cumul_sz_limit: " << bs.m_current_block_cumul_sz_limit << ENDL <<
               "m_currencies: " << bs.m_currencies.size() << ENDL <<
               "m_contracts: " << bs.m_contracts.size() << ENDL <<
               "m_used_currency_descriptions: " << bs.m_used_currency_descriptions.size() << ENDL <<
               "m_delegates:" << bs.m_delegates.size() << ENDL <<
               "m_vote_histories:" << bs.m_vote_histories.size() << ENDL);
}
//------------------------------------------------------------------
static void log_pct(size_t i, size_t total) {
  if ((i + 1) % (total / 20) == 0) {
    LOG_PRINT_YELLOW("    " << ((i + 1) / (total / 20)) * 5 << "% done...", LOG_LEVEL_0);
  }
}
//------------------------------------------------------------------
bool blockchain_storage::v15_ram_converter::process_conversion()
{
  LOG_PRINT_YELLOW("Processing blockchain v15 conversion, this may take a few minutes......", LOG_LEVEL_0);

  // sanity checks
  CHECK_AND_ASSERT_MES(requires_conversion, false, "Does not require conversion");
  CHECK_AND_ASSERT_MES(bs.m_pblockchain_entries.get() != nullptr, false, "blockchain entry store is not initialized");
  CHECK_AND_ASSERT_MES(bs.m_pblockchain_entries->empty(), false, "blockchain entry store is not empty");
  CHECK_AND_ASSERT_MES(bs.m_blocks_by_hash.empty(), false, "block store is not empty");
  CHECK_AND_ASSERT_MES(bs.m_blocks_index.empty(), false, "block index is not empty");
  CHECK_AND_ASSERT_MES(bs.m_transactions.empty(), false, "transaction store is not empty");
  CHECK_AND_ASSERT_MES(bs.m_alternative_chain_entries.empty(), false, "alternative blockchain entry store is not empty");
  CHECK_AND_ASSERT_MES(bs.m_invalid_block_entries.empty(), false, "invalid blockchain entry store is not empty");
  
  // begin
  auto bei_to_bent = [](const block_extended_info& bei) {
    blockchain_entry bent;
    
    bent.hash = get_block_hash(bei.bl);
    bent.height = bei.height;
    bent.timestamp = bei.bl.timestamp;
    bent.block_cumulative_size = bei.block_cumulative_size;
    bent.cumulative_difficulty = bei.cumulative_difficulty;
    bent.already_generated_coins = bei.already_generated_coins;
    
    return bent;
  };
  
  LOG_PRINT_YELLOW("Converting " << m_blocks.size() << " main blocks and blockchain entries...", LOG_LEVEL_0);
  
  FOR_ENUM(i, bei, m_blocks) {
    auto bent = bei_to_bent(bei);
    bs.m_pblockchain_entries->push_back(bent);
    bs.m_blocks_by_hash.store(bent.hash, bei.bl);
    
    log_pct(i, m_blocks.size());
  } END_FOR_ENUM()
  
  
  LOG_PRINT_YELLOW("Converting " << m_blocks_index.size() << " block indices...", LOG_LEVEL_0);
  
  FOR_ENUM_PAIR(i, hash, index, m_blocks_index) {
    bs.m_blocks_index.store(hash, index);
    
    log_pct(i, m_blocks.size());
  } END_FOR_ENUM_PAIR()
  
  
  LOG_PRINT_YELLOW("Converting " << m_transactions.size() << " transactions...", LOG_LEVEL_0);
  
  FOR_ENUM_PAIR(i, hash, tx, m_transactions) {
    bs.m_transactions.store(hash, tx);

    log_pct(i, m_transactions.size());
  } END_FOR_ENUM_PAIR()

  
  LOG_PRINT_YELLOW("Converting " << m_alternative_chains.size() << " alternative chain entries and blocks...", LOG_LEVEL_0);
  
  FOR_ENUM_PAIR(i, hash, bei, m_alternative_chains) {
    bs.m_alternative_chain_entries.store(hash, bei_to_bent(bei));
    bs.m_blocks_by_hash.store(hash, bei.bl);
    
    log_pct(i, m_alternative_chains.size());
  } END_FOR_ENUM_PAIR()

  
  LOG_PRINT_YELLOW("Converting " << m_invalid_blocks.size() << " invalid chain entries and blocks...", LOG_LEVEL_0);

  FOR_ENUM_PAIR(i, hash, bei, m_invalid_blocks) {
    bs.m_invalid_block_entries.store(hash, bei_to_bent(bei));
    bs.m_blocks_by_hash.store(hash, bei.bl);
    
    log_pct(i, m_invalid_blocks.size());
  } END_FOR_ENUM_PAIR()
  
  
  LOG_PRINT_YELLOW("Clearing now-unused RAM...", LOG_LEVEL_0);
  
  m_blocks.clear();
  m_blocks_index.clear();
  m_transactions.clear();
  m_alternative_chains.clear();
  m_invalid_blocks.clear();
  requires_conversion = false;
  
  
  LOG_PRINT_YELLOW("Conversion successful", LOG_LEVEL_0);
  
  return true;
}
//------------------------------------------------------------------
//------------------------------------------------------------------
//------------------------------------------------------------------
