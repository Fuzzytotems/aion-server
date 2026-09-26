#include "aion/commons/database/ConnectionProperties.h"

#include <charconv>

#include <fmt/format.h>

#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::commons::database {

namespace {

using utils::StringUtils::equalsIgnoreCase;

const auto log = logging::LoggerFactory::getLogger("com.aionemu.commons.database.ConnectionProperties");

const char* SQL_STATE_INVALID_CONNECTION_ATTRIBUTE = "01S00";

[[noreturn]] void invalidUrl(std::string_view url, std::string_view reason) {
	throw SQLException(fmt::format("Cannot handle the connection string '{}': {}", url, reason), "08001");
}

int hexValue(char c) {
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

/** Java: URLDecoder.decode(s, UTF-8) */
std::string urlDecode(std::string_view url, std::string_view text) {
	std::string decoded;
	decoded.reserve(text.size());
	for (size_t i = 0; i < text.size(); ++i) {
		char c = text[i];
		if (c == '+') {
			decoded += ' ';
		} else if (c == '%') {
			int high = i + 2 < text.size() ? hexValue(text[i + 1]) : -1;
			int low = i + 2 < text.size() ? hexValue(text[i + 2]) : -1;
			if (high < 0 || low < 0)
				invalidUrl(url, fmt::format("malformed escape sequence in '{}'", text));
			decoded += static_cast<char>(high * 16 + low);
			i += 2;
		} else {
			decoded += c;
		}
	}
	return decoded;
}

bool parseBoolean(std::string_view name, std::string_view value) {
	if (equalsIgnoreCase(value, "true") || equalsIgnoreCase(value, "yes"))
		return true;
	if (equalsIgnoreCase(value, "false") || equalsIgnoreCase(value, "no"))
		return false;
	throw SQLException(fmt::format("The connection property '{}' only accepts values of the form: 'true', 'false', 'yes' or 'no'. The value '{}' is not in this set.", name, value),
		SQL_STATE_INVALID_CONNECTION_ATTRIBUTE);
}

int32_t parseInt(std::string_view name, std::string_view value) {
	int32_t result = 0;
	std::string_view trimmed = utils::StringUtils::trim(value);
	auto [end, ec] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), result);
	if (trimmed.empty() || ec != std::errc() || end != trimmed.data() + trimmed.size())
		throw SQLException(fmt::format("The connection property '{}' only accepts integer values. The value '{}' can not be converted to an integer.", name, value),
			SQL_STATE_INVALID_CONNECTION_ATTRIBUTE);
	return result;
}

void parseHost(ConnectionProperties& props, std::string_view url, std::string_view hostAndPort) {
	std::string_view portText;
	if (hostAndPort.starts_with('[')) {
		size_t close = hostAndPort.find(']');
		if (close == std::string_view::npos)
			invalidUrl(url, "unterminated IPv6 address");
		props.host = std::string(hostAndPort.substr(1, close - 1));
		std::string_view rest = hostAndPort.substr(close + 1);
		if (!rest.empty()) {
			if (rest[0] != ':')
				invalidUrl(url, "invalid host");
			portText = rest.substr(1);
		}
	} else {
		size_t colon = hostAndPort.find(':');
		std::string_view host = hostAndPort.substr(0, colon);
		if (!host.empty())
			props.host = std::string(host);
		if (colon != std::string_view::npos)
			portText = hostAndPort.substr(colon + 1);
	}
	if (!portText.empty()) {
		int32_t port = 0;
		auto [end, ec] = std::from_chars(portText.data(), portText.data() + portText.size(), port);
		if (ec != std::errc() || end != portText.data() + portText.size() || port < 1 || port > 65535)
			invalidUrl(url, fmt::format("invalid port '{}'", portText));
		props.port = static_cast<uint16_t>(port);
	}
}

void applyParameters(ConnectionProperties& props) {
	std::optional<bool> useSsl, requireSsl, verifyServerCertificate;
	std::optional<SslMode> sslMode;
	for (const auto& [name, value] : props.parameters) {
		if (equalsIgnoreCase(name, "characterEncoding")) {
			if (!equalsIgnoreCase(value, "UTF-8") && !equalsIgnoreCase(value, "UTF8") && !equalsIgnoreCase(value, "utf8mb4"))
				log.warn("Ignoring characterEncoding={}: the connection always uses utf8mb4", value);
		} else if (equalsIgnoreCase(name, "serverTimezone") || equalsIgnoreCase(name, "connectionTimeZone")) {
			props.timeZone = ConnectionTimeZone::of(value);
		} else if (equalsIgnoreCase(name, "user")) {
			props.user = value;
		} else if (equalsIgnoreCase(name, "password")) {
			props.password = value;
		} else if (equalsIgnoreCase(name, "connectTimeout")) {
			props.connectTimeout = std::chrono::milliseconds(std::max(0, parseInt(name, value)));
		} else if (equalsIgnoreCase(name, "socketTimeout")) {
			props.socketTimeout = std::chrono::milliseconds(std::max(0, parseInt(name, value)));
		} else if (equalsIgnoreCase(name, "useSSL")) {
			useSsl = parseBoolean(name, value);
		} else if (equalsIgnoreCase(name, "requireSSL")) {
			requireSsl = parseBoolean(name, value);
		} else if (equalsIgnoreCase(name, "verifyServerCertificate")) {
			verifyServerCertificate = parseBoolean(name, value);
		} else if (equalsIgnoreCase(name, "sslMode")) {
			if (equalsIgnoreCase(value, "DISABLED"))
				sslMode = SslMode::DISABLED;
			else if (equalsIgnoreCase(value, "PREFERRED"))
				sslMode = SslMode::PREFERRED;
			else if (equalsIgnoreCase(value, "REQUIRED"))
				sslMode = SslMode::REQUIRED;
			else if (equalsIgnoreCase(value, "VERIFY_CA"))
				sslMode = SslMode::VERIFY_CA;
			else if (equalsIgnoreCase(value, "VERIFY_IDENTITY"))
				sslMode = SslMode::VERIFY_IDENTITY;
			else
				throw SQLException(fmt::format("The connection property 'sslMode' acceptable values are: 'DISABLED', 'PREFERRED', 'REQUIRED', 'VERIFY_CA' or 'VERIFY_IDENTITY'. The value '{}' is not acceptable.", value),
					SQL_STATE_INVALID_CONNECTION_ATTRIBUTE);
		} else if (equalsIgnoreCase(name, "zeroDateTimeBehavior")) {
			if (equalsIgnoreCase(value, "EXCEPTION"))
				props.zeroDateTimeBehavior = ZeroDateTimeBehavior::EXCEPTION;
			else if (equalsIgnoreCase(value, "CONVERT_TO_NULL") || equalsIgnoreCase(value, "convertToNull"))
				props.zeroDateTimeBehavior = ZeroDateTimeBehavior::CONVERT_TO_NULL;
			else if (equalsIgnoreCase(value, "ROUND"))
				props.zeroDateTimeBehavior = ZeroDateTimeBehavior::ROUND;
			else
				throw SQLException(fmt::format("The connection property 'zeroDateTimeBehavior' acceptable values are: 'CONVERT_TO_NULL', 'EXCEPTION' or 'ROUND'. The value '{}' is not acceptable.", value),
					SQL_STATE_INVALID_CONNECTION_ATTRIBUTE);
		} else if (equalsIgnoreCase(name, "useAffectedRows")) {
			props.useAffectedRows = parseBoolean(name, value);
		} else {
			log.debug("Ignoring unsupported JDBC URL parameter {}={}", name, value);
		}
	}
	if (sslMode) {
		props.sslMode = *sslMode;
	} else if (useSsl || requireSsl || verifyServerCertificate) {
		// Connector/J: legacy properties are only used when sslMode is not set explicitly
		if (useSsl == false)
			props.sslMode = SslMode::DISABLED;
		else if (verifyServerCertificate == true)
			props.sslMode = SslMode::VERIFY_CA;
		else if (requireSsl == true)
			props.sslMode = SslMode::REQUIRED;
		else
			props.sslMode = SslMode::PREFERRED;
	}
}

} // namespace

ConnectionProperties ConnectionProperties::parse(std::string_view url) {
	std::string_view rest = utils::StringUtils::trim(url);
	bool schemeFound = false;
	for (std::string_view scheme : {"jdbc:mysql://", "jdbc:mariadb://"}) {
		if (rest.size() >= scheme.size() && equalsIgnoreCase(rest.substr(0, scheme.size()), scheme)) {
			rest.remove_prefix(scheme.size());
			schemeFound = true;
			break;
		}
	}
	if (!schemeFound)
		invalidUrl(url, "expected jdbc:mysql:// or jdbc:mariadb://");

	ConnectionProperties props;
	size_t authorityEnd = rest.find_first_of("/?");
	std::string_view authority = rest.substr(0, authorityEnd);
	rest = authorityEnd == std::string_view::npos ? std::string_view() : rest.substr(authorityEnd);

	size_t at = authority.rfind('@');
	if (at != std::string_view::npos) {
		std::string_view userInfo = authority.substr(0, at);
		authority.remove_prefix(at + 1);
		size_t colon = userInfo.find(':');
		props.user = urlDecode(url, userInfo.substr(0, colon));
		if (colon != std::string_view::npos)
			props.password = urlDecode(url, userInfo.substr(colon + 1));
	}
	size_t comma = authority.find(',');
	if (comma != std::string_view::npos) {
		log.debug("Multiple hosts are not supported, using the first one of {}", authority);
		authority = authority.substr(0, comma);
	}
	parseHost(props, url, authority);

	std::string_view query;
	if (rest.starts_with('/')) {
		size_t queryStart = rest.find('?');
		props.database = urlDecode(url, rest.substr(1, queryStart == std::string_view::npos ? std::string_view::npos : queryStart - 1));
		query = queryStart == std::string_view::npos ? std::string_view() : rest.substr(queryStart + 1);
	} else if (rest.starts_with('?')) {
		query = rest.substr(1);
	}
	for (const std::string& pair : utils::StringUtils::split(query, "&")) {
		if (pair.empty())
			continue;
		size_t eq = pair.find('=');
		std::string name = urlDecode(url, std::string_view(pair).substr(0, eq));
		std::string value = eq == std::string::npos ? std::string() : urlDecode(url, std::string_view(pair).substr(eq + 1));
		props.parameters.emplace_back(std::move(name), std::move(value));
	}
	applyParameters(props);
	return props;
}

ConnectionProperties ConnectionProperties::parse(std::string_view url, std::string_view user, std::string_view password) {
	ConnectionProperties props = parse(url);
	if (!user.empty())
		props.user = std::string(user);
	if (!password.empty())
		props.password = std::string(password);
	return props;
}

std::optional<std::string> ConnectionProperties::getParameter(std::string_view name) const {
	for (const auto& [key, value] : parameters) {
		if (equalsIgnoreCase(key, name))
			return value;
	}
	return std::nullopt;
}

} // namespace aion::commons::database
