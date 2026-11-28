#pragma once

#include <cstdint>
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
 * <p>
 * With a RollingPolicy it is a logback RollingFileAppender with a FixedWindowRollingPolicy and a SizeBasedTriggeringPolicy: before a message
 * is written, a file that has reached maxFileSize is renamed to fileNamePattern with minIndex, the older ones move up one index (the one at
 * maxIndex is deleted), and a new file is started.
 */
class FileAppender final : public spdlog::sinks::base_sink<std::mutex> {
public:
	/** logback FixedWindowRollingPolicy + SizeBasedTriggeringPolicy */
	struct RollingPolicy {
		/** the rolled file names, "%i" is replaced by the index (logback: fileNamePattern) */
		std::filesystem::path fileNamePattern;
		/** logback: minIndex (default 1) */
		int32_t minIndex = 1;
		/** logback: maxIndex (default 7); like logback, at least minIndex and at most minIndex + 20 */
		int32_t maxIndex = 7;
		/** logback SizeBasedTriggeringPolicy: maxFileSize in bytes (default 10 MB) */
		uint64_t maxFileSize = 10 * 1024 * 1024;

		/** @return fileNamePattern with "%i" replaced by index */
		std::filesystem::path fileNameOf(int32_t index) const;
	};

	/**
	 * @param file the log file
	 * @param layout the formatter, usually a PatternLayout (logback: encoder)
	 * @param filter optional filter (logback: filter element)
	 * @param immediateFlush flush after each message (logback: immediateFlush, default true)
	 * @param rollingPolicy optional size based rolling of the file (logback: RollingFileAppender)
	 * @throws IOException if the file cannot be opened
	 */
	FileAppender(const std::filesystem::path& file, std::unique_ptr<spdlog::formatter> layout, std::optional<LevelFilter> filter = std::nullopt,
		bool immediateFlush = true, std::optional<RollingPolicy> rollingPolicy = std::nullopt);
	~FileAppender() override;

	FileAppender(const FileAppender&) = delete;
	FileAppender& operator=(const FileAppender&) = delete;

	const std::filesystem::path& getFile() const noexcept { return file; }

	const std::optional<RollingPolicy>& getRollingPolicy() const noexcept { return rollingPolicy; }

protected:
	void sink_it_(const spdlog::details::log_msg& msg) override;
	void flush_() override;

private:
	/** @return the file opened in append mode, or nullptr */
	static std::FILE* open(const std::filesystem::path& file);
	/** logback RollingFileAppender.rollover: closes the file, renames the files of the window and opens a new file */
	void rollover();

	std::filesystem::path file;
	std::optional<LevelFilter> filter;
	bool immediateFlush;
	std::optional<RollingPolicy> rollingPolicy;
	std::FILE* stream = nullptr;
	/** bytes in the file (its size when opened plus what this appender wrote), for the SizeBasedTriggeringPolicy */
	uint64_t fileSize = 0;
	spdlog::memory_buf_t buffer;
};

} // namespace aion::commons::logging
