// Copyright (c) 2015-2016 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

namespace sqlite3
{
  struct sqlite3;
  struct sqlite3_stmt;
}

#include <string>
#include <cstdint>
#include <memory>
#include <functional>


namespace sqlite3 {
  /** A *very basic* key/value store backed by sqlite3. Types can be anything that serializes
   * to a std::string, with the serialization methods passed into the constructor. Some built-in
   * ones are provided in sqlite3_map_ser.h
   *  
   *  NOTES:
   *  - Not thread-safe!
   *  - Only provides InputIterator
   *  - iterators give an unordered view of the map
   *  - all iterators must be destructed before reopen()ing or destructing the map, or an exception
   *    will be thrown
   *  - find() returns iterator that only points to that element, not any others
   */
  template <class K, class V>
  class sqlite3_map
  {
  public:
    typedef K key_type;
    typedef V mapped_type;
    typedef std::pair<K, V> value_type;
    
    typedef std::unique_ptr<sqlite3_stmt, std::function<void(sqlite3_stmt*)>> stmt_ptr;
    
  public:
    /// bare-minimum iterator to be able to loop through all entries
    /// reference gotten from iterator are only valid so long as the iterator
    /// does not move. assigning to the iterator does not change any values
    /// in the map
    struct iterator
    {
      friend class sqlite3_map<K, V>;
      
    public:
      // must init with a prepared statement, which this will take ownership of
      iterator(const sqlite3_map<K, V>& parent, stmt_ptr&& pStmt);
      
      iterator(const iterator& o);
      iterator(iterator&& o);
      iterator& operator=(iterator o);
      
      ~iterator();
      
    public:
      iterator& operator++();
      value_type operator++(int);
      const value_type& operator*() const;
      const value_type* operator->() const;
      bool operator==(const iterator& i2) const;
      bool operator!=(const iterator& i2) const;
      
    private:
      std::string& getKey() const;
      std::string& getValue() const;
      value_type& getPair() const;
      void invalidateCache() const;
      
      /// erase, returning new iterator
      iterator erase();
      /// return whether this iterator is valid
      bool valid() const;
      
    private:
      const sqlite3_map<K, V>& parent;
      size_t which_db; // to track reopen()s
      stmt_ptr pStmt;
      bool is_past_the_end;
      mutable bool cached_key_valid;
      mutable bool cached_value_valid;
      mutable bool cached_pair_valid;
      mutable std::string cached_key;
      mutable std::string cached_value;
      mutable value_type cached_pair;
      
    public:
      // assumes swapping two iterators of same map
      friend void swap(iterator& first, iterator& second)
      {
        using std::swap;
        
        swap(first.which_db, second.which_db);
        swap(first.pStmt, second.pStmt);
        swap(first.is_past_the_end, second.is_past_the_end);
        swap(first.cached_key_valid, second.cached_key_valid);
        swap(first.cached_value_valid, second.cached_value_valid);
        swap(first.cached_pair_valid, second.cached_pair_valid);
        swap(first.cached_key, second.cached_key);
        swap(first.cached_value, second.cached_value);
        swap(first.cached_pair, second.cached_pair);
      }
    };
    
    // only const InputIterator anyway
    typedef iterator const_iterator;
  
  public:
    /// create an sqlite3 map. if filename is nullptr, an in-memory database is created
    sqlite3_map(const char *filename,
                std::function<K(const std::string&)> load_key,
                std::function<std::string(const K&)> store_key,
                std::function<V(const std::string&)> load_value,
                std::function<std::string(const V&)> store_value);
    sqlite3_map(sqlite3_map&& o);
    sqlite3_map& operator=(sqlite3_map&& o);
    ~sqlite3_map();
    
    /// close the database. all iterators invalidated, all operations besides reopen() are undefined
    void close();
    
    /// close the file & open with a new file. all iterators are invalidated, but can
    /// still be compared to other iterators (but will compare unequal to anything from an
    /// old database). no data is transferred. if filename is nullptr, an in-memory database is created
    void reopen(const char *filename);
    
    /// set the autocommit behavior: whether to commit after every modification, and whether to
    /// commit when the database is closed. Defaults are false, true.
    void set_autocommit(bool per_modification, bool on_close);
    /// get the autocommit behavior
    void get_autocommit(bool& per_modification, bool& on_close) const;
    
    /// commit whatever changes. only necessary if autocommit is disabled
    void commit();
    /// rollback to previous commit. invalidates all iterators
    void rollback();
    
    /// load the value at a given key. if the key doesn't exist, returns a value-initialized value
    V load(const K& key) const;
    /// store a value to a key
    void store(const K& key, const V& value);
    /// insert if key doesn't exist. return pair(iterator to element, whether insertion took place)
    std::pair<iterator, bool> insert(const value_type& value);
    /// count of a key, either 0 or 1
    size_t count(const K& key) const;
    /// return whether the map contains a value for the given key
    bool contains(const K& key) const;
    
    /// returns whether the map is empty
    bool empty() const;
    /// returns the size of the map
    size_t size() const;
    
    /// clears the map
    void clear();
    /// erases the element with the given key
    size_t erase(const K& key);
    /// erase the element at the given iterator. returns an iterator to the next element
    iterator erase(iterator pos);
    
    /// valid iterator to start of map
    const_iterator cbegin() const;
    const_iterator begin() const { return cbegin(); }
    iterator begin() { return cbegin(); }
    
    /// returns an invalid iterator
    const_iterator cend() const;
    const_iterator end() const { return cend(); }
    iterator end() { return cend(); }
    
    /// iterator to element of the given key. either end(), or an iterator with just this
    /// element (i.e. ++ of the it gives end())
    const_iterator find(const K& key) const;
    iterator find(const K& key) { return const_cast<const sqlite3_map<K, V>&>(*this).find(key); }

  private:
    void checked(int rc, const std::string& op="") const;
    void checked_exec(stmt_ptr& stmt) const; // execute statement fully
    void checked_exec(const char *query) const; // execute query fully
    
    stmt_ptr prepare_stmt(const char *query) const;
    stmt_ptr prepare_find_key_blob(const std::string& key_blob) const;
    
  private:
    sqlite3 *pDb;
    size_t which_db;
    std::function<K(const std::string&)> load_key;
    std::function<std::string(const K&)> store_key;
    std::function<V(const std::string&)> load_value;
    std::function<std::string(const V&)> store_value;
    
    bool autocommit_per_modification;
    bool autocommit_on_close;
    
    const char *db_filename;
    
    // optimization
    stmt_ptr _opt_count_stmt;
    stmt_ptr _opt_count_key_stmt;
    stmt_ptr _opt_insert_stmt;
    
  public:
    friend void swap(sqlite3_map& first, sqlite3_map& second)
    {
      using std::swap;
      
      swap(first.pDb, second.pDb);
      swap(first.which_db, second.which_db);
      swap(first.load_key, second.load_key);
      swap(first.store_key, second.store_key);
      swap(first.load_value, second.load_value);
      swap(first.store_value, second.store_value);

      swap(first.autocommit_per_modification, second.autocommit_per_modification);
      swap(first.autocommit_on_close, second.autocommit_on_close);

      swap(first.db_filename, second.db_filename);

      swap(first._opt_count_stmt, second._opt_count_stmt);
      swap(first._opt_count_key_stmt, second._opt_count_key_stmt);
      swap(first._opt_insert_stmt, second._opt_insert_stmt);
    }
  };
}
