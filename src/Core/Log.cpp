#include "Log.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#define ENABLE_FILE_LINE 0

std::atomic<LogLevel> Log::sMinLevel{ LogLevel::Debug };

namespace
{
	constexpr char kHighlightBegin = '\x1D';
	constexpr char kHighlightEnd = '\x1E';

	const char* GetLevelColor(LogLevel level)
	{
		switch (level)
		{
		case LogLevel::Debug:   return "\033[36m";
		case LogLevel::Info:    return "\033[92m";
		case LogLevel::Warning: return "\033[93m";
		case LogLevel::Error:   return "\033[91m";
		case LogLevel::Fatal:   return "\033[97;41m";
		}
		return "\033[0m";
	}

	const char* GetHighlightColor(LogLevel level)
	{
		// Fatal 保留红色背景
		return level == LogLevel::Fatal ? "\033[96;41m" : "\033[96m";
	}

	const char* GetFileColor() { return "\033[95;49m"; }
	const char* GetLineColor() { return "\033[95;49m"; }
	const char* ResetColor() { return "\033[0m"; }

	std::string_view GetLevelName(LogLevel level)
	{
		switch (level)
		{
		case LogLevel::Debug:   return "DEBUG";
		case LogLevel::Info:    return "INFO";
		case LogLevel::Warning: return "WARNING";
		case LogLevel::Error:   return "ERROR";
		case LogLevel::Fatal:   return "FATAL";
		}
		return "UNKNOWN";
	}

	std::string_view GetFileName(const char* file)
	{
		if (file == nullptr)
			return {};

		std::string_view path(file);
		const size_t slashPosition = path.find_last_of("/\\");

		if (slashPosition == std::string_view::npos)
			return path;

		return path.substr(slashPosition + 1);
	}

	void BuildHighlightedFormat(std::string_view format, std::string& result)
	{
		result.clear();

		if (result.capacity() < format.size() + 16)
			result.reserve(format.size() + 16);

		size_t position = 0;

		while (position < format.size())
		{
			const char current = format[position];

			if (current != '{')
			{
				result.push_back(current);
				++position;
				continue;
			}

			if (position + 1 < format.size() && format[position + 1] == '{')
			{
				result.append("{{");
				position += 2;
				continue;
			}

			const size_t fieldBegin = position;
			size_t braceDepth = 0;

			do
			{
				const char c = format[position];
				if (c == '{') ++braceDepth;
				else if (c == '}') --braceDepth;
				++position;
			} while (position < format.size() && braceDepth != 0);

			result.push_back(kHighlightBegin);
			result.append(format.data() + fieldBegin, position - fieldBegin);
			result.push_back(kHighlightEnd);
		}
	}

	void AppendHighlightedMessage(
		std::string& out,
		LogLevel level,
		std::string_view message)
	{
		size_t position = 0;
		bool highlighting = false;

		while (position < message.size())
		{
			const char marker = highlighting ? kHighlightEnd : kHighlightBegin;
			const size_t markerPosition = message.find(marker, position);

			if (markerPosition == std::string_view::npos)
			{
				out.append(message.data() + position, message.size() - position);
				break;
			}

			if (markerPosition > position)
			{
				out.append(message.data() + position, markerPosition - position);
			}

			highlighting = !highlighting;
			out += highlighting ? GetHighlightColor(level) : GetLevelColor(level);
			position = markerPosition + 1;
		}
	}

	struct LogMessage
	{
		LogLevel level;
		const char* file;
		int line;
		std::string text;
	};

#if defined(_WIN32)
	void EnableVirtualTerminal()
	{
		HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
		if (handle == INVALID_HANDLE_VALUE || handle == nullptr)
			return;

		DWORD mode = 0;
		if (!GetConsoleMode(handle, &mode))
			return;

		mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		SetConsoleMode(handle, mode);
	}
#endif

	class AsyncLogger
	{
	public:
		static AsyncLogger& Instance()
		{
			static AsyncLogger instance;
			return instance;
		}

		void Enqueue(LogMessage&& msg)
		{
			{
				std::unique_lock<std::mutex> lock(mutex_);

				if (queue_.size() >= kMaxQueueSize)
				{
					// 队列满：低级别直接丢弃，避免阻塞业务线程
					if (msg.level < LogLevel::Warning)
						return;

					// 高级别日志覆盖最旧一条
					queue_.pop_front();
				}

				queue_.push_back(std::move(msg));
			}

			cv_.notify_one();
		}

		void Shutdown()
		{
			{
				std::lock_guard<std::mutex> lock(mutex_);
				if (!running_)
					return;
				running_ = false;
			}

			cv_.notify_all();

			if (worker_.joinable())
				worker_.join();
		}

	private:
		AsyncLogger()
			: worker_([this] { Worker(); })
		{
#if defined(_WIN32)
			EnableVirtualTerminal();
#endif
		}

		~AsyncLogger()
		{
			Shutdown();
		}

		void Worker()
		{
			std::deque<LogMessage> local;

			while (true)
			{
				{
					std::unique_lock<std::mutex> lock(mutex_);
					cv_.wait(lock, [this]
						{
							return !queue_.empty() || !running_;
						});

					if (!running_ && queue_.empty())
						break;

					// 批量取，减少锁竞争
					constexpr size_t kBatchSize = 128;
					for (size_t i = 0; i < kBatchSize && !queue_.empty(); ++i)
					{
						local.push_back(std::move(queue_.front()));
						queue_.pop_front();
					}
				}

				for (auto& msg : local)
				{
					WriteMessageSync(msg);
				}

				local.clear();
			}

			// 退出前排空剩余日志
			std::deque<LogMessage> remain;
			{
				std::lock_guard<std::mutex> lock(mutex_);
				remain.swap(queue_);
			}

			for (auto& msg : remain)
			{
				WriteMessageSync(msg);
			}
		}

		void WriteMessageSync(const LogMessage& msg)
		{
			std::ostream& output =
				(msg.level == LogLevel::Error || msg.level == LogLevel::Fatal)
				? std::cerr
				: std::cout;

			std::string line;
			line.reserve(msg.text.size() + 64);

			line += GetLevelColor(msg.level);
			line += '[';
			line.append(GetLevelName(msg.level));
			line += "] ";

			AppendHighlightedMessage(line, msg.level, msg.text);

#if ENABLE_FILE_LINE
			line += GetFileColor();
			line += "   [";
			line.append(GetFileName(msg.file));
			line += ':';
			line += std::to_string(msg.line);
			line += ']';
#endif

			line += ResetColor();
			line += '\n';

			output.write(line.data(), static_cast<std::streamsize>(line.size()));
		}

		static constexpr size_t kMaxQueueSize = 10000;

		std::mutex mutex_;
		std::condition_variable cv_;
		std::deque<LogMessage> queue_;
		std::thread worker_;
		bool running_ = true;
	};

} // namespace

void Log::WriteFormatted(
	LogLevel level,
	const char* file,
	int line,
	std::string_view format,
	std::format_args args)
{
	if (!ShouldLog(level))
		return;

	// 线程局部复用，减少分配
	thread_local std::string tlsHighlightedFormat;
	BuildHighlightedFormat(format, tlsHighlightedFormat);

	std::string formattedMessage;
	formattedMessage.reserve(tlsHighlightedFormat.size() + 64);

	std::vformat_to(
		std::back_inserter(formattedMessage),
		tlsHighlightedFormat,
		args);

	AsyncLogger::Instance().Enqueue(
		LogMessage{ level, file, line, std::move(formattedMessage) });
}

void Log::Shutdown()
{
	AsyncLogger::Instance().Shutdown();
}

namespace
{
	using TimerClock = std::chrono::steady_clock;
	thread_local std::unordered_map<std::string_view, TimerClock::time_point> gTimers;
	std::unordered_map<std::string, double> gTimerRecorder;
	std::mutex gTimerRecorderMutex;
	std::atomic<bool> gEnableTimerRecording{ false };
}

void Log::BeginTimer(std::string_view name)
{
	gTimers.insert_or_assign(name, TimerClock::now());
}

void Log::EndTimer(std::string_view name, bool isWriteConsole, const char* file, int line)
{
	const auto end = TimerClock::now();
	const auto it = gTimers.find(name);

	if (it == gTimers.end())
	{
		Write(LogLevel::Warning, file, line, "[TIME] Timer '{}' not found", name);
		return;
	}

	const double ms =
		std::chrono::duration<double, std::milli>(end - it->second).count();

	gTimers.erase(it);

	if (isWriteConsole)
	{
		Write(LogLevel::Info, file, line, "[TIME] {}: {:.3f} ms", name, ms);
	}

	if (gEnableTimerRecording)
	{
		std::lock_guard<std::mutex> lock(gTimerRecorderMutex);

		auto [iter, inserted] =
			gTimerRecorder.try_emplace(std::string{ name }, ms);

		if (!inserted)
		{
			constexpr double kEmaAlpha = 0.8;
			iter->second =
				iter->second * (1.0 - kEmaAlpha) + ms * kEmaAlpha;
		}
	}
}

std::unordered_map<std::string, double> Log::GetTimerRecords()
{
	std::lock_guard<std::mutex> lock(gTimerRecorderMutex);
	return gTimerRecorder;
}

void Log::ClearTimerRecords()
{
	std::lock_guard<std::mutex> lock(gTimerRecorderMutex);
	gTimerRecorder.clear();
}

void Log::EnableTimerRecording(bool enable)
{
	gEnableTimerRecording.store(enable);
	if (!enable)
		ClearTimerRecords();
}