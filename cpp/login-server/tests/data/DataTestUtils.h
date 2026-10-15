#pragma once

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <string_view>

#include <gtest/gtest.h>
#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/database/SqlTypes.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"

/** Helpers for the login server data layer tests. */
namespace aion::loginserver::test {

inline std::string env(const char* name) {
	const char* value = std::getenv(name);
	return value ? value : "";
}

/** @return the current time truncated to whole seconds (TIMESTAMP columns store no fractional seconds) */
inline commons::database::Timestamp nowSeconds() {
	auto now = commons::database::Timestamp(std::chrono::milliseconds(commons::utils::currentTimeMillis()));
	return std::chrono::floor<std::chrono::seconds>(now);
}

/** @return milliseconds since the epoch of the timestamp */
inline int64_t millis(commons::database::Timestamp timestamp) {
	return timestamp.time_since_epoch().count();
}

/** Captures the messages of one logger subtree ("level|message" per line) while it exists; the messages don't reach the root sink. */
class LogCapture {
public:
	explicit LogCapture(std::string loggerName) : name(std::move(loggerName)) {
		auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
		sink->set_pattern("%l|%v");
		commons::logging::LoggerFactory::configure(name, {.sinks = {sink}, .additive = false});
	}
	~LogCapture() { commons::logging::LoggerFactory::removeConfig(name); }
	LogCapture(const LogCapture&) = delete;
	LogCapture& operator=(const LogCapture&) = delete;

	/** @return the captured lines with LF line ends (the sink writes the platform line separator) */
	std::string str() const {
		std::string text = stream.str();
		std::erase(text, '\r');
		return text;
	}
	bool contains(std::string_view text) const { return str().find(text) != std::string::npos; }

private:
	std::string name;
	std::ostringstream stream;
};

/** A directory below the system temp directory, removed with its content on destruction. */
class TempDirectory {
public:
	TempDirectory() {
		path = std::filesystem::temp_directory_path() / ("aion_ls_data_test_" + std::to_string(std::random_device()()));
		std::filesystem::create_directories(path);
	}
	~TempDirectory() {
		std::error_code ec;
		std::filesystem::remove_all(path, ec);
	}
	TempDirectory(const TempDirectory&) = delete;
	TempDirectory& operator=(const TempDirectory&) = delete;

	/** Writes a file (creating parent directories) */
	void write(const std::filesystem::path& relativePath, std::string_view content) const {
		std::filesystem::path file = path / relativePath;
		std::filesystem::create_directories(file.parent_path());
		std::ofstream(file, std::ios::binary) << content;
	}

	std::filesystem::path path;
};

/** Changes the current working directory while it exists. */
class ScopedCurrentPath {
public:
	explicit ScopedCurrentPath(const std::filesystem::path& path) : previous(std::filesystem::current_path()) { std::filesystem::current_path(path); }
	~ScopedCurrentPath() {
		std::error_code ec;
		std::filesystem::current_path(previous, ec);
	}
	ScopedCurrentPath(const ScopedCurrentPath&) = delete;
	ScopedCurrentPath& operator=(const ScopedCurrentPath&) = delete;

private:
	std::filesystem::path previous;
};

} // namespace aion::loginserver::test
