#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

#define STR_IMPL_(x) #x
#define STR(x) STR_IMPL_(x)

#define BOLD_PRINT_NO_RETURN(format, ...) printf("\033[1m" format "\033[0m", ##__VA_ARGS__)

#define BOLD_PRINT(format, ...) printf("\033[1m" format "\033[0m\r\n", ##__VA_ARGS__)
#define SUCCESS_PRINT(format, ...) printf("\033[1;32m" format "\033[0m\r\n", ##__VA_ARGS__)
#define WARNING_PRINT(format, ...) printf("\033[1;33m" format "\033[0m\r\n", ##__VA_ARGS__)
#define ERROR_PRINT(format, ...) printf("\033[1;31m" format "\033[0m\r\n", ##__VA_ARGS__)

#endif
