#include "Log.h"

#include <iostream>
#include <iterator>
#include <mutex>
#include <string>

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#endif

#define ENABLE_FILE_LINE 0

namespace
{
	// 使用不可见控制字符作为格式化参数的内部标记。
	// 它们只存在于格式化后的临时字符串中，真正输出控制台前会被移除。
	constexpr char kHighlightBegin = '\x1D';
	constexpr char kHighlightEnd = '\x1E';

	// 防止多个线程同时输出日志时互相穿插。
	std::mutex gLogMutex;


	void BuildHighlightedFormat(
		std::string_view format,
		std::string& result)
	{
		result.clear();

		// 一般日志只会多出少量 marker；保留 capacity 可以避免后续日志重复分配。
		if (result.capacity() < format.size() + 16)
		{
			result.reserve(format.size() + 16);
		}

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

			// "{{" 是 std::format 的转义左花括号，不是 replacement field。
			if (position + 1 < format.size() &&
				format[position + 1] == '{')
			{
				result.append("{{");
				position += 2;
				continue;
			}

			// 找到真正的 replacement field：
			//   {}
			//   {:.2f}
			//   {0}
			//   {0:{1}}
			//
			// 对整个 replacement field 前后插入内部 marker。
			const size_t fieldBegin = position;
			size_t braceDepth = 0;

			do
			{
				const char c = format[position];

				if (c == '{')
				{
					++braceDepth;
				}
				else if (c == '}')
				{
					--braceDepth;
				}

				++position;

			} while (position < format.size() && braceDepth != 0);

			result.push_back(kHighlightBegin);
			result.append(
				format.data() + fieldBegin,
				position - fieldBegin);
			result.push_back(kHighlightEnd);
		}
	}


#if defined(_WIN32)

	HANDLE gConsoleHandle = INVALID_HANDLE_VALUE;

	// 保存控制台原始颜色。
	WORD gDefaultConsoleAttributes =
		FOREGROUND_RED |
		FOREGROUND_GREEN |
		FOREGROUND_BLUE;

	bool gConsoleInitialized = false;


	void InitializeWindowsConsole()
	{
		if (gConsoleInitialized)
		{
			return;
		}

		gConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

		if (gConsoleHandle == INVALID_HANDLE_VALUE ||
			gConsoleHandle == nullptr)
		{
			return;
		}

		CONSOLE_SCREEN_BUFFER_INFO info{};

		if (GetConsoleScreenBufferInfo(
			gConsoleHandle,
			&info))
		{
			gDefaultConsoleAttributes = info.wAttributes;
			gConsoleInitialized = true;
		}
	}


	void SetConsoleAttributes(WORD attributes)
	{
		if (!gConsoleInitialized)
		{
			InitializeWindowsConsole();
		}

		if (gConsoleHandle == INVALID_HANDLE_VALUE ||
			gConsoleHandle == nullptr)
		{
			return;
		}

		SetConsoleTextAttribute(
			gConsoleHandle,
			attributes);
	}


	WORD GetLevelColor(LogLevel level)
	{
		switch (level)
		{
		case LogLevel::Debug:
			// 青色
			return
				FOREGROUND_GREEN |
				FOREGROUND_BLUE;

		case LogLevel::Info:
			// 亮绿色
			return
				FOREGROUND_GREEN |
				FOREGROUND_INTENSITY;

		case LogLevel::Warning:
			// 亮黄色
			return
				FOREGROUND_RED |
				FOREGROUND_GREEN |
				FOREGROUND_INTENSITY;

		case LogLevel::Error:
			// 亮红色
			return
				FOREGROUND_RED |
				FOREGROUND_INTENSITY;

		case LogLevel::Fatal:
			// 亮白色 + 红色背景
			return
				FOREGROUND_RED |
				FOREGROUND_GREEN |
				FOREGROUND_BLUE |
				FOREGROUND_INTENSITY |
				BACKGROUND_RED;
		}

		return gDefaultConsoleAttributes;
	}


	void SetLevelColor(
		std::ostream&,
		LogLevel level)
	{
		SetConsoleAttributes(
			GetLevelColor(level));
	}


	void SetHighlightColor(
		std::ostream&,
		LogLevel level)
	{
		// 格式化参数使用亮青色。
		WORD color =
			FOREGROUND_GREEN |
			FOREGROUND_BLUE |
			FOREGROUND_INTENSITY;

		// Fatal 保留红色背景，不让参数高亮破坏整条 Fatal 的视觉语义。
		if (level == LogLevel::Fatal)
		{
			color |= BACKGROUND_RED;
		}

		SetConsoleAttributes(color);
	}


	void SetFileColor(std::ostream&)
	{
		// 亮紫色
		SetConsoleAttributes(
			FOREGROUND_RED |
			FOREGROUND_BLUE |
			FOREGROUND_INTENSITY);
	}


	void SetLineColor(std::ostream&)
	{
		// 亮紫色
		SetConsoleAttributes(
			FOREGROUND_RED |
			FOREGROUND_BLUE |
			FOREGROUND_INTENSITY);
	}


	void ResetConsoleColor(std::ostream&)
	{
		if (!gConsoleInitialized)
		{
			return;
		}

		SetConsoleAttributes(
			gDefaultConsoleAttributes);
	}


#else


	const char* GetLevelColor(LogLevel level)
	{
		switch (level)
		{
		case LogLevel::Debug:
			return "\033[36m";

		case LogLevel::Info:
			return "\033[92m";

		case LogLevel::Warning:
			return "\033[93m";

		case LogLevel::Error:
			return "\033[91m";

		case LogLevel::Fatal:
			return "\033[97;41m";
		}

		return "\033[0m";
	}


	void SetLevelColor(
		std::ostream& output,
		LogLevel level)
	{
		output << GetLevelColor(level);
	}


	void SetHighlightColor(
		std::ostream& output,
		LogLevel level)
	{
		if (level == LogLevel::Fatal)
		{
			// 亮青色 + 保留 Fatal 红色背景。
			output << "\033[96;41m";
		}
		else
		{
			output << "\033[96m";
		}
	}


	void SetFileColor(std::ostream& output)
	{
		// 亮紫色
		output << "\033[95;49m";
	}


	void SetLineColor(std::ostream& output)
	{
		// 亮紫色
		output << "\033[95;49m";
	}


	void ResetConsoleColor(std::ostream& output)
	{
		output << "\033[0m";
	}


#endif


	void WriteHighlightedMessage(
		std::ostream& output,
		LogLevel level,
		std::string_view message)
	{
		size_t position = 0;
		bool highlighting = false;

		while (position < message.size())
		{
			const char marker =
				highlighting ? kHighlightEnd : kHighlightBegin;

			const size_t markerPosition =
				message.find(marker, position);

			if (markerPosition == std::string_view::npos)
			{
				output.write(
					message.data() + position,
					static_cast<std::streamsize>(
						message.size() - position));
				break;
			}

			if (markerPosition > position)
			{
				output.write(
					message.data() + position,
					static_cast<std::streamsize>(
						markerPosition - position));
			}

			highlighting = !highlighting;

			if (highlighting)
			{
				SetHighlightColor(output, level);
			}
			else
			{
				SetLevelColor(output, level);
			}

			position = markerPosition + 1;
		}
	}

} // namespace


void Log::WriteFormatted(
	LogLevel level,
	const char* file,
	int line,
	std::string_view format,
	std::format_args args)
{
	std::string highlightedFormat;
	std::string formattedMessage;

	BuildHighlightedFormat(
		format,
		highlightedFormat);

	formattedMessage.clear();

	std::vformat_to(
		std::back_inserter(formattedMessage),
		highlightedFormat,
		args);

	WriteImpl(
		level,
		file,
		line,
		formattedMessage);
}


void Log::WriteImpl(
	LogLevel level,
	const char* file,
	int line,
	std::string_view message)
{
	const std::scoped_lock lock(gLogMutex);

	std::ostream& output =
		(level == LogLevel::Error ||
			level == LogLevel::Fatal)
		? std::cerr
		: std::cout;

	SetLevelColor(
		output,
		level);

	output
		<< '['
		<< GetLevelName(level)
		<< "] ";

	WriteHighlightedMessage(
		output,
		level,
		message);


#if ENABLE_FILE_LINE

	// Source Location 整体弱化，行号单独突出。
	SetFileColor(output);

	output
		<< "   ["
		<< GetFileName(file)
		<< ':';

	SetLineColor(output);

	output << line;

	SetFileColor(output);

	output << ']';

#endif

	ResetConsoleColor(output);
	output << '\n';
}


std::string_view Log::GetLevelName(LogLevel level)
{
	switch (level)
	{
	case LogLevel::Debug:
		return "DEBUG";

	case LogLevel::Info:
		return "INFO";

	case LogLevel::Warning:
		return "WARNING";

	case LogLevel::Error:
		return "ERROR";

	case LogLevel::Fatal:
		return "FATAL";
	}

	return "UNKNOWN";
}


std::string_view Log::GetFileName(const char* file)
{
	if (file == nullptr)
	{
		return {};
	}

	std::string_view path(file);

	const size_t slashPosition =
		path.find_last_of("/\\");

	if (slashPosition == std::string_view::npos)
	{
		return path;
	}

	return path.substr(
		slashPosition + 1);
}
