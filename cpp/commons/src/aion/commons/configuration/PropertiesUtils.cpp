#include "aion/commons/configuration/PropertiesUtils.h"

#include <algorithm>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "aion/commons/utils/Exception.h"

namespace aion::commons::configuration::PropertiesUtils {

namespace {

std::string toDisplayString(const std::filesystem::path& path) {
	std::u8string u8 = path.u8string();
	return std::string(u8.begin(), u8.end());
}

void loadProperties(Properties& properties, const std::filesystem::path& file) {
	try {
		std::ifstream in(file, std::ios::binary);
		if (!in)
			throw utils::IOException("Could not open " + toDisplayString(file));
		properties.load(in);
	} catch (const utils::IOException&) {
		throw utils::IOException("Could not parse " + toDisplayString(file), std::current_exception());
	}
}

bool isMatchingFile(const std::filesystem::path& path, const std::filesystem::file_status& status) {
	// Java: attr.isRegularFile() && path.toString().endsWith(".properties"), with attributes read without following links
	return std::filesystem::is_regular_file(status) && toDisplayString(path).ends_with(".properties");
}

std::vector<std::filesystem::path> collectPropertiesFiles(const std::filesystem::path& dir, bool recursive) {
	namespace fs = std::filesystem;
	std::vector<fs::path> files;
	try {
		fs::file_status dirStatus = fs::symlink_status(dir);
		if (!fs::exists(dirStatus))
			throw utils::IOException("No such file or directory: " + toDisplayString(dir));
		// Deviation: Java's Files.find (without FOLLOW_LINKS) does not descend into a start directory that is a symbolic link, so it finds no files
		// there. Following the link is more useful (e.g. a linked config directory) and harmless.
		if (fs::is_symlink(dirStatus))
			dirStatus = fs::status(dir);
		if (!fs::is_directory(dirStatus)) {
			if (isMatchingFile(dir, dirStatus))
				files.push_back(dir);
			return files;
		}
		auto collect = [&](const fs::directory_entry& entry) {
			if (isMatchingFile(entry.path(), entry.symlink_status()))
				files.push_back(entry.path());
		};
		if (recursive) {
			for (const auto& entry : fs::recursive_directory_iterator(dir))
				collect(entry);
		} else {
			for (const auto& entry : fs::directory_iterator(dir))
				collect(entry);
		}
	} catch (const fs::filesystem_error& e) {
		throw utils::IOException(e.what(), std::current_exception());
	}
	// Deviation: Java loads the files in file system listing order, which is platform dependent. Sorting makes the result deterministic.
	std::ranges::sort(files);
	return files;
}

} // namespace

Properties load(const std::filesystem::path& file, std::shared_ptr<const Properties> defaults) {
	Properties p(std::move(defaults));
	std::error_code ec;
	if (std::filesystem::is_regular_file(file, ec))
		loadProperties(p, file);
	return p;
}

void loadFromDirectory(Properties& properties, const std::filesystem::path& dir, bool recursive) {
	for (const std::filesystem::path& file : collectPropertiesFiles(dir, recursive))
		loadProperties(properties, file);
}

} // namespace aion::commons::configuration::PropertiesUtils
