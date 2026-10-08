#pragma once

#include <atomic>
#include <format>
#include <string_view>
#include <unordered_map>

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
		if (!ShouldLog(level))
			return;

		WriteFormatted(level, file, line, format.get(), std::make_format_args(args...));
	}

	static bool ShouldLog(LogLevel level) noexcept
	{
		return static_cast<int>(level) >= static_cast<int>(sMinLevel.load(std::memory_order_relaxed));
	}

	static void SetMinLevel(LogLevel level) noexcept
	{
		sMinLevel.store(level, std::memory_order_relaxed);
	}

	static void BeginTimer(std::string_view name);
	static void EndTimer(std::string_view name, bool isWriteConsole, const char* file, int line);
	static void EnableTimerRecording(bool enable);
	static std::unordered_map<std::string, double> GetTimerRecords();
	static void ClearTimerRecords();

	// 程序退出前调用，确保后台日志写完
	static void Shutdown();

private:
	static std::atomic<LogLevel> sMinLevel;

	static void WriteFormatted(
		LogLevel level,
		const char* file,
		int line,
		std::string_view format,
		std::format_args args);
};

#if ENABLE_LOG

#if ENABLE_DEBUG_LOG
#define LOG_DEBUG(...)                                      \
    do {                                                    \
        if (Log::ShouldLog(LogLevel::Debug))                \
            Log::Write(LogLevel::Debug, __FILE__, __LINE__, __VA_ARGS__); \
    } while (0)
#else
#define LOG_DEBUG(...) ((void)0)
#endif

#define LOG_INFO(...)                                       \
    do {                                                    \
        if (Log::ShouldLog(LogLevel::Info))                 \
            Log::Write(LogLevel::Info, __FILE__, __LINE__, __VA_ARGS__);  \
    } while (0)

#define LOG_WARNING(...)                                    \
    do {                                                    \
        if (Log::ShouldLog(LogLevel::Warning))              \
            Log::Write(LogLevel::Warning, __FILE__, __LINE__, __VA_ARGS__); \
    } while (0)

#define LOG_ERROR(...)                                      \
    do {                                                    \
        if (Log::ShouldLog(LogLevel::Error))                \
            Log::Write(LogLevel::Error, __FILE__, __LINE__, __VA_ARGS__); \
    } while (0)

#define LOG_FATAL(...)                                      \
    do {                                                    \
        if (Log::ShouldLog(LogLevel::Fatal))                \
            Log::Write(LogLevel::Fatal, __FILE__, __LINE__, __VA_ARGS__); \
    } while (0)

#define LOG_TIME_BEGIN(name) Log::BeginTimer(#name)
#define LOG_TIME_END(name, isWriteConsole) \
    Log::EndTimer(#name, isWriteConsole, __FILE__, __LINE__)

#else

#define LOG_DEBUG(...)   ((void)0)
#define LOG_INFO(...)    ((void)0)
#define LOG_WARNING(...) ((void)0)
#define LOG_ERROR(...)   ((void)0)
#define LOG_FATAL(...)   ((void)0)
#define LOG_TIME_BEGIN(name) ((void)0)
#define LOG_TIME_END(name, isWriteConsole) ((void)0)

#endif