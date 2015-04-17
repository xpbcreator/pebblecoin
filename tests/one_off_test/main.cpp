#include <chrono>
#include <thread>

#include "include_base_utils.h"

#include "crypto/hash.h"
#include "crypto/hash_options.h"

#include "common/ntp_time.h"

int main(int argc, char *argv[])
{
  epee::log_space::get_set_log_detalisation_level(true, LOG_LEVEL_4);
  epee::log_space::log_singletone::add_logger(LOGGER_CONSOLE, NULL, NULL, LOG_LEVEL_4);
  
  tools::ntp_time t(5);
  
  //t.set_ntp_timeout_ms(-1);
  
  LOG_PRINT_L0("Time is " << t.get_time() << ", local is " << time(NULL));
  
  std::this_thread::sleep_for(std::chrono::seconds(1));
  
  LOG_PRINT_L0("Time is " << t.get_time());

  std::this_thread::sleep_for(std::chrono::seconds(1));
  
  LOG_PRINT_L0("Time is " << t.get_time());

  std::this_thread::sleep_for(std::chrono::seconds(5));
  
  LOG_PRINT_L0("Time is " << t.get_time());
  
  LOG_PRINT_L0("Starting timeout at 0ms, incrementing by 50ms each time...");
  for (time_t timeout=0; timeout < 10000; timeout += 50)
  {
    t.set_ntp_timeout_ms(timeout);
    
    bool r = t.update();
    LOG_PRINT_L0("Update " << (r ? "succeeded" : "failed"));
    if (r) {
      break;
    }
  }
  
  LOG_PRINT_L0("Looping forever, try changing system time...");
  while (true)
  {
    LOG_PRINT_L0("Time is " << t.get_time());
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  
  return 0;
}
