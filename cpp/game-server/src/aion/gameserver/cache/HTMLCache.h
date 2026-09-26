#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/cache/fwd.h"

namespace aion::gameserver::cache {

/**
 * C++: a singleton (fieldmap K4, base Immortal; hub-headers.md §11.2) with the fieldmap members.
 * <ul>
 * <li>Deviation: Java's cache file is a serialized HashMap (ObjectOutputStream), which C++ cannot read or write; the port keeps the file and every
 * log line but uses its own format: the magic "AIONHTM1", then for each entry the UTF-8 key and value, each as a 4-byte little endian length plus
 * bytes (DEVIATIONS). A Java cache file is not a valid C++ one, so its first load logs the warning and rebuilds it, as Java does for a broken
 * file.</li>
 * <li>HTML_FILTER (an anonymous java.io.FileFilter) is the private function acceptHtmlFile; HTML_ROOT reads HTMLConfig on each use (its Java
 * static initializer runs after the configuration is loaded), TAGS_TO_COMPACT is a std::vector built at static initialization.</li>
 * <li>Sizes are String.length() values (UTF-16 code units); files are read as UTF-8 whatever HTMLConfig.HTML_ENCODING names (DEVIATIONS).</li>
 * </ul>
 *
 * @author Layane, nbali, savormix, hex1r0, lord_rex
 */
class HTMLCache final : public runtime::Immortal {
public:
	static HTMLCache& getInstance();

private:
	runtime::Field<runtime::Ref<runtime::RcHashMap<std::string, std::string>>> cache{};
	runtime::Field<int32_t> loadedFiles{};
	runtime::Field<int32_t> size{};

	HTMLCache();

public:
	/** synchronized */
	void reload(bool deleteCacheFile);

private:
	std::filesystem::path getCacheFile();

	// fieldmap: filled once by the static initializer and only read afterwards; a std::vector avoids a RefCounted Array at static initialization
	static const std::vector<std::string> TAGS_TO_COMPACT;

	/** Java: compactHtml(StringBuilder sb, String html), the builder being a local of the caller */
	std::string compactHtml(std::string_view html);

	/** Java: replaceAll(StringBuilder sb, String pattern, String value) on the UTF-16 builder */
	void replaceAll(std::u16string& sb, std::u16string_view pattern, std::u16string_view value);

public:
	void reloadPath(const std::filesystem::path& f);

	void parseDir(const std::filesystem::path& dir);

	/** @return the content, null (std::nullopt) if the file is not loadable or could not be read */
	std::optional<std::string> loadFile(const std::filesystem::path& file);

	/** @return the html, null (std::nullopt) if there is none for the path */
	std::optional<std::string> getHTML(std::string_view path);

private:
	bool isLoadable(const std::filesystem::path& file);

public:
	bool pathExists(std::string_view path);

	std::string toString();

	/** Java: file.toURI().getPath().substring(base.toURI().getPath().length()) - the '/' separated path of file below base */
	static std::string getRelativePath(const std::filesystem::path& base, const std::filesystem::path& file);

private:
	/** C++ only: Java HTML_FILTER.accept(file) - directories and *.xhtml files */
	static bool acceptHtmlFile(const std::filesystem::path& file);

	/** C++ only: Java new File(HTMLConfig.HTML_ROOT) */
	static std::filesystem::path htmlRoot();

	/** C++ only: reads the cache file (see the class comment) into a new map, @throws IOException if it is not a valid cache file */
	static runtime::Ref<runtime::RcHashMap<std::string, std::string>> readCacheFile(const std::filesystem::path& file);

	/** C++ only: writes the cache file (see the class comment), @throws IOException */
	static void writeCacheFile(const std::filesystem::path& file, runtime::RcHashMap<std::string, std::string>& map);
};

} // namespace aion::gameserver::cache
