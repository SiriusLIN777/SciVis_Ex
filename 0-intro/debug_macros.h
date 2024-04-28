#ifndef DEBUG_MACROS_H
#define DEBUG_MACROS_H

#include <iostream>
#include <Windows.h>

// ANSI escape codes for text color
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"

//#define DEBUG_MODE
//#define INFO_MODE
//#define ERROR_MODE

#ifdef DEBUG_MODE
#define DEBUG(debugMsg) do { \
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE); \
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN); \
    std::cout << ANSI_COLOR_YELLOW << "[DEBUG] " << debugMsg << ANSI_COLOR_RESET << std::endl; \
} while (0)
#else
#define DEBUG(debugMsg)
#endif

#ifdef INFO_MODE
#define INFO(infoMsg) do { \
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE); \
    SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE); \
    std::cout << ANSI_COLOR_BLUE << "[INFO] " << infoMsg << ANSI_COLOR_RESET << std::endl; \
} while (0)
#else
#define INFO(infoMsg)
#endif

#ifdef ERROR_MODE
#define ERROR(errorMsg) do { \
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE); \
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED); \
    std::cout << ANSI_COLOR_RED << "[ERROR] " << errorMsg << ANSI_COLOR_RESET << std::endl; \
} while (0)
#else
#define ERROR(errorMsg)
#endif

#endif // DEBUG_MACROS_H