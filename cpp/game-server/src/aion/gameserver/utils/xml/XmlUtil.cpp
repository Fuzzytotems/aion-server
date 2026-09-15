#include "aion/gameserver/utils/xml/XmlUtil.h"

#include <cstdint>
#include <exception>
#include <limits>
#include <string>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::gameserver::utils::xml {

namespace {

std::string utf8(const std::filesystem::path& path) {
	std::u8string text = path.u8string();
	return std::string(text.begin(), text.end());
}

/**
 * Java BasicFileAttributes.isDirectory() read without following links: a symbolic link is not a directory, a junction is (Windows
 * attributes: only IO_REPARSE_TAG_SYMLINK makes a link). MSVC reports a junction as file_type::junction.
 */
bool isDirectory(std::filesystem::file_status linkStatus) {
	return std::filesystem::is_directory(linkStatus) || linkStatus.type() == std::filesystem::file_type::junction;
}

/** Java: attrs.isRegularFile() && path.toString().toLowerCase().endsWith(".xml"), attributes read without following links */
bool isXmlFile(const std::filesystem::path& path, std::filesystem::file_status linkStatus) {
	if (!std::filesystem::is_regular_file(linkStatus))
		return false;
	return commons::utils::StringUtils::toLowerCase(utf8(path)).ends_with(".xml");
}

/** Java FileTreeWalker without FOLLOW_LINKS: depth first, each entry before the contents of its directory; maxDepth limits opened directories */
void walk(const std::filesystem::path& directory, int32_t depth, int32_t maxDepth, std::vector<std::filesystem::path>& files) {
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory)) {
		std::filesystem::file_status status = entry.symlink_status();
		if (isXmlFile(entry.path(), status))
			files.push_back(entry.path());
		if (depth + 1 < maxDepth && isDirectory(status))
			walk(entry.path(), depth + 1, maxDepth, files);
	}
}

} // namespace

std::vector<std::filesystem::path> XmlUtil::listFiles(const std::filesystem::path& root, bool recursive) {
	std::vector<std::filesystem::path> files;
	try {
		std::filesystem::file_status rootStatus = std::filesystem::symlink_status(root);
		if (!std::filesystem::exists(rootStatus))
			throw commons::utils::IOException("java.nio.file.NoSuchFileException: " + utf8(root));
		if (!isDirectory(rootStatus)) { // Files.find visits the start file itself (depth 0); a symbolic link to a directory is not walked
			if (isXmlFile(root, rootStatus))
				files.push_back(root);
			return files;
		}
		walk(root, 0, recursive ? std::numeric_limits<int32_t>::max() : 1, files);
	} catch (const std::filesystem::filesystem_error& e) {
		try {
			throw commons::utils::IOException(e.what(), std::current_exception());
		} catch (const commons::utils::IOException& io) {
			throw commons::utils::Exception(io.what(), std::current_exception()); // Java: new RuntimeException(e)
		}
	} catch (const commons::utils::IOException& e) {
		throw commons::utils::Exception(e.what(), std::current_exception()); // Java: new RuntimeException(e)
	}
	return files;
}

} // namespace aion::gameserver::utils::xml
