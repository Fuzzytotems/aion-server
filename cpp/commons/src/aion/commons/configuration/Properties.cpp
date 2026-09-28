#include "aion/commons/configuration/Properties.h"

#include <chrono>
#include <format>
#include <istream>
#include <iterator>
#include <ostream>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::commons::configuration {

namespace {

/**
 * Port of java.util.Properties.LineReader: reads "logical lines" from the character buffer. A logical line is terminated by \n, \r or \r\n and
 * may span several natural lines when they end with an odd number of backslashes. Comment lines, blank lines and leading whitespace are skipped.
 */
class LineReader {
public:
	explicit LineReader(std::u16string_view in) noexcept : in(in) {}

	/** The characters of the last line read (valid until the next readLine call). */
	std::u16string lineBuf;

	/** @return the length of the logical line in lineBuf, or -1 at the end of input */
	std::ptrdiff_t readLine() {
		lineBuf.clear();
		bool skipWhiteSpace = true;
		bool appendedLineBegin = false;
		bool precedingBackslash = false;

		while (true) {
			if (off >= in.size()) {
				if (lineBuf.empty())
					return -1;
				return precedingBackslash ? static_cast<std::ptrdiff_t>(lineBuf.size()) - 1 : static_cast<std::ptrdiff_t>(lineBuf.size());
			}
			char16_t c = in[off++];

			if (skipWhiteSpace) {
				if (c == u' ' || c == u'\t' || c == u'\f')
					continue;
				if (!appendedLineBegin && (c == u'\r' || c == u'\n'))
					continue;
				skipWhiteSpace = false;
				appendedLineBegin = false;
			}
			if (lineBuf.empty()) { // still on a new logical line
				if (c == u'#' || c == u'!') {
					// comment, consume the rest of the natural line
					while (true) {
						if (off >= in.size())
							return -1;
						char16_t commentChar = in[off++];
						if (commentChar == u'\r' || commentChar == u'\n')
							break;
					}
					skipWhiteSpace = true;
					continue;
				}
			}

			if (c != u'\n' && c != u'\r') {
				lineBuf.push_back(c);
				// flip the preceding backslash flag
				precedingBackslash = c == u'\\' ? !precedingBackslash : false;
			} else {
				// reached EOL
				if (lineBuf.empty()) {
					skipWhiteSpace = true;
					continue;
				}
				if (off >= in.size())
					return precedingBackslash ? static_cast<std::ptrdiff_t>(lineBuf.size()) - 1 : static_cast<std::ptrdiff_t>(lineBuf.size());
				if (precedingBackslash) {
					// backslash at EOL is not part of the line
					lineBuf.pop_back();
					// skip leading whitespace characters in the following line
					skipWhiteSpace = true;
					appendedLineBegin = true;
					precedingBackslash = false;
					// take care not to include any subsequent \n
					if (c == u'\r' && in[off] == u'\n')
						off++;
				} else {
					return static_cast<std::ptrdiff_t>(lineBuf.size());
				}
			}
		}
	}

private:
	std::u16string_view in;
	std::size_t off = 0;
};

int hexDigitValue(char16_t c) noexcept {
	if (c >= u'0' && c <= u'9')
		return c - u'0';
	if (c >= u'a' && c <= u'f')
		return 10 + (c - u'a');
	if (c >= u'A' && c <= u'F')
		return 10 + (c - u'A');
	return -1;
}

/** Port of Properties.loadConvert: resolves the escape sequences \\uXXXX, \\t, \\r, \\n, \\f and \\x (any other char maps to itself). */
std::string loadConvert(std::u16string_view in) {
	if (in.find(u'\\') == std::u16string_view::npos)
		return utils::StringUtils::toUtf8(in);

	std::u16string out;
	out.reserve(in.size());
	std::size_t off = 0;
	std::size_t end = in.size();
	while (off < end) {
		char16_t c = in[off++];
		if (c != u'\\') {
			out.push_back(c);
			continue;
		}
		// No bounds check needed in Java since LineReader::readLine excludes unescaped backslashes at the end of the line. The key/value split
		// never separates a backslash from its escaped character either, but be defensive: a trailing lone backslash maps to nothing.
		if (off >= end)
			break;
		c = in[off++];
		if (c == u'u') {
			if (off + 4 > end)
				throw utils::IllegalArgumentException("Malformed \\uxxxx encoding.");
			int value = 0;
			for (int i = 0; i < 4; i++) {
				int digit = hexDigitValue(in[off++]);
				if (digit < 0)
					throw utils::IllegalArgumentException("Malformed \\uxxxx encoding.");
				value = (value << 4) + digit;
			}
			out.push_back(static_cast<char16_t>(value));
		} else {
			if (c == u't')
				c = u'\t';
			else if (c == u'r')
				c = u'\r';
			else if (c == u'n')
				c = u'\n';
			else if (c == u'f')
				c = u'\f';
			out.push_back(c);
		}
	}
	return utils::StringUtils::toUtf8(out);
}

std::string readAll(std::istream& in) {
	std::string data{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
	if (in.bad())
		throw utils::IOException("Could not read properties from stream");
	return data;
}

const char HEX_DIGITS[] = "0123456789ABCDEF";

void appendUnicodeEscape(std::u16string& out, char16_t c) {
	out += u"\\u";
	for (int shift = 12; shift >= 0; shift -= 4)
		out.push_back(static_cast<char16_t>(HEX_DIGITS[(c >> shift) & 0xF]));
}

/** Port of Properties.saveConvert. */
std::u16string saveConvert(std::u16string_view s, bool escapeSpace, bool escapeUnicode) {
	std::u16string out;
	out.reserve(s.size() * 2);
	for (std::size_t x = 0; x < s.size(); x++) {
		char16_t c = s[x];
		// handle common case first, selecting largest block that avoids the specials below
		if (c > 61 && c < 127) {
			if (c == u'\\')
				out += u"\\\\";
			else
				out.push_back(c);
			continue;
		}
		switch (c) {
			case u' ':
				if (x == 0 || escapeSpace)
					out.push_back(u'\\');
				out.push_back(u' ');
				break;
			case u'\t':
				out += u"\\t";
				break;
			case u'\n':
				out += u"\\n";
				break;
			case u'\r':
				out += u"\\r";
				break;
			case u'\f':
				out += u"\\f";
				break;
			case u'=':
			case u':':
			case u'#':
			case u'!':
				out.push_back(u'\\');
				out.push_back(c);
				break;
			default:
				if ((c < 0x0020 || c > 0x007e) && escapeUnicode)
					appendUnicodeEscape(out, c);
				else
					out.push_back(c);
		}
	}
	return out;
}

/** Writes UTF-16 text either as Latin-1 bytes (Java: OutputStream variant, all chars are <= 0xFF there) or as UTF-8 (Java: Writer variant). */
void write(std::ostream& out, std::u16string_view text, bool latin1) {
	if (latin1) {
		std::string bytes;
		bytes.reserve(text.size());
		for (char16_t c : text)
			bytes.push_back(static_cast<char>(c <= 0xFF ? c : u'?'));
		out << bytes;
	} else {
		out << utils::StringUtils::toUtf8(text);
	}
}

/** Port of Properties.writeComments. */
void writeComments(std::ostream& out, std::u16string_view comments, bool latin1) {
	std::u16string buffer = u"#";
	std::size_t len = comments.size();
	std::size_t current = 0;
	std::size_t last = 0;
	while (current < len) {
		char16_t c = comments[current];
		if (c > 0x00FF || c == u'\n' || c == u'\r') {
			if (last != current)
				buffer += comments.substr(last, current - last);
			if (c > 0x00FF) {
				appendUnicodeEscape(buffer, c);
			} else {
				buffer.push_back(u'\n');
				if (c == u'\r' && current != len - 1 && comments[current + 1] == u'\n')
					current++;
				if (current == len - 1 || (comments[current + 1] != u'#' && comments[current + 1] != u'!'))
					buffer.push_back(u'#');
			}
			last = current + 1;
		}
		current++;
	}
	if (last != current)
		buffer += comments.substr(last, current - last);
	buffer.push_back(u'\n');
	write(out, buffer, latin1);
}

/** Java: new Date().toString(), e.g. "Sat Sep 12 14:03:00 CEST 2026" */
std::string currentDateString() {
	auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
	try {
		return std::format("{:%a %b %d %H:%M:%S %Z %Y}", std::chrono::zoned_time(std::chrono::current_zone(), now));
	} catch (const std::exception&) {
		// no time zone database available
		return std::format("{:%a %b %d %H:%M:%S} UTC {:%Y}", now, now);
	}
}

} // namespace

void Properties::load(std::istream& in) {
	std::string bytes = readAll(in);
	std::u16string chars;
	chars.reserve(bytes.size());
	for (char b : bytes)
		chars.push_back(static_cast<char16_t>(static_cast<unsigned char>(b))); // ISO-8859-1 decoding
	load0(chars);
}

void Properties::loadUtf8(std::istream& reader) {
	loadUtf8(readAll(reader));
}

void Properties::loadUtf8(std::string_view text) {
	load0(utils::StringUtils::toUtf16(text));
}

void Properties::load0(std::u16string_view chars) {
	LineReader lr(chars);
	std::ptrdiff_t limit;
	while ((limit = lr.readLine()) >= 0) {
		std::u16string_view line(lr.lineBuf.data(), static_cast<std::size_t>(limit));
		std::size_t lineLength = line.size();
		std::size_t keyLen = 0;
		std::size_t valueStart = lineLength;
		bool hasSep = false;
		bool precedingBackslash = false;
		while (keyLen < lineLength) {
			char16_t c = line[keyLen];
			// need check if escaped
			if ((c == u'=' || c == u':') && !precedingBackslash) {
				valueStart = keyLen + 1;
				hasSep = true;
				break;
			} else if ((c == u' ' || c == u'\t' || c == u'\f') && !precedingBackslash) {
				valueStart = keyLen + 1;
				break;
			}
			precedingBackslash = c == u'\\' ? !precedingBackslash : false;
			keyLen++;
		}
		while (valueStart < lineLength) {
			char16_t c = line[valueStart];
			if (c != u' ' && c != u'\t' && c != u'\f') {
				if (!hasSep && (c == u'=' || c == u':'))
					hasSep = true;
				else
					break;
			}
			valueStart++;
		}
		std::string key = loadConvert(line.substr(0, keyLen));
		std::string value = loadConvert(line.substr(valueStart));
		table.insert_or_assign(std::move(key), std::move(value));
	}
}

void Properties::store(std::ostream& out, std::optional<std::string_view> comments) const {
	store0(out, comments, true);
}

void Properties::storeUtf8(std::ostream& out, std::optional<std::string_view> comments) const {
	store0(out, comments, false);
}

void Properties::store0(std::ostream& out, std::optional<std::string_view> comments, bool escUnicode) const {
	if (comments)
		writeComments(out, utils::StringUtils::toUtf16(*comments), escUnicode);
	out << '#' << currentDateString() << '\n';
	// Java (since JDK 18) writes the entries sorted by key, like our table. Note: UTF-8 byte order equals code point order, which differs from
	// Java's UTF-16 order only for supplementary characters versus U+E000..U+FFFF.
	for (const auto& [key, value] : table) {
		std::u16string line = saveConvert(utils::StringUtils::toUtf16(key), true, escUnicode);
		line.push_back(u'=');
		line += saveConvert(utils::StringUtils::toUtf16(value), false, escUnicode);
		line.push_back(u'\n');
		write(out, line, escUnicode);
	}
	out.flush();
	if (out.bad())
		throw utils::IOException("Could not write properties to stream");
}

std::optional<std::string> Properties::getProperty(std::string_view key) const {
	for (const Properties* p = this; p; p = p->defaults.get()) {
		if (auto it = p->table.find(key); it != p->table.end())
			return it->second;
	}
	return std::nullopt;
}

std::string Properties::getProperty(std::string_view key, std::string_view defaultValue) const {
	std::optional<std::string> value = getProperty(key);
	return value ? std::move(*value) : std::string(defaultValue);
}

std::optional<std::string> Properties::setProperty(std::string key, std::string value) {
	auto [it, inserted] = table.try_emplace(std::move(key));
	std::optional<std::string> previous;
	if (!inserted)
		previous = std::move(it->second);
	it->second = std::move(value);
	return previous;
}

void Properties::putAll(const Properties& other) {
	if (&other == this)
		return;
	for (const auto& [key, value] : other.table)
		table.insert_or_assign(key, value);
}

std::optional<std::string> Properties::remove(std::string_view key) {
	auto it = table.find(key);
	if (it == table.end())
		return std::nullopt;
	std::optional<std::string> previous = std::move(it->second);
	table.erase(it);
	return previous;
}

std::set<std::string> Properties::stringPropertyNames() const {
	std::set<std::string> names;
	collectNames(names);
	return names;
}

void Properties::collectNames(std::set<std::string>& names) const {
	for (const Properties* p = this; p; p = p->defaults.get()) {
		for (const auto& entry : p->table)
			names.insert(entry.first);
	}
}

} // namespace aion::commons::configuration
