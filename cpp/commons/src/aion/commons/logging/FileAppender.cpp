#include "aion/commons/logging/FileAppender.h"

#include <cstdint>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

#include "aion/commons/utils/Exception.h"

namespace aion::commons::logging {

std::filesystem::path FileAppender::RollingPolicy::fileNameOf(int32_t index) const {
	std::u8string pattern = fileNamePattern.u8string();
	const std::u8string token = u8"%i";
	const std::string number = std::to_string(index);
	const std::u8string replacement(number.begin(), number.end());
	for (size_t at = pattern.find(token); at != std::u8string::npos; at = pattern.find(token, at + replacement.size()))
		pattern.replace(at, token.size(), replacement);
	return std::filesystem::path(pattern);
}

FileAppender::FileAppender(const std::filesystem::path& file, std::unique_ptr<spdlog::formatter> layout, std::optional<LevelFilter> filter,
	bool immediateFlush, std::optional<RollingPolicy> rollingPolicy)
	: base_sink(std::move(layout)), file(file), filter(filter), immediateFlush(immediateFlush), rollingPolicy(std::move(rollingPolicy)) {
	if (this->rollingPolicy) {
		RollingPolicy& policy = *this->rollingPolicy;
		// logback FixedWindowRollingPolicy.start: a maxIndex below minIndex becomes minIndex, a window of more than 20 files is cut to 20
		if (policy.maxIndex < policy.minIndex)
			policy.maxIndex = policy.minIndex;
		if (policy.maxIndex - policy.minIndex > 20)
			policy.maxIndex = policy.minIndex + 20;
	}
	std::error_code error;
	if (file.has_parent_path())
		std::filesystem::create_directories(file.parent_path(), error); // failures show when opening the file
	stream = open(file);
	if (!stream)
		throw utils::IOException("Could not open log file " + file.string());
	uintmax_t size = std::filesystem::file_size(file, error);
	fileSize = error ? 0 : static_cast<uint64_t>(size);
}

FileAppender::~FileAppender() {
	if (stream)
		std::fclose(stream);
}

std::FILE* FileAppender::open(const std::filesystem::path& file) {
	std::FILE* stream = nullptr;
#ifdef _WIN32
	// like Java's FileOutputStream: other programs may read, write, delete and rename the file while it is open (e.g. Logging::archiveLogs while
	// an appender of a LoggerConfig still writes to it)
	HANDLE handle = CreateFileW(file.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (handle != INVALID_HANDLE_VALUE) {
		int fd = _open_osfhandle(static_cast<intptr_t>(reinterpret_cast<std::uintptr_t>(handle)), _O_APPEND | _O_BINARY);
		if (fd == -1)
			CloseHandle(handle);
		else if (!(stream = _fdopen(fd, "ab")))
			_close(fd);
	}
#else
	stream = std::fopen(file.c_str(), "ab");
#endif
	return stream;
}

void FileAppender::rollover() {
	const RollingPolicy& policy = *rollingPolicy;
	if (stream) {
		std::fclose(stream);
		stream = nullptr;
	}
	// logback FixedWindowRollingPolicy.rollover; like logback, a failed delete or rename is reported and the appender goes on
	auto report = [](const std::string& what, const std::error_code& error) {
		std::fprintf(stderr, "ERROR in FixedWindowRollingPolicy - %s: %s\n", what.c_str(), error.message().c_str());
	};
	std::error_code error;
	std::filesystem::path oldest = policy.fileNameOf(policy.maxIndex);
	if (std::filesystem::exists(oldest, error) && !std::filesystem::remove(oldest, error))
		report("could not delete " + oldest.string(), error);
	for (int32_t i = policy.maxIndex - 1; i >= policy.minIndex; i--) {
		std::filesystem::path toRename = policy.fileNameOf(i);
		if (std::filesystem::exists(toRename, error)) {
			std::filesystem::rename(toRename, policy.fileNameOf(i + 1), error);
			if (error)
				report("could not rename " + toRename.string(), error);
		}
	}
	std::filesystem::rename(file, policy.fileNameOf(policy.minIndex), error);
	if (error)
		report("could not rename " + file.string(), error);
	stream = open(file);
	if (!stream)
		std::fprintf(stderr, "ERROR in RollingFileAppender - could not open log file %s\n", file.string().c_str());
	uintmax_t size = std::filesystem::file_size(file, error);
	fileSize = error ? 0 : static_cast<uint64_t>(size);
}

void FileAppender::sink_it_(const spdlog::details::log_msg& msg) {
	if (filter && !filter->accepts(msg.level))
		return;
	if (rollingPolicy && fileSize >= rollingPolicy->maxFileSize) // logback: SizeBasedTriggeringPolicy.isTriggeringEvent before writing
		rollover();
	if (!stream)
		return;
	buffer.clear();
	formatter_->format(msg, buffer);
	fileSize += std::fwrite(buffer.data(), 1, buffer.size(), stream);
	if (immediateFlush)
		std::fflush(stream);
}

void FileAppender::flush_() {
	if (stream)
		std::fflush(stream);
}

} // namespace aion::commons::logging
