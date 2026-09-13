#include "aion/commons/logging/FileAppender.h"

#include <cstdint>
#include <system_error>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

#include "aion/commons/utils/Exception.h"

namespace aion::commons::logging {

FileAppender::FileAppender(const std::filesystem::path& file, std::unique_ptr<spdlog::formatter> layout, std::optional<LevelFilter> filter,
	bool immediateFlush)
	: base_sink(std::move(layout)), file(file), filter(filter), immediateFlush(immediateFlush) {
	std::error_code error;
	if (file.has_parent_path())
		std::filesystem::create_directories(file.parent_path(), error); // failures show when opening the file
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
	if (!stream)
		throw utils::IOException("Could not open log file " + file.string());
}

FileAppender::~FileAppender() {
	if (stream)
		std::fclose(stream);
}

void FileAppender::sink_it_(const spdlog::details::log_msg& msg) {
	if (filter && !filter->accepts(msg.level))
		return;
	buffer.clear();
	formatter_->format(msg, buffer);
	std::fwrite(buffer.data(), 1, buffer.size(), stream);
	if (immediateFlush)
		std::fflush(stream);
}

void FileAppender::flush_() {
	std::fflush(stream);
}

} // namespace aion::commons::logging
