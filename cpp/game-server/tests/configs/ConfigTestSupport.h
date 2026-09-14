#pragma once

#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/configuration/Properties.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/services/cron/CronExpression.h"

/** Helpers of the game server config tests. */
namespace aion::gameserver::configs::test {

/** The Java game server directory of this repository (game-server/: config/, src/) */
inline std::filesystem::path javaGameServerDir() {
	return std::filesystem::path(AION_GAMESERVER_JAVA_DIR);
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

	/** @return the captured lines with LF line ends */
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
		path = std::filesystem::temp_directory_path() / ("aion_gs_configs_test_" + std::to_string(std::random_device()()));
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

/**
 * Copies the shipped configuration directories (config/administration, config/main, config/network of the Java game server) into dir/config.
 * config/mygs.properties, which an operator may have created for a real server, is left out so it cannot change the results.
 */
inline void copyShippedConfig(const TempDirectory& dir) {
	std::filesystem::path javaConfig = javaGameServerDir() / "config";
	std::filesystem::create_directories(dir.path / "config");
	for (const char* subdirectory : {"administration", "main", "network"}) {
		ASSERT_TRUE(std::filesystem::is_directory(javaConfig / subdirectory)) << javaConfig / subdirectory;
		std::filesystem::copy(javaConfig / subdirectory, dir.path / "config" / subdirectory, std::filesystem::copy_options::recursive);
	}
}

/** One @Property or @Properties annotation of a Java config class */
struct JavaProperty {
	std::string className;
	std::string fieldName;
	/** @Property key, empty for @Properties */
	std::string key;
	/** @Property defaultValue if present */
	std::optional<std::string> defaultValue;
	/** @Properties keyPattern if present */
	std::optional<std::string> keyPattern;
};

/** Decodes the body of a Java string literal (\\uXXXX, \\", \\\\, \\n, \\t) to UTF-8. */
inline std::string decodeJavaStringLiteral(std::string_view literal) {
	std::string out;
	auto appendUtf8 = [&out](char32_t c) {
		if (c < 0x80) {
			out += static_cast<char>(c);
		} else if (c < 0x800) {
			out += static_cast<char>(0xC0 | (c >> 6));
			out += static_cast<char>(0x80 | (c & 0x3F));
		} else {
			out += static_cast<char>(0xE0 | (c >> 12));
			out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
			out += static_cast<char>(0x80 | (c & 0x3F));
		}
	};
	for (size_t i = 0; i < literal.size(); ++i) {
		char c = literal[i];
		if (c != '\\') {
			out += c;
			continue;
		}
		char next = literal.at(++i);
		switch (next) {
			case 'u':
				appendUtf8(static_cast<char32_t>(std::stoul(std::string(literal.substr(i + 1, 4)), nullptr, 16)));
				i += 4;
				break;
			case 'n':
				out += '\n';
				break;
			case 't':
				out += '\t';
				break;
			default:
				out += next;
		}
	}
	return out;
}

/**
 * Scans a Java config class source for its @Property/@Properties annotations (independent of the C++ bind functions, which were written from
 * the same annotations). Annotations may span lines; each is attributed to the next "public static ... NAME;" declaration.
 */
inline std::vector<JavaProperty> scanJavaConfigClass(const std::filesystem::path& javaFile) {
	std::ifstream in(javaFile, std::ios::binary);
	EXPECT_TRUE(in) << javaFile;
	std::stringstream buffer;
	buffer << in.rdbuf();
	const std::string source = buffer.str();
	const std::string className = javaFile.stem().string();
	static const std::regex annotationOrField(
	  R"re(@Property\(\s*key\s*=\s*"((?:[^"\\]|\\.)*)"(?:\s*,\s*defaultValue\s*=\s*"((?:[^"\\]|\\.)*)")?\s*\)|@Properties\(\s*keyPattern\s*=\s*"((?:[^"\\]|\\.)*)"\s*\)|public\s+static\s+[\w<>\[\], ]+?\s+(\w+)\s*(?:=[^;]*)?;)re");
	std::vector<JavaProperty> result;
	std::optional<JavaProperty> pending;
	for (auto it = std::sregex_iterator(source.begin(), source.end(), annotationOrField); it != std::sregex_iterator(); ++it) {
		const std::smatch& m = *it;
		if (m[4].matched) {
			if (pending) {
				pending->fieldName = m[4].str();
				result.push_back(std::move(*pending));
				pending.reset();
			}
			continue;
		}
		EXPECT_FALSE(pending) << "two annotations without field in " << javaFile;
		JavaProperty property{.className = className};
		if (m[1].matched) {
			property.key = decodeJavaStringLiteral(m[1].str());
			if (m[2].matched)
				property.defaultValue = decodeJavaStringLiteral(m[2].str());
		} else {
			property.keyPattern = decodeJavaStringLiteral(m[3].str());
		}
		pending = std::move(property);
	}
	EXPECT_FALSE(pending) << "annotation without field in " << javaFile;
	return result;
}

/** @return the text of a CronExpression field value, "<null>" for nullptr */
inline std::string cronText(const services::cron::CronExpression* expression) {
	return expression ? expression->getCronExpression() : "<null>";
}

/** @return the texts of a CronExpression[] field value */
inline std::vector<std::string> cronTexts(const std::vector<const services::cron::CronExpression*>& expressions) {
	std::vector<std::string> texts;
	for (const services::cron::CronExpression* expression : expressions)
		texts.push_back(cronText(expression));
	return texts;
}

} // namespace aion::gameserver::configs::test
