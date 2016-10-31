#ifndef DEBUG_HPP_
#define DEBUG_HPP_

#include <iostream>
#include <iomanip>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "typedefs.h"

#define __FILENAME__            (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define DEBUG1(x)               std::cout << "DEBUG1: " << std::setw(12) << __DATE__ \
                                          << ":" << std::setw(10) << __TIME__ \
                                          << ":" << std::setw(20) << __FILENAME__ \
                                          << ":" << std::setw(4) << __LINE__ \
                                          << ":" << std::setw(40) << __FUNCTION__ \
                                          << ":" << std::setw(12) << (uint_t) pthread_self() \
                                          << ":" << x << std::endl;

#define DEBUG2(x, args ...)     printf("DEBUG2: %12s:%10s:%20s:%4d:%40s:%12u:" x "\n", \
                                        __DATE__, __TIME__, __FILENAME__, __LINE__, __FUNCTION__, \
                                        (uint_t) pthread_self(), ##args)

#endif /* !DEBUG_HPP_ */
