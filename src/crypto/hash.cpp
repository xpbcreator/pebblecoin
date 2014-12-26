// Copyright (c) 2014 The Pebblecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "hash.h"
#include "hash_options.h"

#include "common/command_line.h"
#include "cryptonote_core/miner.h"

#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>
#define BOOST_THREAD_DONT_PROVIDE_FUTURE
#include <boost/thread/future.hpp>
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>

#include <thread>

namespace
{
  bool f_threads_inited = false;
  
  boost::asio::io_service io_service;
  boost::thread_group thread_pool;
  boost::asio::io_service::work *pwork;
  uint32_t states_per_thread = 1;
  
  void check_init_threads(const boost::program_options::variables_map& vm)
  {
    if (f_threads_inited)
      return;
    
    pwork = new boost::asio::io_service::work(io_service);
    
    uint32_t nthreads;
    if(command_line::has_arg(vm, hashing_opt::arg_worker_threads))
    {
      nthreads = command_line::get_arg(vm, hashing_opt::arg_worker_threads);
    }
    else
    {
      nthreads = std::thread::hardware_concurrency();
    }
    
    if(command_line::has_arg(vm, hashing_opt::arg_states_per_thread))
    {
      states_per_thread = command_line::get_arg(vm, hashing_opt::arg_states_per_thread);
    }
    
    for (uint32_t i=0; i < nthreads; i++)
    {
      thread_pool.create_thread(boost::bind(&boost::asio::io_service::run, &io_service));
    }
    
    LOG_PRINT_L0("Started " << nthreads << " boulderhash worker threads");
    f_threads_inited = true;
  }
  
  void check_stop_threads()
  {
    if (!f_threads_inited)
      return;
    
    io_service.stop();
    delete pwork;
    LOG_PRINT_L0("Joining boulderhash worker threads...");
    thread_pool.join_all();
    LOG_PRINT_L0("Joined boulderhash worker threads");
    
    f_threads_inited = false;
  }
  
  int do_fill_states(uint64_t **state, int start_i, int end_i)
  {
    end_i = std::min(end_i, BOULDERHASH_STATES);
    
    for (int i=start_i; i < end_i; i++)
    {
      crypto::pc_boulderhash_fill_state(state[i]);
    }
    
    return 1;
  }
  
  boost::shared_future<int> post_fill_states_job(uint64_t **state, int start_i, int end_i)
  {
    if (!f_threads_inited)
      throw std::runtime_error("boulderhash threadpool not initialized");
    
    // shared_ptr work-around for packaged_task not being CopyConstructible
    auto ptask = boost::make_shared<boost::packaged_task<int> >(boost::bind(do_fill_states, state, start_i, end_i));
    boost::shared_future<int> result = ptask->get_future();
    io_service.post(boost::bind(&boost::packaged_task<int>::operator(), ptask));
    return boost::move(result);
  }
  
  void fill_all_states(uint64_t **state)
  {
    if (!f_threads_inited)
    {
      for (int i=0; i < BOULDERHASH_STATES; i++) {
        crypto::pc_boulderhash_fill_state(state[i]);
      }
      
      return;
    }
    
    std::list<boost::shared_future<int> > futures;
    
    for (int i=0; i < BOULDERHASH_STATES; i += states_per_thread) {
      futures.push_back(post_fill_states_job(state, i, i+states_per_thread));
    }
    
    BOOST_FOREACH(auto& result, futures)
    {
      result.wait();
      if (!result.get())
        throw std::runtime_error("future didn't complete");
    }
  }
}

namespace crypto
{
  uint64_t **g_boulderhash_state = NULL;
  
  uint64_t **pc_malloc_state()
  {
    if (!hashing_opt::boulderhash_enabled())
    {
      LOG_PRINT_L0("Boulderhash disabled, not malloc'ing boulderhash state...");
      return NULL;
    }
    
    LOG_PRINT_L0("Malloc'ing boulderhash state...");
    uint64_t **state = (uint64_t **)malloc(sizeof(uint64_t *)*BOULDERHASH_STATES);
    for (int i=0; i < BOULDERHASH_STATES; i++) {
      state[i] = (uint64_t *)malloc(sizeof(uint64_t)*BOULDERHASH_STATE_SIZE);
    }
    return state;
  }
  
  void pc_free_state(uint64_t **state) {
    if (state == NULL)
      return;
    
    for (int i=0; i < BOULDERHASH_STATES; i++) {
      free(state[i]);
    }
    free(state);
  }
  
  void pc_init_threadpool(const boost::program_options::variables_map& vm)
  {
    check_init_threads(vm);
  }
  
  void pc_stop_threadpool()
  {
    check_stop_threads();
  }
  
  static void _pc_boulderhash(const void *data, std::size_t length, hash& hash, uint64_t **state) {
    uint64_t result[HASH_SIZE / sizeof(uint64_t)];
    uint64_t extra;
    
    pc_boulderhash_init(data, length, state, &result[0], &extra);
    
    fill_all_states(state);
    
    pc_boulderhash_calc_result(result, extra, state);
    
    // final hash
    cn_fast_hash(result, HASH_SIZE, reinterpret_cast<char *>(&hash));
  }

  static epee::critical_section g_boulderhash_state_lock;
  void pc_boulderhash(const void *data, std::size_t length, hash& hash, uint64_t **state) {
    if (state == NULL)
    {
      if (state == g_boulderhash_state)
        throw std::runtime_error("pc_boulderhash given NULL global state");
      else
        throw std::runtime_error("pc_boulderhash given NULL state");
    }
    
    if (state == g_boulderhash_state)
    {
      CRITICAL_REGION_LOCAL(g_boulderhash_state_lock);
      _pc_boulderhash(data, length, hash, state);
    }
    else
    {
      _pc_boulderhash(data, length, hash, state);
    }
  }
  
  hash pc_boulderhash(const void *data, std::size_t length, uint64_t **state) {
    hash h;
    pc_boulderhash(data, length, h, state);
    return h;
  }
}






