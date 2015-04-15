// Copyright (c) 2014-2015 The Pebblecoin developers
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "misc_language.h"
#include "storages/portable_storage_template_helper.h"

#include "cryptonote_config.h"

#include "miner.h"

namespace cryptonote
{
  bool miner::load_config()
  {
    m_config = AUTO_VAL_INIT(m_config);
    return epee::serialization::load_t_from_json_file(m_config, m_config_folder_path + "/" + MINER_CONFIG_FILE_NAME);
  }
  bool miner::store_config()
  {
    return epee::serialization::store_t_to_json_file(m_config, m_config_folder_path + "/" + MINER_CONFIG_FILE_NAME);
  }
}
