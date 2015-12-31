#include "version.h"

#include <iostream>


int main(int argc, char **argv)
{
  std::cout << PROJECT_VERSION_MAJOR << "."
            << PROJECT_VERSION_MINOR << "."
            << PROJECT_VERSION_REVISION << "."
            << PROJECT_VERSION_BUILD;
  
  return 0;
}
