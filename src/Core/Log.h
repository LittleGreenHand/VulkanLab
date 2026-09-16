#pragma once

#include <format>
#include <string_view>

enum class LogLevel
{
	Debug,
	Info,
	Warning,
	Error,
	Fatal
};

class Log
{
public:
	Log() = delete;
	~Log() = delete;

	template<typename... Args>
	static void Write(
		LogLevel level,
		const char* file,
		int line,
		std::format_string<Args...> format,
		Args&&... args)
	{
		WriteFormatted(
			level,
			file,
			line,
			format.get(),
			std::make_format_args(args...));
	}

private:
	static void WriteFormatted(
		LogLevel level,
		const char* file,
		int line,
		std::string_view format,
		std::format_args args);

	static void WriteImpl(
		LogLevel level,
		const char* file,
		int line,
		std::string_view message);

	static std::string_view GetLevelName(LogLevel level);
	static std::string_view GetFileName(const char* file);
};


#if ENABLE_LOG

#if ENABLE_DEBUG_LOG

#define LOG_DEBUG(...)                                  \
    Log::Write(                                         \
        LogLevel::Debug,                                \
        __FILE__,                                       \
        __LINE__,                                       \
        __VA_ARGS__)

#else

#define LOG_DEBUG(...) ((void)0)

#endif


#define LOG_INFO(...)                                   \
    Log::Write(                                         \
        LogLevel::Info,                                 \
        __FILE__,                                       \
        __LINE__,                                       \
        __VA_ARGS__)


#define LOG_WARNING(...)                                \
    Log::Write(                                         \
        LogLevel::Warning,                              \
        __FILE__,                                       \
        __LINE__,                                       \
        __VA_ARGS__)


#define LOG_ERROR(...)                                  \
    Log::Write(                                         \
        LogLevel::Error,                                \
        __FILE__,                                       \
        __LINE__,                                       \
        __VA_ARGS__)


#define LOG_FATAL(...)                                  \
    Log::Write(                                         \
        LogLevel::Fatal,                                \
        __FILE__,                                       \
        __LINE__,                                       \
        __VA_ARGS__)

#else

#define LOG_DEBUG(...)   ((void)0)
#define LOG_INFO(...)    ((void)0)
#define LOG_WARNING(...) ((void)0)
#define LOG_ERROR(...)   ((void)0)
#define LOG_FATAL(...)   ((void)0)

#endif
