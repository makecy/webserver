#pragma once
#include <string>
#include <iostream>
#include <ctime>
#include <sstream>


// for status printing
enum LogLevel
{
	DEBUG_LEVEL = 0,
	INFO_LEVEL = 1,
	ERROR_LEVEL = 2
};

#ifndef LOG_LEVEL
#define LOG_LEVEL INFO_LEVEL
#endif

std::string get_timestamp();
void log_debug(const std::string &message);
void log_info(const std::string &message);
void log_error(const std::string &message);

#if LOG_LEVEL <= DEBUG_LEVEL
	#define LOG_DEBUG(message) log_debug(message)
#else
	#define LOG_DEBUG(message) do {} while(0)
#endif

#if LOG_LEVEL <= INFO_LEVEL
	#define LOG_INFO(message) log_info(message)
#else
	#define LOG_INFO(message) do {} while(0)
#endif

#if LOG_LEVEL <= ERROR_LEVEL
	#define LOG_ERROR(message) log_error(message)
#else
	#define LOG_ERROR(message) do {} while(0)
#endif