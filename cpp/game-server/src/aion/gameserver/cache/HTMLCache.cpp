#include "aion/gameserver/cache/HTMLCache.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <iterator>
#include <system_error>

#include "aion/commons/logging/Logger.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/main/HTMLConfig.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::cache {

namespace {

namespace StringUtils = commons::utils::StringUtils;

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.cache.HTMLCache"));
	return *logger;
}

constexpr std::string_view CACHE_MAGIC = "AIONHTM1";

std::string utf8(const std::filesystem::path& path) {
	std::u8string text = path.u8string();
	return std::string(text.begin(), text.end());
}

std::string genericUtf8(const std::filesystem::path& path) {
	std::u8string text = path.generic_u8string();
	return std::string(text.begin(), text.end());
}

/** Java: Character.isWhitespace(char) */
bool isWhitespace(char16_t c) noexcept {
	switch (static_cast<uint32_t>(c)) {
		case 0x0009: // tab
		case 0x000A:
		case 0x000B:
		case 0x000C:
		case 0x000D:
		case 0x001C:
		case 0x001D:
		case 0x001E:
		case 0x001F:
		case 0x0020: // space separators without the non-breaking spaces 00A0, 2007 and 202F
		case 0x1680:
		case 0x205F:
		case 0x3000:
		case 0x2028: // line separator
		case 0x2029: // paragraph separator
			return true;
		default:
			return c >= 0x2000 && c <= 0x200A && c != 0x2007;
	}
}

/** Java: String.format("%.3f", value) (Locale.US digits): the shortest decimal of the double rounded HALF_UP to three fraction digits */
std::string formatThreeDecimals(double value) {
	char shortest[64];
	auto [end, error] = std::to_chars(shortest, shortest + sizeof(shortest), value, std::chars_format::fixed);
	std::string digits(shortest, end);
	bool negative = !digits.empty() && digits[0] == '-';
	if (negative)
		digits.erase(0, 1);
	size_t point = digits.find('.');
	std::string integerPart = point == std::string::npos ? digits : digits.substr(0, point);
	std::string fraction = point == std::string::npos ? std::string() : digits.substr(point + 1);
	fraction.resize(std::max<size_t>(fraction.size(), 4), '0');
	bool roundUp = fraction[3] >= '5';
	std::string number = integerPart + fraction.substr(0, 3);
	if (roundUp) {
		size_t i = number.size();
		while (i > 0) {
			--i;
			if (number[i] == '9') {
				number[i] = '0';
				if (i == 0)
					number.insert(number.begin(), '1');
			} else {
				++number[i];
				break;
			}
		}
	}
	return std::string(negative ? "-" : "") + number.substr(0, number.size() - 3) + "." + number.substr(number.size() - 3);
}

void writeLength(std::ofstream& out, size_t length) {
	uint32_t value = static_cast<uint32_t>(length);
	for (int shift = 0; shift < 32; shift += 8)
		out.put(static_cast<char>((value >> shift) & 0xFF));
}

bool readLength(std::ifstream& in, uint32_t& length) {
	length = 0;
	for (int shift = 0; shift < 32; shift += 8) {
		int c = in.get();
		if (c == std::char_traits<char>::eof())
			return false;
		length |= static_cast<uint32_t>(static_cast<uint8_t>(c)) << shift;
	}
	return true;
}

std::vector<std::string> buildTagsToCompact() {
	// TODO: is there any other tag that should be replaced?
	const std::vector<std::string> tagsToCompact{"html", "title", "body", "br", "br1", "p", "table", "tr", "td"};
	std::vector<std::string> list;
	for (const std::string& tag : tagsToCompact) {
		list.push_back("<" + tag + ">");
		list.push_back("</" + tag + ">");
		list.push_back("<" + tag + "/>");
		list.push_back("<" + tag + " />");
	}
	std::vector<std::string> list2;
	for (const std::string& tag : list) {
		list2.push_back(tag);
		list2.push_back(tag + " ");
		list2.push_back(" " + tag);
	}
	return list2;
}

} // namespace

const std::vector<std::string> HTMLCache::TAGS_TO_COMPACT = buildTagsToCompact();

HTMLCache& HTMLCache::getInstance() {
	static HTMLCache instance; // Java: SingletonHolder
	return instance;
}

HTMLCache::HTMLCache() : cache(runtime::RcHashMap<std::string, std::string>::create(AION_LOCK_CLASS(HTMLCache::cache))) {
	reload(false);
}

std::filesystem::path HTMLCache::htmlRoot() {
	std::string root = *configs::main::HTMLConfig::HTML_ROOT.get();
	return std::filesystem::path(std::u8string(root.begin(), root.end()));
}

void HTMLCache::reload(bool deleteCacheFile) {
	SYNCHRONIZED(*this) {
		cache.get()->clear();
		loadedFiles = 0;
		size = 0;

		const std::filesystem::path cacheFile = getCacheFile();
		std::error_code error;
		if (deleteCacheFile && std::filesystem::exists(cacheFile, error)) {
			log().info("Cache[HTML]: Deleting cache file... OK.");
			std::filesystem::remove(cacheFile, error);
		}

		log().info("Cache[HTML]: Caching started... OK.");
		if (std::filesystem::exists(cacheFile, error)) {
			log().info("Cache[HTML]: Using cache file... OK.");
			try {
				cache = readCacheFile(cacheFile);
				for (const auto& entry : cache.get()->snapshot()) {
					loadedFiles += 1;
					size += StringUtils::utf16Length(entry.value);
				}
			} catch (const std::exception& e) {
				log().warn("", e);
				reload(true);
				return;
			}
		} else {
			parseDir(htmlRoot());
		}

		log().info(toString());

		if (std::filesystem::exists(cacheFile, error)) {
			log().info("Cache[HTML]: Compaction skipped!");
		} else {
			log().info("Cache[HTML]: Compacting htmls... OK.");
			runtime::Ptr<runtime::RcHashMap<std::string, std::string>> map = cache.get();
			for (const auto& entry : map->snapshot()) {
				try {
					const std::string& oldHtml = entry.value;
					std::string newHtml = compactHtml(oldHtml);
					size -= StringUtils::utf16Length(oldHtml);
					size += StringUtils::utf16Length(newHtml);
					map->put(entry.key, std::move(newHtml));
				} catch (const std::exception& e) {
					log().warn("Cache[HTML]: Error during compaction of " + entry.key, e);
				}
			}
			log().info(toString());
		}

		if (!std::filesystem::exists(cacheFile, error)) {
			log().info("Cache[HTML]: Creating cache file... OK.");
			try {
				writeCacheFile(cacheFile, *cache.get());
			} catch (const std::exception& e) {
				log().warn("", e);
			}
		}
	}
}

std::filesystem::path HTMLCache::getCacheFile() {
	std::string file = *configs::main::HTMLConfig::HTML_CACHE_FILE.get();
	return std::filesystem::path(std::u8string(file.begin(), file.end()));
}

std::string HTMLCache::compactHtml(std::string_view html) {
	std::u16string sb = StringUtils::toUtf16(html);
	for (char16_t& c : sb) {
		if (isWhitespace(c))
			c = u' ';
	}
	replaceAll(sb, u"  ", u" ");
	replaceAll(sb, u"< ", u"<");
	replaceAll(sb, u" >", u">");
	for (size_t i = 0; i < TAGS_TO_COMPACT.size(); i += 3) {
		std::u16string tag = StringUtils::toUtf16(TAGS_TO_COMPACT[i]);
		replaceAll(sb, StringUtils::toUtf16(TAGS_TO_COMPACT[i + 1]), tag);
		replaceAll(sb, StringUtils::toUtf16(TAGS_TO_COMPACT[i + 2]), tag);
	}
	replaceAll(sb, u"  ", u" ");
	// String.trim() without additional garbage
	size_t fromIndex = 0;
	size_t toIndex = sb.size();
	while (fromIndex < toIndex && sb[fromIndex] == u' ')
		fromIndex++;
	while (fromIndex < toIndex && sb[toIndex - 1] == u' ')
		toIndex--;
	return StringUtils::toUtf8(std::u16string_view(sb).substr(fromIndex, toIndex - fromIndex));
}

void HTMLCache::replaceAll(std::u16string& sb, std::u16string_view pattern, std::u16string_view value) {
	for (size_t index = 0; (index = sb.find(pattern, index)) != std::u16string::npos;)
		sb.replace(index, pattern.size(), value);
}

void HTMLCache::reloadPath(const std::filesystem::path& f) {
	parseDir(f);
	log().info("Cache[HTML]: Reloaded specified path.");
}

void HTMLCache::parseDir(const std::filesystem::path& dir) {
	std::error_code error;
	std::filesystem::directory_iterator files(dir, error);
	if (error) // Java: dir.listFiles(HTML_FILTER) returns null for a missing directory, and the for loop throws
		throw runtime::NullPointerException("Cannot read the array length because \"<local1>\" is null");
	for (const std::filesystem::directory_entry& entry : files) {
		const std::filesystem::path& file = entry.path();
		if (!acceptHtmlFile(file))
			continue;
		if (!std::filesystem::is_directory(file, error))
			loadFile(file);
		else
			parseDir(file);
	}
}

std::optional<std::string> HTMLCache::loadFile(const std::filesystem::path& file) {
	if (isLoadable(file)) {
		try {
			std::ifstream in(file, std::ios::binary);
			if (!in)
				throw commons::utils::IOException("java.io.FileNotFoundException: " + utf8(file));
			std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
			std::string relpath = getRelativePath(htmlRoot(), file);
			size += StringUtils::utf16Length(content);
			runtime::Ptr<runtime::RcHashMap<std::string, std::string>> map = cache.get();
			std::optional<std::string> oldContent = map->get(relpath);
			if (!oldContent)
				loadedFiles += 1;
			else
				size -= StringUtils::utf16Length(*oldContent);
			map->put(relpath, content);
			return content;
		} catch (const std::exception& e) {
			log().warn("Problem with htm file:", e);
		}
	}
	return std::nullopt;
}

std::optional<std::string> HTMLCache::getHTML(std::string_view path) {
	return cache.get()->get(std::string(path));
}

bool HTMLCache::isLoadable(const std::filesystem::path& file) {
	std::error_code error;
	return std::filesystem::exists(file, error) && !std::filesystem::is_directory(file, error) && acceptHtmlFile(file);
}

bool HTMLCache::pathExists(std::string_view path) {
	return cache.get()->containsKey(std::string(path));
}

std::string HTMLCache::toString() {
	return "Cache[HTML]: " + formatThreeDecimals(static_cast<float>(size.get()) / 1024.0f) + " kilobytes on " + std::to_string(loadedFiles.get()) +
		" file(s) loaded.";
}

std::string HTMLCache::getRelativePath(const std::filesystem::path& base, const std::filesystem::path& file) {
	std::string basePath = genericUtf8(std::filesystem::absolute(base).lexically_normal());
	std::error_code error;
	if (std::filesystem::is_directory(base, error) && !basePath.ends_with('/'))
		basePath += '/'; // Java: File.toURI() ends a directory's path with '/'
	std::string filePath = genericUtf8(std::filesystem::absolute(file).lexically_normal());
	if (basePath.size() > filePath.size())
		throw runtime::IndexOutOfBoundsException("begin " + std::to_string(basePath.size()) + ", end " + std::to_string(filePath.size()));
	return filePath.substr(basePath.size());
}

bool HTMLCache::acceptHtmlFile(const std::filesystem::path& file) {
	std::error_code error;
	return std::filesystem::is_directory(file, error) || utf8(file.filename()).ends_with(".xhtml");
}

runtime::Ref<runtime::RcHashMap<std::string, std::string>> HTMLCache::readCacheFile(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	if (!in)
		throw commons::utils::IOException("java.io.FileNotFoundException: " + utf8(file));
	std::string magic(CACHE_MAGIC.size(), '\0');
	if (!in.read(magic.data(), static_cast<std::streamsize>(magic.size())) || magic != CACHE_MAGIC)
		throw commons::utils::IOException("java.io.StreamCorruptedException: invalid stream header in " + utf8(file));
	runtime::Ref<runtime::RcHashMap<std::string, std::string>> map =
		runtime::RcHashMap<std::string, std::string>::create(AION_LOCK_CLASS(HTMLCache::cache));
	for (;;) {
		uint32_t keyLength;
		if (!readLength(in, keyLength))
			break;
		std::string key(keyLength, '\0');
		uint32_t valueLength;
		if (!in.read(key.data(), keyLength) || !readLength(in, valueLength))
			throw commons::utils::IOException("java.io.EOFException: " + utf8(file));
		std::string value(valueLength, '\0');
		if (!in.read(value.data(), valueLength))
			throw commons::utils::IOException("java.io.EOFException: " + utf8(file));
		map->put(std::move(key), std::move(value));
	}
	return map;
}

void HTMLCache::writeCacheFile(const std::filesystem::path& file, runtime::RcHashMap<std::string, std::string>& map) {
	std::ofstream out(file, std::ios::binary | std::ios::trunc);
	if (!out)
		throw commons::utils::IOException("java.io.FileNotFoundException: " + utf8(file));
	out.write(CACHE_MAGIC.data(), static_cast<std::streamsize>(CACHE_MAGIC.size()));
	for (const auto& entry : map.snapshot()) {
		writeLength(out, entry.key.size());
		out.write(entry.key.data(), static_cast<std::streamsize>(entry.key.size()));
		writeLength(out, entry.value.size());
		out.write(entry.value.data(), static_cast<std::streamsize>(entry.value.size()));
	}
	if (!out)
		throw commons::utils::IOException("java.io.IOException: could not write " + utf8(file));
}

} // namespace aion::gameserver::cache
