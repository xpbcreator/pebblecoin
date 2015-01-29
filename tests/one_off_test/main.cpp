#include "include_base_utils.h"

#include "crypto/hash.h"
#include "crypto/hash_options.h"

#include <stdio.h>

int main(int argc, char *argv[])
{
  epee::log_space::get_set_log_detalisation_level(true, LOG_LEVEL_2);
  epee::log_space::log_singletone::add_logger(LOGGER_CONSOLE, NULL, NULL, LOG_LEVEL_0);
  
  hashing_opt::_do_boulderhash = true;
  
  printf("Malloccing state...\n");
  uint64_t **state = crypto::pc_malloc_state();
  
  LOG_PRINT_L0("Initializing boulderhash threadpool...");
  boost::program_options::variables_map vm;
  crypto::pc_init_threadpool(vm);
  
  printf("state = %p\n", state);
  
  printf("Version 1 boulderhashing...\n");
  crypto::hash h;
  h = crypto::pc_boulderhash(1, "Testing!", 8, state);
  LOG_PRINT_L0("Result: " << epee::string_tools::pod_to_hex(h));
  
  printf("Version 2 boulderhashing...\n");
  h = crypto::pc_boulderhash(2, "Testing!", 8, state);
  LOG_PRINT_L0("Result: " << epee::string_tools::pod_to_hex(h));
  
  printf("Freeing state...\n");
  crypto::pc_free_state(state);
  
  return 0;
}
