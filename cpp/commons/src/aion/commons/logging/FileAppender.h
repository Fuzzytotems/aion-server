#pragma once

#include <cstdio>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>

#include <spdlog/sinks/base_sink.h>

#include "aion/commons/logging/LevelFilter.h"

namespace aion::commons::logging {

/**
 * Java: ch.qos.logback.core.FileAppender with a PatternLayoutEncoder (append mode) - appends formatted messages to a file. Missing parent
 * directories are created. Other programs can read, write, rename and delete the file while it is open (like Java).
 */
class FileAppender final : public spdlog::sinks::base_sink<std::mutex> {
public:
	/**
	 * @param file the log file
	 * @param layout the formatter, usually a PatternLayout (logback: encoder)
	 * @param filter optional filter (logback: filter element)
	 * @param immediateFlush flush after each message (logback: immediateFlush, default true)
	 * @throws IOException if the file cannot be opened
	 */
	FileAppender(const std::filesystem::path& file, std::unique_ptr<spdlog::formatter> layout, std::optional<LevelFilter> filter = std::nullopt,
		bool immediateFlush = true);
	~FileAppender() override;

	FileAppender(const FileAppender&) = delete;
	FileAppender& operator=(const FileAppender&) = delete;

	const std::filesystem::path& getFile() const noexcept { return file; }

protected:
	void sink_it_(const spdlog::details::log_msg& msg) override;
	void flush_() override;

private:
	std::filesystem::path file;
	std::optional<LevelFilter> filter;
	bool immediateFlush;
	std::FILE* stream = nullptr;
	spdlog::memory_buf_t buffer;
};

} // namespace aion::commons::logging
