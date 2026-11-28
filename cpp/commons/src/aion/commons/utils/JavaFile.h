#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

/**
 * java.io.File's ordering of abstract path names, for code that sorts the files it lists (upstream 2cd926056: PropertiesUtils and XmlUtil
 * sort their Files.find results).
 */
namespace aion::commons::utils::JavaFile {

/**
 * Java: File.compareTo, i.e. the platform FileSystem's compare of the two path strings. Windows (WinNTFileSystem): String.compareToIgnoreCase of
 * the paths with '\' separators ('/' is normalized to '\', like java.io.File does). Other systems (UnixFileSystem): String.compareTo, the
 * ordinal order of the UTF-16 code units.
 *
 * @return a negative number, zero or a positive number as `a` sorts before, together with or after `b`
 */
int32_t compare(const std::filesystem::path& a, const std::filesystem::path& b);

/** Java: Stream&lt;File&gt;.sorted() (a stable sort in File.compareTo order) */
void sort(std::vector<std::filesystem::path>& files);

} // namespace aion::commons::utils::JavaFile
