#include "aion/commons/configuration/transformers/InetSocketAddressTransformer.h"

#include <cstdint>
#include <optional>

#include <asio/ip/address_v6.hpp>
#include <fmt/format.h>

#include "aion/commons/utils/Exception.h"

namespace aion::commons::configuration::transformers::InetSocketAddressTransformer {

namespace {

constexpr bool isDigit(char c) noexcept {
	return c >= '0' && c <= '9';
}

constexpr bool isAlpha(char c) noexcept {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

constexpr bool isAlphaNum(char c) noexcept {
	return isDigit(c) || isAlpha(c);
}

constexpr bool isHexDigit(char c) noexcept {
	return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/** Thrown inside the parser for syntax errors (Java: URISyntaxException), never escapes this file. */
struct SyntaxError {};

/**
 * Port of the server-based authority parsing of java.net.URI.Parser (parseServer, parseIPv4Address, parseHostname, parseIPv6Reference), reduced
 * to deciding whether the authority is valid and extracting host and port. Java percent-encodes characters that are not allowed in an authority
 * before parsing; such characters can never be part of a valid host or port, so they simply fail here.
 */
class AuthorityParser {
public:
	explicit AuthorityParser(std::string_view input) noexcept : input(input) {}

	std::string host;
	int64_t port = -1;

	/** @throws SyntaxError */
	void parseServer() {
		std::size_t n = input.size();
		std::size_t p = 0;

		// userinfo (anything but brackets, the characters that are not allowed are percent-encoded by Java)
		if (std::size_t at = input.find('@'); at != std::string_view::npos) {
			if (input.substr(0, at).find_first_of("[]") != std::string_view::npos)
				throw SyntaxError();
			p = at + 1;
		}

		// hostname, IPv4 address, or IPv6 address
		if (at(p, n, '[')) {
			p++;
			std::size_t q = input.find(']', p);
			if (q == std::string_view::npos || q == p)
				throw SyntaxError(); // expected closing bracket for IPv6 address
			std::size_t r = input.substr(p, q - p).find('%');
			if (r != std::string_view::npos) {
				r += p;
				parseIPv6Reference(p, r);
				if (r + 1 == q)
					throw SyntaxError(); // scope id expected
				for (std::size_t i = r + 1; i < q; i++) {
					if (!isAlphaNum(input[i]) && input[i] != '_' && input[i] != '.')
						throw SyntaxError();
				}
			} else {
				parseIPv6Reference(p, q);
			}
			host = input.substr(p, q - p); // without brackets
			p = q + 1;
		} else {
			std::optional<std::size_t> q = parseIPv4Address(p, n);
			if (!q)
				q = parseHostname(p, n);
			host = input.substr(p, *q - p);
			p = *q;
		}

		// port
		if (at(p, n, ':')) {
			p++;
			std::size_t q = p;
			while (q < n && isDigit(input[q]))
				q++;
			if (q < n)
				throw SyntaxError(); // illegal character in port number
			if (q > p) {
				int64_t value = 0;
				for (std::size_t i = p; i < q; i++) {
					value = value * 10 + (input[i] - '0');
					if (value > INT32_MAX)
						throw SyntaxError(); // malformed port number (Integer.parseInt overflow)
				}
				port = value;
				p = q;
			}
		}
		if (p < n)
			throw SyntaxError(); // expected port number
	}

private:
	bool at(std::size_t start, std::size_t end, char c) const noexcept { return start < end && input[start] == c; }

	bool at(std::size_t start, std::size_t end, std::string_view s) const noexcept {
		return start + s.size() <= end && input.substr(start, s.size()) == s;
	}

	std::size_t scanDigits(std::size_t p, std::size_t end) const noexcept {
		while (p < end && isDigit(input[p]))
			p++;
		return p;
	}

	std::size_t scanHexDigits(std::size_t p, std::size_t end) const noexcept {
		while (p < end && isHexDigit(input[p]))
			p++;
		return p;
	}

	/** @return end of the byte, or start if it is not a number &lt;= 255 */
	std::size_t scanByte(std::size_t start, std::size_t end) const noexcept {
		std::size_t q = scanDigits(start, end);
		if (q <= start)
			return q;
		int64_t value = 0;
		for (std::size_t i = start; i < q && value <= 255; i++)
			value = value * 10 + (input[i] - '0');
		return value > 255 ? start : q;
	}

	/** Java: scanIPv4Address. @return end of the address, or nullopt if it is not a valid IPv4 address */
	std::optional<std::size_t> scanIPv4Address(std::size_t start, std::size_t n, bool strict) const noexcept {
		std::size_t p = start;
		std::size_t m = start;
		while (m < n && (isDigit(input[m]) || input[m] == '.'))
			m++;
		if (m <= p || (strict && m != n))
			return std::nullopt;
		for (int i = 0; i < 4; i++) {
			if (i > 0) {
				if (!at(p, m, '.'))
					return std::nullopt;
				p++;
			}
			std::size_t q = scanByte(p, m);
			if (q <= p)
				return std::nullopt;
			p = q;
		}
		if (p < m)
			return std::nullopt;
		return p;
	}

	/** Java: parseIPv4Address */
	std::optional<std::size_t> parseIPv4Address(std::size_t start, std::size_t n) const noexcept {
		std::optional<std::size_t> p = scanIPv4Address(start, n, false);
		// an IPv4 address may only be followed by ':'
		if (p && *p < n && input[*p] != ':')
			return std::nullopt;
		return p;
	}

	/**
	 * Java: parseHostname
	 * hostname      = domainlabel [ "." ] | 1*( domainlabel "." ) toplabel [ "." ]
	 * domainlabel   = alphanum | alphanum *( alphanum | "-" ) alphanum
	 * toplabel      = alpha | alpha *( alphanum | "-" ) alphanum
	 */
	std::size_t parseHostname(std::size_t start, std::size_t n) const {
		std::size_t p = start;
		std::optional<std::size_t> l; // start of last parsed label
		do {
			std::size_t q = p;
			while (q < n && isAlphaNum(input[q]))
				q++;
			if (q <= p)
				break;
			l = p;
			p = q;
			while (q < n && (isAlphaNum(input[q]) || input[q] == '-'))
				q++;
			if (q > p) {
				if (input[q - 1] == '-')
					throw SyntaxError();
				p = q;
			}
			if (!at(p, n, '.'))
				break;
			p++;
		} while (p < n);

		if (p < n && !at(p, n, ':'))
			throw SyntaxError();
		if (!l)
			throw SyntaxError(); // expected hostname
		// for a fully qualified hostname check that the rightmost label starts with an alpha character
		if (*l > start && !isAlpha(input[*l]))
			throw SyntaxError();
		return p;
	}

	/** Java: scanHexSeq. @return end of the sequence, or nullopt if no hex sequence could be scanned (e.g. start of an IPv4 address) */
	std::optional<std::size_t> scanHexSeq(std::size_t start, std::size_t n) {
		std::size_t p = start;
		std::size_t q = scanHexDigits(p, n);
		if (q <= p || at(q, n, '.'))
			return std::nullopt;
		if (q > p + 4)
			throw SyntaxError(); // IPv6 hexadecimal digit sequence too long
		ipv6ByteCount += 2;
		p = q;
		while (p < n) {
			if (!at(p, n, ':') || at(p + 1, n, ':'))
				break;
			p++;
			q = scanHexDigits(p, n);
			if (q <= p)
				throw SyntaxError(); // expected digits for an IPv6 address
			if (at(q, n, '.')) {   // beginning of IPv4 address
				p--;
				break;
			}
			if (q > p + 4)
				throw SyntaxError();
			ipv6ByteCount += 2;
			p = q;
		}
		return p;
	}

	std::size_t takeIPv4Address(std::size_t start, std::size_t n) const {
		std::optional<std::size_t> p = scanIPv4Address(start, n, true);
		if (!p || *p <= start)
			throw SyntaxError();
		return *p;
	}

	/** Java: scanHexPost. hexpost = hexseq | hexseq ":" IPv4address | IPv4address */
	std::size_t scanHexPost(std::size_t start, std::size_t n) {
		std::size_t p = start;
		if (p == n)
			return p;
		if (std::optional<std::size_t> q = scanHexSeq(p, n)) {
			p = *q;
			if (at(p, n, ':')) {
				p = takeIPv4Address(p + 1, n);
				ipv6ByteCount += 4;
			}
		} else {
			p = takeIPv4Address(p, n);
			ipv6ByteCount += 4;
		}
		return p;
	}

	/**
	 * Java: parseIPv6Reference
	 * IPv6address = hexseq [ ":" IPv4address ] | hexseq [ "::" [ hexpost ] ] | "::" [ hexpost ]
	 * Addresses without compressed zeros must contain exactly 16 bytes, addresses with compressed zeros less than 16 bytes.
	 */
	void parseIPv6Reference(std::size_t start, std::size_t n) {
		std::size_t p = start;
		bool compressedZeros = false;
		if (std::optional<std::size_t> q = scanHexSeq(p, n)) {
			p = *q;
			if (at(p, n, "::")) {
				compressedZeros = true;
				p = scanHexPost(p + 2, n);
			} else if (at(p, n, ':')) {
				p = takeIPv4Address(p + 1, n);
				ipv6ByteCount += 4;
			}
		} else if (at(p, n, "::")) {
			compressedZeros = true;
			p = scanHexPost(p + 2, n);
		}
		if (p < n || ipv6ByteCount > 16 || (!compressedZeros && ipv6ByteCount < 16) || (compressedZeros && ipv6ByteCount == 16))
			throw SyntaxError();
	}

	std::string_view input;
	int ipv6ByteCount = 0;
};

/**
 * Java resolves IP literals eagerly, so InetAddress.isAnyLocalAddress() is true for every spelling of the unspecified address, and IPv4-mapped
 * addresses become an Inet4Address. utils::InetSocketAddress keeps the host unresolved and only recognizes "0.0.0.0" and "::" as any local
 * address, so other spellings of the unspecified IPv6 address are normalized to "::", and the IPv4-mapped unspecified address to "0.0.0.0".
 * Addresses with a scope id and all other addresses keep their spelling.
 */
std::string normalizeUnspecifiedIPv6(std::string host) {
	if (host.find(':') == std::string::npos || host.find('%') != std::string::npos)
		return host;
	std::error_code error;
	asio::ip::address_v6 address = asio::ip::make_address_v6(host, error);
	if (error)
		return host;
	if (address.is_unspecified())
		return "::";
	if (address.is_v4_mapped() && asio::ip::make_address_v4(asio::ip::v4_mapped, address).is_unspecified())
		return "0.0.0.0";
	return host;
}

} // namespace

utils::InetSocketAddress parse(std::string_view value) {
	if (value.empty())
		throw utils::IllegalArgumentException("Expected authority at index 2: //"); // Java: URISyntaxException
	AuthorityParser parser(value);
	try {
		parser.parseServer();
	} catch (const SyntaxError&) {
		// Java falls back to a registry-based authority without host, unless the value contains brackets (not allowed in a registry name)
		if (value.find_first_of("[]") != std::string_view::npos)
			throw utils::IllegalArgumentException(fmt::format("Malformed IPv6 address or port in authority: //{}", value));
		throw utils::IllegalArgumentException("hostname can't be null");
	}
	if (parser.port < 0 || parser.port > 0xFFFF)
		throw utils::IllegalArgumentException(fmt::format("port out of range:{}", parser.port));
	return utils::InetSocketAddress{.host = normalizeUnspecifiedIPv6(std::move(parser.host)), .port = static_cast<uint16_t>(parser.port)};
}

} // namespace aion::commons::configuration::transformers::InetSocketAddressTransformer
