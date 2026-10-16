#pragma once

// Helpers of the static data binder tests: temporary directories with XML fixture files, exception message matchers.

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <thread>

#include <gtest/gtest.h>

#include "aion/gameserver/dataholders/loadingutils/StaticDataException.h"

namespace aion::gameserver::xml::test {

/** A unique temporary directory, removed with its contents on destruction. */
class TempDir {
public:
	TempDir() {
		static std::atomic<int> counter{0};
		auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		auto thread = std::hash<std::thread::id>{}(std::this_thread::get_id());
		dir = std::filesystem::temp_directory_path() /
		      ("aion_xml_test_" + std::to_string(stamp) + "_" + std::to_string(thread % 100000) + "_" + std::to_string(counter.fetch_add(1)));
		std::filesystem::create_directories(dir);
	}
	~TempDir() {
		std::error_code error;
		std::filesystem::remove_all(dir, error);
	}
	TempDir(const TempDir&) = delete;
	TempDir& operator=(const TempDir&) = delete;

	const std::filesystem::path& path() const { return dir; }

	/** writes a file (creating parent directories) and returns its path */
	std::filesystem::path write(std::string_view relative, std::string_view content) const {
		std::filesystem::path file = dir / std::filesystem::path(std::u8string(relative.begin(), relative.end()));
		std::filesystem::create_directories(file.parent_path());
		std::ofstream out(file, std::ios::binary);
		out.write(content.data(), static_cast<std::streamsize>(content.size()));
		return file;
	}

	std::filesystem::path mkdir(std::string_view relative) const {
		std::filesystem::path path = dir / std::filesystem::path(std::u8string(relative.begin(), relative.end()));
		std::filesystem::create_directories(path);
		return path;
	}

	/** path relative to the temporary directory with forward slashes */
	std::string relative(const std::filesystem::path& path) const { return std::filesystem::relative(path, dir).generic_string(); }

private:
	std::filesystem::path dir;
};

/** runs `statement`, expects a StaticDataException whose message contains every fragment */
#define EXPECT_STATIC_DATA_ERROR(statement, ...)                                                                                                     \
	do {                                                                                                                                               \
		bool thrown_ = false;                                                                                                                            \
		try {                                                                                                                                            \
			statement;                                                                                                                                     \
		} catch (const ::aion::gameserver::xml::StaticDataException& e_) {                                                                               \
			thrown_ = true;                                                                                                                                \
			std::string message_ = e_.what();                                                                                                              \
			for (std::string_view fragment_ : {__VA_ARGS__})                                                                                               \
				EXPECT_NE(message_.find(fragment_), std::string::npos) << "missing '" << fragment_ << "' in: " << message_;                                  \
		}                                                                                                                                                \
		EXPECT_TRUE(thrown_) << "expected StaticDataException from: " #statement;                                                                        \
	} while (false)

} // namespace aion::gameserver::xml::test
