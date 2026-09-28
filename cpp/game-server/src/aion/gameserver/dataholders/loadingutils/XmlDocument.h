#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <pugixml.hpp>

namespace aion::gameserver::xml {

/** 1-based line and column (in bytes) of a position in an XML file; 0/0 if unknown. */
struct XmlLocation {
	uint32_t line = 0;
	uint32_t column = 0;

	bool known() const noexcept { return line != 0; }
};

/**
 * One parsed XML file (docs/design/static-data.md §3.2): the file bytes, the pugixml DOM parsed in place over them and a line index for
 * error locations.
 *
 * Parsing uses pugi::parse_default (character references and entities, EOL normalization, attribute whitespace normalization as the XML
 * spec and StAX do, CDATA kept; comments, processing instructions and whitespace-only text dropped) with UTF-8 input. Files with a UTF-16/32
 * byte order mark or an XML declaration naming another encoding are rejected (all static data files are UTF-8).
 *
 * Every node and attribute name points into the owned buffer, so `locate` maps it back to line:column. The line index is built before
 * parsing because in-place parsing rewrites the buffer (about 4 bytes per line).
 *
 * Thread-safety: parsing may run on any thread (the loader parses the files of one import in parallel); a parsed document is then used by
 * one binding thread. Immutable while bound.
 */
class XmlDocument {
public:
	/** Reads and parses a file. @throws StaticDataException (I/O error or `file:line:col: parse error`) */
	static std::unique_ptr<XmlDocument> parseFile(const std::filesystem::path& path);
	/** Parses text (tests, generated binder tests); `displayName` is used in locations. @throws StaticDataException */
	static std::unique_ptr<XmlDocument> parseString(std::string_view text, std::string displayName = "<memory>");

	XmlDocument(const XmlDocument&) = delete;
	XmlDocument& operator=(const XmlDocument&) = delete;
	~XmlDocument();

	/** the file path (empty for parseString) */
	const std::filesystem::path& path() const noexcept { return filePath; }
	/** generic path string or the display name, used in messages */
	const std::string& displayName() const noexcept { return name; }
	/** the document element (root); never empty for a successfully parsed document */
	pugi::xml_node root() const noexcept { return document.document_element(); }
	/** size of the file in bytes */
	size_t size() const noexcept { return bufferSize; }

	/** location of a pointer into the document buffer (a node or attribute name, a text value), 0/0 if it is not inside the buffer */
	XmlLocation locate(const char* position) const noexcept;
	/** location of an element (its name) or text node (its value) */
	XmlLocation locate(pugi::xml_node node) const noexcept;
	/** location of an attribute (its name) */
	XmlLocation locate(pugi::xml_attribute attribute) const noexcept;
	/** "file:line:col" (or just the file name if the location is unknown) */
	std::string describe(XmlLocation location) const;

private:
	XmlDocument(std::unique_ptr<char[]> buffer, size_t size, std::filesystem::path path, std::string displayName);
	void parse();
	XmlLocation locateOffset(size_t offset) const noexcept;

	std::unique_ptr<char[]> buffer;
	size_t bufferSize;
	std::filesystem::path filePath;
	std::string name;
	/** offsets of the first byte of every line */
	std::vector<uint32_t> lineStarts;
	pugi::xml_document document;
};

} // namespace aion::gameserver::xml
