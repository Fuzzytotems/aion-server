#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aion::gameserver::xml {

/** One resolved `<import>` of static_data.xml. */
struct StaticDataImport {
	/** the `file` attribute as written */
	std::string file;
	/** the imported file or directory after the region override */
	std::filesystem::path path;
	bool directory = false;
	/** `singleRootTag` (directories only; always true for accepted directory imports) */
	bool singleRootTag = false;
	/** `recursiveImport` (directories only, default true) */
	bool recursive = true;
	/** the XML files in binding order: the file itself, or the directory's .xml files in depth-first NTFS order (never empty) */
	std::vector<std::filesystem::path> files;
	/** line of the <import> element in static_data.xml */
	uint32_t line = 0;
};

/**
 * Import resolution of static_data.xml with XmlMerger's rules, without writing a merged file (Java: XmlMerger + XmlUtil.listFiles;
 * docs/design/static-data.md §3.1). @author Aquanox, Neon (XmlMerger)
 *
 * 1. static_data.xml's root must be <static_data>; each child <import file=".." singleRootTag=".." recursiveImport=".."/> in document order.
 *    Other child elements (inline holders) are rejected: XmlMerger would copy them, but no data file uses that.
 * 2. Region override (XmlMerger.applyCountryOverride): for country codes 1 usa, 2 europe, 4 japan, 5 china, 6 taiwan, 7 russia,
 *    `<base>_<region><ext>` (split at the last '.' of the file name) replaces the import only if it is a regular file, so directories are
 *    never overridden by other directories.
 * 3. A regular file is imported as is. Otherwise the path must be a directory: its .xml files (case-insensitive suffix, regular files only,
 *    symbolic links not followed, subdirectories only with recursiveImport) are sorted in File.compareTo order of their paths (upstream
 *    2cd926056; commons::utils::JavaFile::compare: on Windows case-insensitive with backslash separators, so "a-b.xml" sorts before the
 *    files of directory "a" and "a_b.xml" before "ab.xml").
 * 4. A directory import requires singleRootTag="true" (XmlMerger would write an unbalanced end tag otherwise) and at least one .xml file
 *    (an empty directory would also unbalance the merged document).
 * Attribute values of singleRootTag/recursiveImport follow Boolean.parseBoolean ("true" in any case is true, everything else false).
 * xmlns and xsi:* attributes are ignored; other unknown attributes are errors in strict mode and warnings otherwise.
 *
 * Deviation: no ./cache/static_data.xml and no CRC metadata file (they only existed for JAXB).
 */
class StaticDataImports {
public:
	/** XmlMerger.COUNTRY_REGION: the file name suffix of a country code, empty if the code has no region */
	static std::string_view countryRegion(int32_t countryCode) noexcept;
	/** XmlMerger.applyCountryOverride */
	static std::filesystem::path applyCountryOverride(const std::filesystem::path& file, int32_t countryCode);
	/** XmlUtil.listFiles(root, recursive), sorted (see the class comment). @throws StaticDataException if `directory` cannot be listed */
	static std::vector<std::filesystem::path> listFiles(const std::filesystem::path& directory, bool recursive);
	/**
	 * Resolves every import of static_data.xml.
	 * @throws StaticDataException for a missing file, a directory without singleRootTag or .xml files, or a malformed static_data.xml
	 */
	static std::vector<StaticDataImport> resolve(const std::filesystem::path& staticDataXml, int32_t countryCode, bool strict);
};

} // namespace aion::gameserver::xml
