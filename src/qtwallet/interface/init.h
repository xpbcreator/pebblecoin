#ifndef PEBBLECOIN_BCINIT_H
#define PEBBLECOIN_BCINIT_H

#include <QString>

namespace boost {
  class thread_group;
  class thread;
};

void StartShutdown();
bool ShutdownRequested();
void Shutdown();
bool AppInit2(boost::thread_group& threadGroup, QString& error);


#endif
