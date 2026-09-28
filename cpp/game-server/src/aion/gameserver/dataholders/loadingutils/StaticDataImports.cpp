#include "aion/gameserver/dataholders/loadingutils/StaticDataImports.h"

#include <algorithm>
#include <system_error>
#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataException.h"
#include "aion/gameserver/dataholders/loadingutils/XmlDocument.h"
#include "aion/gameserver/dataholders/loadingutils/XmlValues.h"

namespace aion::gameserver::xml {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger =
	  new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dataholders.loadingutils.XmlMerger"));
	return *logger;
}

/** Java Boolean.parseBoolean */
bool parseJavaBoolean(std::string_view value) noexcept {
	return commons::utils::StringUtils::equalsIgnoreCase(value, "true");
}

bool endsWithXml(const std::filesystem::path& fileName) {
	std::string name = fileName.generic_string();
	if (name.size() < 4)
		return false;
	std::string suffix = name.substr(name.size() - 4);
	std::ranges::transform(suffix, suffix.begin(), [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; });
	return suffix == ".xml";
}

void walk(const std::filesystem::path& directory, bool recursive, std::vector<std::filesystem::path>& out) {
	struct Entry {
		std::u16string key;
		std::filesystem::path path;
		std::filesystem::file_status status;
	};
	std::vector<Entry> entries;
	std::error_code error;
	std::filesystem::directory_iterator it(directory, error);
	if (error)
		throw StaticDataException("Cannot list directory " + directory.generic_string() + ": " + error.message());
	for (std::filesystem::directory_iterator end; it != end; it.increment(error)) {
		std::filesystem::file_status status = it->symlink_status(error);
		if (error)
			throw StaticDataException("Cannot read " + it->path().generic_string() + ": " + error.message());
		entries.push_back(Entry{StaticDataImports::orderKey(it->path().filename()), it->path(), status});
	}
	if (error)
		throw StaticDataException("Cannot list directory " + directory.generic_string() + ": " + error.message());
	std::ranges::sort(entries, [](const Entry& a, const Entry& b) {
		if (a.key != b.key)
			return a.key < b.key;
		return a.path.filename().u16string() < b.path.filename().u16string(); // case-sensitive directories only
	});
	for (const Entry& entry : entries) {
		if (std::filesystem::is_regular_file(entry.status)) {
			if (endsWithXml(entry.path.filename()))
				out.push_back(entry.path);
		} else if (recursive && std::filesystem::is_directory(entry.status)) {
			walk(entry.path, recursive, out);
		}
	}
}

bool isIgnoredAttribute(std::string_view name) {
	return name == "xmlns" || name.starts_with("xmlns:") || name.starts_with("xsi:");
}

} // namespace

std::string_view StaticDataImports::countryRegion(int32_t countryCode) noexcept {
	switch (countryCode) {
		case 1:
			return "usa";
		case 2:
			return "europe";
		case 4:
			return "japan";
		case 5:
			return "china";
		case 6:
			return "taiwan";
		case 7:
			return "russia";
		default:
			return {};
	}
}

std::filesystem::path StaticDataImports::applyCountryOverride(const std::filesystem::path& file, int32_t countryCode) {
	std::string_view region = countryRegion(countryCode);
	if (region.empty())
		return file;
	std::u8string name = file.filename().u8string();
	size_t dot = name.rfind(u8'.');
	std::u8string base = dot == std::u8string::npos ? name : name.substr(0, dot);
	std::u8string extension = dot == std::u8string::npos ? std::u8string() : name.substr(dot);
	std::u8string regionName(region.begin(), region.end());
	std::filesystem::path override = file.parent_path() / (base + u8'_' + regionName + extension);
	std::error_code error;
	return std::filesystem::is_regular_file(override, error) ? override : file;
}

std::u16string StaticDataImports::orderKey(const std::filesystem::path& fileName) {
	std::u16string key = fileName.u16string();
	for (char16_t& unit : key) {
		if (unit >= 0xD800 && unit <= 0xDFFF)
			continue;
		char32_t upper = commons::utils::StringUtils::toUpperCase(static_cast<char32_t>(unit));
		if (upper <= 0xFFFF)
			unit = static_cast<char16_t>(upper);
	}
	return key;
}

std::vector<std::filesystem::path> StaticDataImports::listFiles(const std::filesystem::path& directory, bool recursive) {
	std::vector<std::filesystem::path> files;
	walk(directory, recursive, files);
	return files;
}

std::vector<StaticDataImport> StaticDataImports::resolve(const std::filesystem::path& staticDataXml, int32_t countryCode, bool strict) {
	std::unique_ptr<XmlDocument> document = XmlDocument::parseFile(staticDataXml);
	pugi::xml_node root = document->root();
	auto failAt = [&](pugi::xml_node node, const std::string& message) -> StaticDataException {
		return StaticDataException(document->describe(document->locate(node)) + ": " + message);
	};
	auto checkAttribute = [&](pugi::xml_node node, pugi::xml_attribute attribute) {
		std::string message = "Unknown attribute '" + std::string(attribute.name()) + "' on <" + node.name() + ">";
		if (strict)
			throw StaticDataException(document->describe(document->locate(attribute)) + ": " + message);
		log().warn(document->describe(document->locate(attribute)) + ": " + message);
	};
	if (std::string_view(root.name()) != "static_data")
		throw failAt(root, "Root element must be <static_data>, found <" + std::string(root.name()) + ">");
	for (pugi::xml_attribute attribute = root.first_attribute(); attribute; attribute = attribute.next_attribute()) {
		if (!isIgnoredAttribute(attribute.name()))
			checkAttribute(root, attribute);
	}

	std::filesystem::path baseDirectory = staticDataXml.parent_path();
	std::vector<StaticDataImport> imports;
	for (pugi::xml_node child = root.first_child(); child; child = child.next_sibling()) {
		if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata) {
			if (!trimXml(child.value()).empty())
				throw failAt(child, "Unexpected text in <static_data>");
			continue;
		}
		if (child.type() != pugi::node_element)
			continue;
		if (std::string_view(child.name()) != "import")
			throw failAt(child, "Unsupported element <" + std::string(child.name()) + "> in static_data.xml (only <import> is supported)");

		StaticDataImport entry;
		entry.line = document->locate(child).line;
		std::optional<std::string> fileAttribute;
		std::optional<std::string> singleRootTag;
		std::optional<std::string> recursiveImport;
		for (pugi::xml_attribute attribute = child.first_attribute(); attribute; attribute = attribute.next_attribute()) {
			std::string_view name = attribute.name();
			if (name == "file")
				fileAttribute = attribute.value();
			else if (name == "singleRootTag")
				singleRootTag = attribute.value();
			else if (name == "recursiveImport")
				recursiveImport = attribute.value();
			else if (!isIgnoredAttribute(name))
				checkAttribute(child, attribute);
		}
		if (!fileAttribute)
			throw failAt(child, "Attribute 'file' is missing or empty.");
		entry.file = *fileAttribute;
		std::string relative = *fileAttribute;
		while (relative.size() > 1 && (relative.back() == '/' || relative.back() == '\\'))
			relative.pop_back();
		std::filesystem::path path = baseDirectory / std::filesystem::path(std::u8string(relative.begin(), relative.end()));
		path = applyCountryOverride(path, countryCode);
		entry.path = path;

		std::error_code error;
		std::filesystem::file_status status = std::filesystem::status(path, error);
		if (!std::filesystem::exists(status))
			throw failAt(child, "Missing file to import: " + path.generic_string());
		if (std::filesystem::is_regular_file(status)) {
			entry.files.push_back(path);
		} else if (std::filesystem::is_directory(status)) {
			entry.directory = true;
			entry.singleRootTag = parseJavaBoolean(singleRootTag.value_or("false"));
			entry.recursive = parseJavaBoolean(recursiveImport.value_or("true"));
			if (!entry.singleRootTag)
				throw failAt(child, "Directory import " + path.generic_string() + " requires singleRootTag=\"true\"");
			entry.files = listFiles(path, entry.recursive);
			if (entry.files.empty())
				throw failAt(child, "Directory import " + path.generic_string() + " contains no .xml files");
		} else {
			throw failAt(child, "Import " + path.generic_string() + " is neither a regular file nor a directory");
		}
		log().debug("Import {} ({} file(s))", path.generic_string(), entry.files.size());
		imports.push_back(std::move(entry));
	}
	return imports;
}

} // namespace aion::gameserver::xml
