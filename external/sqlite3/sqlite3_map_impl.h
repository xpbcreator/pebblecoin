// Copyright (c) 2015-2016 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "sqlite3_map.h"

namespace sqlite3 {
  extern "C" {
    #include "sqlite3.h"
  }
}

#include <boost/utility/value_init.hpp>

//#define SQLITE3_MAP_DEBUG

#ifdef SQLITE3_MAP_DEBUG
#include <iostream>

inline void log_cursor(void *pCursor, const std::string& msg) {
  std::cout << "-- cursor " << pCursor << " -- " << msg << std::endl;
}
inline void log_cursor(void *pCursor, const std::string& msg, const std::string& msg2) {
  std::cout << "-- cursor " << pCursor << " -- " << msg << " " << msg2 << std::endl;
}

#endif

namespace sqlite3 {
  template <class K, class V>
  sqlite3_map<K, V>::sqlite3_map(const char *filename,
                                 std::function<K(const std::string&)> load_key,
                                 std::function<std::string(const K&)> store_key,
                                 std::function<V(const std::string&)> load_value,
                                 std::function<std::string(const V&)> store_value)
      : pDb(nullptr)
      , which_db(0)
      , load_key(load_key)
      , store_key(store_key)
      , load_value(load_value)
      , store_value(store_value)
      , autocommit_per_modification(false)
      , autocommit_on_close(true)
      , db_filename(nullptr)
      , _opt_count_stmt(nullptr)
      , _opt_count_key_stmt(nullptr)
      , _opt_insert_stmt(nullptr)
  {
    reopen(filename);
  }
  
  template <class K, class V>
  sqlite3_map<K, V>::sqlite3_map(sqlite3_map&& o)
      // temporary in-mem database with the same serializers...
      : sqlite3_map(nullptr, o.load_key, o.store_key, o.load_value, o.store_value)
  {
    // swap 'em
    swap(*this, o);
  }
  
  template <class K, class V>
  sqlite3_map<K, V>& sqlite3_map<K, V>::operator=(sqlite3_map&& o)
  {
    swap(*this, o);
    return *this;
  }
  
  template <class K, class V>
  sqlite3_map<K, V>::~sqlite3_map()
  {
    close();
  }
  
  template <class K, class V>
  void sqlite3_map<K, V>::close()
  {
    if (!pDb)
      return;
    
    if (autocommit_on_close) {
      commit();
    }
    
    // clear optimized queries
    _opt_count_stmt.reset(nullptr);
    _opt_count_key_stmt.reset(nullptr);
    _opt_insert_stmt.reset(nullptr);
    
    checked(sqlite3_close(pDb), "close, maybe not all iterators destructed?");
    pDb = nullptr;
    ++which_db; // invalidate previous iterators
  }
  
  template <class K, class V>
  void sqlite3_map<K, V>::reopen(const char *filename)
  {
    close();
    checked(sqlite3_open(filename == nullptr ? ":memory:" : filename, &pDb), "open db");
    checked_exec("CREATE TABLE IF NOT EXISTS the_table (key BLOB PRIMARY KEY, value BLOB);");
    // begin transaction to disable autocommit
    checked_exec("BEGIN TRANSACTION;");
    db_filename = filename;
    
    // initialized optimized queries
    _opt_count_stmt = prepare_stmt("SELECT COUNT(*) FROM the_table;");
    _opt_count_key_stmt = prepare_stmt("SELECT COUNT(*) FROM the_table WHERE key = ?");
    _opt_insert_stmt = prepare_stmt("INSERT OR REPLACE INTO the_table (key, value) VALUES (?, ?);");
  }
  
  template <class K, class V>
  void sqlite3_map<K, V>::set_autocommit(bool per_modification, bool on_close)
  {
    autocommit_per_modification = per_modification;
    autocommit_on_close = on_close;
  }
  
  template <class K, class V>
  void sqlite3_map<K, V>::get_autocommit(bool& per_modification, bool& on_close) const
  {
    per_modification = autocommit_per_modification;
    on_close = autocommit_on_close;
  }
  
  template <class K, class V>
  void sqlite3_map<K, V>::commit()
  {
    checked_exec("COMMIT TRANSACTION;");
    checked_exec("BEGIN TRANSACTION;");
  }
  
  template <class K, class V>
  void sqlite3_map<K, V>::rollback()
  {
    checked_exec("ROLLBACK TRANSACTION;");
    checked_exec("BEGIN TRANSACTION;");
  }
  
  template <class K, class V>
  V sqlite3_map<K, V>::load(const K& key) const
  {
    auto it = find(key);
    if (it == end()) {
      return boost::value_initialized<V>();
    }
    return it->second;
  }
  
  template <class K, class V>
  void sqlite3_map<K, V>::store(const K& key, const V& value)
  {
    auto key_str = store_key(key);
    auto val_str = store_value(value);
    
#ifdef SQLITE3_MAP_DEBUG
    log_cursor(_opt_insert_stmt.get(), "binding key", key_str);
#endif
    checked(sqlite3_bind_blob(_opt_insert_stmt.get(), 1, key_str.data(), key_str.size(), SQLITE_STATIC),
            "bind store() key");
    
#ifdef SQLITE3_MAP_DEBUG
    log_cursor(_opt_insert_stmt.get(), "binding value", val_str);
#endif
    checked(sqlite3_bind_blob(_opt_insert_stmt.get(), 2, val_str.data(), val_str.size(), SQLITE_STATIC),
            "bind store() value");

    checked_exec(_opt_insert_stmt);
    
    checked(sqlite3_reset(_opt_insert_stmt.get()), "reset insert stmt");
    
    if (autocommit_per_modification) {
      commit();
    }
  }
  
  template <class K, class V>
  std::pair<typename sqlite3_map<K, V>::iterator, bool> sqlite3_map<K, V>::insert(const value_type& v)
  {
    auto it = find(v.first);
    if (it != end()) {
      return std::make_pair(std::move(it), false);
    }
    
    store(v.first, v.second);
    return std::make_pair(find(v.first), true);
  }
  
  template <class K, class V>
  size_t sqlite3_map<K, V>::count(const K& key) const
  {
    // bind key blob value
    auto key_blob = store_key(key);
    checked(sqlite3_bind_blob(_opt_count_key_stmt.get(), 1, key_blob.data(), key_blob.size(), SQLITE_STATIC),
            "bind find() key");
    
    // execute it
    if (sqlite3_step(_opt_count_key_stmt.get()) != SQLITE_ROW) {
      throw std::runtime_error("select count(*) didn't return data");
    }
    
    // return result
    size_t res = ((size_t)sqlite3_column_int64(_opt_count_key_stmt.get(), 0)) > 0;
    // reset query
    checked(sqlite3_reset(_opt_count_key_stmt.get()));
    return res;
  }
  
  template <class K, class V>
  bool sqlite3_map<K, V>::contains(const K& key) const
  {
    return count(key) > 0;
  }
  
  template <class K, class V>
  bool sqlite3_map<K, V>::empty() const
  {
    return size() == 0;
  }
  
  template <class K, class V>
  size_t sqlite3_map<K, V>::size() const
  {
#ifdef SQLITE3_MAP_DEBUG
    log_cursor(_opt_count_stmt.get(), "sqlite3_step()...");
#endif
    if (sqlite3_step(_opt_count_stmt.get()) != SQLITE_ROW) {
      throw std::runtime_error("select count(*) didn't return data");
    }
    
    size_t res = (size_t)sqlite3_column_int64(_opt_count_stmt.get(), 0);
    checked(sqlite3_reset(_opt_count_stmt.get()), "reset count table stmt");
    return res;
  }
  
  template <class K, class V>
  void sqlite3_map<K, V>::clear()
  {
    checked_exec("DROP TABLE the_table");
    checked_exec("CREATE TABLE IF NOT EXISTS the_table (key BLOB PRIMARY KEY, value BLOB);");
    
    if (autocommit_per_modification) {
      commit();
    }
  }

  template <class K, class V>
  size_t sqlite3_map<K, V>::erase(const K& key)
  {
    auto it = find(key);
    if (it == end()) {
      return 0;
    }
    
    erase(it);
    
    return 1;
  }
  
  template <class K, class V>
  typename sqlite3_map<K, V>::iterator sqlite3_map<K, V>::erase(iterator pos)
  {
    if (pos == end()) {
      return pos;
    }
    
    auto res = pos.erase();
    
    if (autocommit_per_modification) {
      commit();
    }
    
    return res;
  }
  /// ------------------------------------------------------

  template <class K, class V>
  void sqlite3_map<K, V>::checked(int rc, const std::string& op) const {
    if (!(rc == SQLITE_OK || rc == SQLITE_DONE)) {
      throw std::runtime_error("sqlite3 error " + std::to_string(rc) + (op.empty() ? "" : " when " + op)
                               + ": " + std::string(sqlite3_errmsg(pDb)));
    }
  }
  
  template <class K, class V>
  void sqlite3_map<K, V>::checked_exec(stmt_ptr& stmt) const {
    // ignore all values returned, execute statement until it's done
    int rc;
#ifdef SQLITE3_MAP_DEBUG
    log_cursor(stmt.get(), "sqlite3_step()...");
#endif
    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
#ifdef SQLITE3_MAP_DEBUG
      log_cursor(stmt.get(), "sqlite3_step()...");
#endif
    };
    checked(rc, "step exec");
  }

  template <class K, class V>
  void sqlite3_map<K, V>::checked_exec(const char *query) const {
    auto stmt = prepare_stmt(query);
    return checked_exec(stmt);
  }
  
  template <class K, class V>
  typename sqlite3_map<K, V>::stmt_ptr sqlite3_map<K, V>::prepare_stmt(const char *query) const
  {
    auto destroy = [this](sqlite3_stmt *pStmt) {
      this->checked(sqlite3_finalize(pStmt));
    };
    
    if (query == nullptr) {
#ifdef SQLITE3_MAP_DEBUG
      log_cursor(nullptr, "prepared nullptr statement");
#endif
      return stmt_ptr(nullptr, destroy);
    }
    
    sqlite3_stmt *pStmt;
    checked(sqlite3_prepare_v2(pDb, query, strlen(query), &pStmt, nullptr));
#ifdef SQLITE3_MAP_DEBUG
    log_cursor(pStmt, "prepared statement", query);
#endif
    return stmt_ptr(pStmt, destroy);
  }

  template <class K, class V>
  typename sqlite3_map<K, V>::stmt_ptr sqlite3_map<K, V>::prepare_find_key_blob(const std::string& key_blob) const
  {
    auto pStmt = prepare_stmt("SELECT key, value FROM the_table WHERE key = ?;");
#ifdef SQLITE3_MAP_DEBUG
    log_cursor(pStmt.get(), "binding key", key_blob);
#endif
    checked(sqlite3_bind_blob(pStmt.get(), 1, key_blob.data(), key_blob.size(), SQLITE_TRANSIENT),
            "bind find() key");
    return pStmt;
  }
  /// ------------------------------------------------------
  /// iterator
  
  template <class K, class V>
  sqlite3_map<K, V>::iterator::iterator(const sqlite3_map<K, V>& parent, stmt_ptr&& the_pStmt)
      : parent(parent)
      , which_db(parent.which_db)
      , pStmt(std::move(the_pStmt))
      , is_past_the_end(false)
      , cached_key_valid(false)
      , cached_value_valid(false)
      , cached_pair_valid(false)
  {
#ifdef SQLITE3_MAP_DEBUG
    log_cursor(pStmt.get(), "construct...");
#endif
    if (pStmt.get() == nullptr) {
#ifdef SQLITE3_MAP_DEBUG
      log_cursor(pStmt.get(), "construct null/past-the-end with which_db", std::to_string(which_db));
#endif
      is_past_the_end = true;
      return;
    }
    
#ifdef SQLITE3_MAP_DEBUG
    log_cursor(pStmt.get(), "construct w/ query, which_db", std::to_string(which_db));
#endif
    // start executing query
    ++(*this);
  }
  
  template <class K, class V>
  sqlite3_map<K, V>::iterator::iterator(const iterator& o)
      // same parent & which_db
      : parent(o.parent)
      , which_db(o.which_db)
      // new statement
      , pStmt(nullptr)
      , is_past_the_end(false)
      , cached_key_valid(false)
      , cached_value_valid(false)
      , cached_pair_valid(false)
  {
#ifdef SQLITE3_MAP_DEBUG
    log_cursor(o.pStmt.get(), "copy construct *from* this ptr, with which_db", std::to_string(which_db));
#endif
    
    if (!o.valid()) {
      // remain invalid
#ifdef SQLITE3_MAP_DEBUG
      log_cursor(pStmt.get(), "copy construct, remaining invalid");
#endif
      is_past_the_end = true;
      return;
    }
    
    // seek to where the other is
    pStmt = parent.prepare_find_key_blob(o.getKey());
    // start executing query
#ifdef SQLITE3_MAP_DEBUG
    log_cursor(pStmt.get(), "copy constructed *to* this ptr, seeking to other");
#endif
    ++(*this);
  }
  
  template <class K, class V>
  sqlite3_map<K, V>::iterator::iterator(iterator&& o)
      // start w/ placeholder empty statement
      : iterator(o.parent, o.parent.prepare_stmt(nullptr))
  {
#ifdef SQLITE3_MAP_DEBUG
    log_cursor(pStmt.get(), "move construct start");
#endif
    // steal the others' values
    swap(*this, o);
#ifdef SQLITE3_MAP_DEBUG
    log_cursor(pStmt.get(), "move construct done");
#endif
  }
  
  template <class K, class V>
  typename sqlite3_map<K, V>::iterator::iterator& sqlite3_map<K, V>::iterator::operator=(iterator o)
  {
#ifdef SQLITE3_MAP_DEBUG
    log_cursor(pStmt.get(), "assign start");
#endif
    swap(*this, o);
#ifdef SQLITE3_MAP_DEBUG
    log_cursor(pStmt.get(), "assign done");
#endif
    return *this;
  }
  
  template <class K, class V>
  sqlite3_map<K, V>::iterator::~iterator()
  {
#ifdef SQLITE3_MAP_DEBUG
    log_cursor(pStmt.get(), "destruct");
#endif
  }
  
  // ------------------------------------------------------
  
  template <class K, class V>
  typename sqlite3_map<K, V>::iterator& sqlite3_map<K, V>::iterator::operator++() {
#ifdef SQLITE3_MAP_DEBUG
    log_cursor(pStmt.get(), "iterator++");
#endif
    
    if (!valid()) {
      throw std::runtime_error("incrementing invalid iterator");
    }
    
#ifdef SQLITE3_MAP_DEBUG
    log_cursor(pStmt.get(), "sqlite3_step()...");
#endif
    int rc = sqlite3_step(pStmt.get());
    
    if (rc == SQLITE_DONE) {
#ifdef SQLITE3_MAP_DEBUG
      log_cursor(pStmt.get(), "step rc done, past the end!");
#endif
      is_past_the_end = true;
    } else if (rc == SQLITE_ROW) {
#ifdef SQLITE3_MAP_DEBUG
      log_cursor(pStmt.get(), "step rc row!");
#endif
      // wait before retrieving data
    } else {
      throw std::runtime_error("statement step error");
    }
    
    invalidateCache();
    return *this;
  }
  
  template <class K, class V>
  typename sqlite3_map<K, V>::value_type sqlite3_map<K, V>::iterator::operator++(int) {
    auto res = *(*this);
    ++(*this);
    return res;
  }
  
  template <class K, class V>
  const typename sqlite3_map<K, V>::value_type& sqlite3_map<K, V>::iterator::operator*() const {
    return getPair();
  }
  
  template <class K, class V>
  const typename sqlite3_map<K, V>::value_type* sqlite3_map<K, V>::iterator::operator->() const {
    return &getPair();
  }
  
  // ------------------------------------------------------
  
  template <class K, class V>
  bool sqlite3_map<K, V>::iterator::operator==(const iterator& i2) const {
#ifdef SQLITE3_MAP_DEBUG
    log_cursor(pStmt.get(), "eq LHS");
    log_cursor(i2.pStmt.get(), "eq RHS");
#endif
    
    if (which_db != i2.which_db) { // different database
      return false;
    }
    
    bool thisValid = valid();
    bool thatValid = i2.valid();
    
    // any invalid iterators are equal to each other
    if (!thisValid && !thatValid) {
      return true;
    }
    // if one is invalid but not the other, then they are inequal
    if (thisValid != thatValid) {
      return false;
    }
    
    // both valid - compare keys
    return getKey() == i2.getKey();
  }
  
  template <class K, class V>
  bool sqlite3_map<K, V>::iterator::operator!=(const iterator& i2) const {
    return !this->operator==(i2);
  }
    
  /// assumes the iterator is valid
  template <class K, class V>
  std::string& sqlite3_map<K, V>::iterator::getKey() const {
    if (!cached_key_valid) {
      if (!valid()) {
        throw std::runtime_error("loading key for invalid iterator");
      }
      
      cached_key = std::string((const char *)sqlite3_column_blob(pStmt.get(), 0),
                               sqlite3_column_bytes(pStmt.get(), 0));
      cached_key_valid = true;
      
#ifdef SQLITE3_MAP_DEBUG
      log_cursor(pStmt.get(), "loaded key", cached_key);
#endif
    }
    
    return cached_key;
  }
  
  template <class K, class V>
  std::string& sqlite3_map<K, V>::iterator::getValue() const {
    if (!cached_value_valid) {
      if (!valid()) {
        throw std::runtime_error("loading value for invalid iterator");
      }
      
      cached_value = std::string((const char *)sqlite3_column_blob(pStmt.get(), 1),
                                 sqlite3_column_bytes(pStmt.get(), 1));
      cached_value_valid = true;
      
#ifdef SQLITE3_MAP_DEBUG
      log_cursor(pStmt.get(), "loaded value", cached_value);
#endif
    }
    
    return cached_value;
  }
  
  template <class K, class V>
  typename sqlite3_map<K, V>::value_type& sqlite3_map<K, V>::iterator::getPair() const {
    if (!cached_pair_valid) {
      cached_pair = value_type(parent.load_key(getKey()), parent.load_value(getValue()));
      cached_pair_valid = true;
    }
    return cached_pair;
  }
  
  template <class K, class V>
  void sqlite3_map<K, V>::iterator::invalidateCache() const {
    cached_key_valid = false;
    cached_value_valid = false;
    cached_pair_valid = false;
  }
  
  // ------------------------------------------------------
  
  template <class K, class V>
  typename sqlite3_map<K, V>::iterator sqlite3_map<K, V>::iterator::erase()
  {
    if (!valid()) {
      throw std::runtime_error("deleting invalid iterator");
    }
    
    auto delStmt = parent.prepare_stmt("DELETE FROM the_table WHERE key = ?;");
#ifdef SQLITE3_MAP_DEBUG
    log_cursor(pStmt.get(), "binding key", getKey());
#endif
    parent.checked(sqlite3_bind_blob(delStmt.get(), 1, getKey().data(), getKey().size(), SQLITE_STATIC),
                   "bind erase() key");
    parent.checked_exec(delStmt);
    
    invalidateCache();
    is_past_the_end = true; // done with this
    
    // return invalid iterator (i.e. copy of this)
    return *this;
  }
  
  template <class K, class V>
  bool sqlite3_map<K, V>::iterator::valid() const
  {
    if (which_db != parent.which_db) { // different database
#ifdef SQLITE3_MAP_DEBUG
      log_cursor(pStmt.get(), "valid(): invalid which_db from parent", std::to_string(which_db));
#endif
      return false;
    }
    if (is_past_the_end) {
#ifdef SQLITE3_MAP_DEBUG
      log_cursor(pStmt.get(), "valid(): is past the end");
#endif
      return false;
    }
    if (pStmt.get() == nullptr) {
#ifdef SQLITE3_MAP_DEBUG
      log_cursor(pStmt.get(), "valid(): is nullptr");
#endif
      return false;
    }
    
#ifdef SQLITE3_MAP_DEBUG
    log_cursor(pStmt.get(), "valid(): is valid");
#endif
    return true;
  }
  
  /// ------------------------------------------------------
  /// funcs using iterators
  
  template <class K, class V>
  typename sqlite3_map<K, V>::iterator sqlite3_map<K, V>::cbegin() const {
    return iterator(*this, prepare_stmt("SELECT key, value FROM the_table;"));
  }
  
  template <class K, class V>
  typename sqlite3_map<K, V>::iterator sqlite3_map<K, V>::cend() const {
    return iterator(*this, prepare_stmt(nullptr));
  }
  
  template <class K, class V>
  typename sqlite3_map<K, V>::iterator sqlite3_map<K, V>::find(const K& key) const {
    // get it to >= key
#ifdef SQLITE3_MAP_DEBUG
    log_cursor((void *)0x666, "find() making iterator w/ prepare_find_key_blob...");
#endif
    auto it = iterator(*this, prepare_find_key_blob(store_key(key)));
#ifdef SQLITE3_MAP_DEBUG
    log_cursor(it.pStmt.get(), "find() made iterator...");
#endif
    
    return it;
  }
}
