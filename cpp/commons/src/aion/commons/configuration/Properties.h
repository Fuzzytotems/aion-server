#pragma once

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace aion::commons::configuration {

/**
 * A persistent set of string key/value pairs with the exact loading rules of the Java .properties format, and an optional chain of default
 * properties that is searched when a key is not found in this object.
 * <p>
 * Keys and values are stored as UTF-8. load(std::istream&) decodes the input as ISO-8859-1 (like Java's load(InputStream)), while loadUtf8 treats
 * the input as already decoded text (like Java's load(Reader)).
 * <p>
 * Deviations from Java:
 * <ul>
 * <li>Not internally synchronized (Java's Properties is a Hashtable). Guard concurrent modification externally, like with standard containers.</li>
 * <li>The defaults are shared immutably (std::shared_ptr&lt;const Properties&gt;) instead of being a live reference to a mutable object.</li>
 * <li>Unpaired surrogates produced by \\uXXXX escapes cannot be represented in UTF-8 and become U+FFFD.</li>
 * <li>Only a subset of the Hashtable/Map API is provided (no values(), entrySet() views etc.; use entries()).</li>
 * </ul>
 * Java: java.util.Properties
 */
class Properties {
public:
	/** Own entries, sorted by key (std::less&lt;&gt; allows lookups by std::string_view). */
	using Map = std::map<std::string, std::string, std::less<>>;

	/** Creates an empty property list with no default values. */
	Properties() = default;

	/** Creates an empty property list with the specified defaults (may be nullptr). Java: new Properties(defaults) */
	explicit Properties(std::shared_ptr<const Properties> defaults) noexcept : defaults(std::move(defaults)) {}

	/**
	 * Reads a property list from a byte stream, decoding it as ISO-8859-1 (every byte is one character). Characters outside Latin-1 can only be
	 * given as \\uXXXX escapes. Reads until the end of the stream.
	 *
	 * @throws utils::IOException if reading from the stream fails
	 * @throws utils::IllegalArgumentException if a malformed \\uXXXX escape occurs ("Malformed \\uxxxx encoding.")
	 */
	void load(std::istream& in);

	/** Java: load(Reader) with a UTF-8 reader. Invalid UTF-8 sequences are replaced with U+FFFD. Otherwise like load(std::istream&). */
	void loadUtf8(std::istream& reader);

	/** Java: load(new StringReader(text)). The text is UTF-8. */
	void loadUtf8(std::string_view text);

	/**
	 * Writes the comments (if any), a comment line with the current date and the own entries (not the defaults), sorted by key, in a format
	 * suitable for load(std::istream&): characters below 0x20 and above 0x7E are written as \\uXXXX escapes, so the output is ASCII (except for
	 * characters 0x80-0xFF in comments, which are written as Latin-1 bytes like in Java).
	 * Deviation: the line separator is always '\\n' (Java uses the platform line separator).
	 * Java: store(OutputStream, comments)
	 *
	 * @param comments optional description written as a comment line (Java: null for none)
	 */
	void store(std::ostream& out, std::optional<std::string_view> comments) const;

	/** Like store(std::ostream&, comments), but writes non-ASCII characters as UTF-8 instead of escaping them. Java: store(Writer, comments) */
	void storeUtf8(std::ostream& out, std::optional<std::string_view> comments) const;

	/**
	 * Searches for the property with the specified key in this property list, then (recursively) in the default property list.
	 *
	 * @return the value, or nullopt if the property is not found (Java: null)
	 */
	std::optional<std::string> getProperty(std::string_view key) const;

	/** @return the value for the key (see getProperty(key)), or defaultValue if the property is not found */
	std::string getProperty(std::string_view key, std::string_view defaultValue) const;

	/**
	 * Sets the value of a property in this object (defaults are not affected).
	 *
	 * @return the previous own value of the property, or nullopt if there was none
	 */
	std::optional<std::string> setProperty(std::string key, std::string value);

	/** Copies all own entries of other into this object, replacing existing values (Java: Hashtable.putAll; defaults of other are ignored). */
	void putAll(const Properties& other);

	/** Removes an own entry. @return the removed value, or nullopt */
	std::optional<std::string> remove(std::string_view key);

	/** Removes all own entries (the defaults stay). */
	void clear() noexcept { table.clear(); }

	/** @return true if this object itself contains the key (Java: Hashtable.containsKey, defaults are not searched) */
	bool containsKey(std::string_view key) const { return table.contains(key); }

	/** @return all keys in this property list including the distinct keys of the default property lists */
	std::set<std::string> stringPropertyNames() const;

	/** @return true if this object itself has no entries. Like in Java, the defaults are not taken into account. */
	bool isEmpty() const noexcept { return table.empty(); }

	/** @return the number of own entries. Like in Java, the defaults are not taken into account. */
	std::size_t size() const noexcept { return table.size(); }

	/** @return the own entries (without defaults) */
	const Map& entries() const noexcept { return table; }

	/** @return the default property list, or nullptr */
	const std::shared_ptr<const Properties>& getDefaults() const noexcept { return defaults; }

private:
	void load0(std::u16string_view chars);
	void store0(std::ostream& out, std::optional<std::string_view> comments, bool escUnicode) const;
	void collectNames(std::set<std::string>& names) const;

	Map table;
	std::shared_ptr<const Properties> defaults;
};

} // namespace aion::commons::configuration
