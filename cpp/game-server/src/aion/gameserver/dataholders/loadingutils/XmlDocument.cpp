#include "aion/gameserver/dataholders/loadingutils/XmlDocument.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <system_error>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataException.h"

namespace aion::gameserver::xml {

namespace {

bool startsWith(const char* data, size_t size, std::string_view prefix) noexcept {
	return size >= prefix.size() && std::memcmp(data, prefix.data(), prefix.size()) == 0;
}

/** Rejects byte order marks and XML declarations of encodings other than UTF-8 (pugixml would silently convert or misparse them). */
void checkEncoding(const char* data, size_t size, const std::string& displayName) {
	using namespace std::string_view_literals;
	if (startsWith(data, size, "\xFE\xFF"sv) || startsWith(data, size, "\xFF\xFE"sv) || startsWith(data, size, "\x00\x00\xFE\xFF"sv))
		throw StaticDataException(displayName + ": only UTF-8 encoded XML is supported (found a UTF-16/32 byte order mark)");
	std::string_view text(data, size);
	if (text.starts_with("\xEF\xBB\xBF"sv))
		text.remove_prefix(3);
	if (!text.starts_with("<?xml"))
		return;
	size_t end = text.find("?>");
	std::string_view declaration = text.substr(0, end == std::string_view::npos ? text.size() : end);
	size_t pos = declaration.find("encoding");
	if (pos == std::string_view::npos)
		return;
	pos = declaration.find_first_of("\"'", pos);
	if (pos == std::string_view::npos)
		return;
	size_t close = declaration.find(declaration[pos], pos + 1);
	if (close == std::string_view::npos)
		return;
	std::string_view encoding = declaration.substr(pos + 1, close - pos - 1);
	if (!commons::utils::StringUtils::equalsIgnoreCase(encoding, "UTF-8") && !commons::utils::StringUtils::equalsIgnoreCase(encoding, "UTF8"))
		throw StaticDataException(displayName + ": only UTF-8 encoded XML is supported (declared encoding \"" + std::string(encoding) + "\")");
}

} // namespace

XmlDocument::XmlDocument(std::unique_ptr<char[]> buffer, size_t size, std::filesystem::path path, std::string displayName)
    : buffer(std::move(buffer)), bufferSize(size), filePath(std::move(path)), name(std::move(displayName)) {}

XmlDocument::~XmlDocument() = default;

std::unique_ptr<XmlDocument> XmlDocument::parseFile(const std::filesystem::path& path) {
	std::string displayName = path.generic_string();
	std::ifstream in(path, std::ios::binary);
	if (!in)
		throw StaticDataException("Cannot open " + displayName);
	std::error_code error;
	uintmax_t fileSize = std::filesystem::file_size(path, error);
	if (error)
		throw StaticDataException("Cannot read the size of " + displayName + ": " + error.message());
	if (fileSize >= std::numeric_limits<uint32_t>::max())
		throw StaticDataException(displayName + " is too large (" + std::to_string(fileSize) + " bytes)");
	auto size = static_cast<size_t>(fileSize);
	auto buffer = std::make_unique_for_overwrite<char[]>(size == 0 ? 1 : size);
	if (size > 0 && !in.read(buffer.get(), static_cast<std::streamsize>(size)))
		throw StaticDataException("Cannot read " + displayName);
	std::unique_ptr<XmlDocument> document(new XmlDocument(std::move(buffer), size, path, std::move(displayName)));
	document->parse();
	return document;
}

std::unique_ptr<XmlDocument> XmlDocument::parseString(std::string_view text, std::string displayName) {
	if (text.size() >= std::numeric_limits<uint32_t>::max())
		throw StaticDataException(displayName + " is too large");
	auto buffer = std::make_unique_for_overwrite<char[]>(text.empty() ? 1 : text.size());
	if (!text.empty())
		std::memcpy(buffer.get(), text.data(), text.size());
	std::unique_ptr<XmlDocument> document(new XmlDocument(std::move(buffer), text.size(), {}, std::move(displayName)));
	document->parse();
	return document;
}

void XmlDocument::parse() {
	const char* data = buffer.get();
	checkEncoding(data, bufferSize, name);
	lineStarts.clear();
	lineStarts.push_back(0);
	for (const char *p = data, *end = data + bufferSize; p < end;) {
		const auto* newline = static_cast<const char*>(std::memchr(p, '\n', static_cast<size_t>(end - p)));
		if (newline == nullptr)
			break;
		lineStarts.push_back(static_cast<uint32_t>(newline + 1 - data));
		p = newline + 1;
	}
	pugi::xml_parse_result result = document.load_buffer_inplace(buffer.get(), bufferSize, pugi::parse_default, pugi::encoding_utf8);
	if (!result) {
		XmlLocation location = result.offset >= 0 ? locateOffset(static_cast<size_t>(result.offset)) : XmlLocation{};
		throw StaticDataException(describe(location) + ": XML parse error: " + result.description());
	}
	if (!document.document_element())
		throw StaticDataException(name + ": XML document has no root element");
}

XmlLocation XmlDocument::locateOffset(size_t offset) const noexcept {
	if (offset > bufferSize || lineStarts.empty())
		return {};
	auto next = std::upper_bound(lineStarts.begin(), lineStarts.end(), static_cast<uint32_t>(offset));
	auto line = static_cast<size_t>(next - lineStarts.begin()); // >= 1, since lineStarts[0] == 0
	return XmlLocation{static_cast<uint32_t>(line), static_cast<uint32_t>(offset - lineStarts[line - 1] + 1)};
}

XmlLocation XmlDocument::locate(const char* position) const noexcept {
	const char* data = buffer.get();
	if (position == nullptr || position < data || position > data + bufferSize)
		return {};
	return locateOffset(static_cast<size_t>(position - data));
}

XmlLocation XmlDocument::locate(pugi::xml_node node) const noexcept {
	if (!node)
		return {};
	if (node.type() == pugi::node_element)
		return locate(node.name());
	return locate(node.value());
}

XmlLocation XmlDocument::locate(pugi::xml_attribute attribute) const noexcept {
	return attribute ? locate(attribute.name()) : XmlLocation{};
}

std::string XmlDocument::describe(XmlLocation location) const {
	if (!location.known())
		return name;
	return name + ":" + std::to_string(location.line) + ":" + std::to_string(location.column);
}

} // namespace aion::gameserver::xml
