#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "aion/commons/database/ConnectionTimeZone.h"

namespace aion::commons::database {

/** Connector/J zeroDateTimeBehavior: what getTimestamp/getDate return for MySQL's zero date "0000-00-00". */
enum class ZeroDateTimeBehavior { EXCEPTION, CONVERT_TO_NULL, ROUND };

/** Connector/J sslMode. */
enum class SslMode { DISABLED, PREFERRED, REQUIRED, VERIFY_CA, VERIFY_IDENTITY };

/**
 * Everything needed to open a Connection: parsed from a JDBC URL (HikariConfig.setJdbcUrl + Connector/J properties) plus credentials.
 * <p>
 * Supported URL format: <code>jdbc:mysql://[user[:password]@]host[:port][,more hosts][/database][?key=value&amp;...]</code>, also with the
 * <code>jdbc:mariadb:</code> prefix. IPv6 hosts must be written in brackets. Host defaults to localhost, port to 3306. Only the first host is
 * used. Parameter names are case-insensitive and values are URL-decoded.
 * <p>
 * Supported parameters (all others are ignored with a debug log):
 * <ul>
 * <li>characterEncoding: the connection always uses utf8mb4, since all strings in the C++ code are UTF-8. Values other than UTF-8 variants
 * are ignored with a warning.</li>
 * <li>serverTimezone / connectionTimeZone: time zone for Timestamp conversions (see ConnectionTimeZone). Empty: system time zone.</li>
 * <li>user, password: used if no credentials are given via DatabaseConfig</li>
 * <li>connectTimeout, socketTimeout (milliseconds, 0 = none): rounded up to whole seconds, since Connector/C only supports seconds</li>
 * <li>sslMode (DISABLED, PREFERRED, REQUIRED, VERIFY_CA, VERIFY_IDENTITY), useSSL, requireSSL, verifyServerCertificate</li>
 * <li>zeroDateTimeBehavior (EXCEPTION, CONVERT_TO_NULL, ROUND, also the old camel case names)</li>
 * <li>useAffectedRows (default false: UPDATE returns the number of matched rows, like Connector/J)</li>
 * </ul>
 */
struct ConnectionProperties {
	std::string host = "localhost";
	uint16_t port = 3306;
	std::string database;
	std::string user;
	std::string password;
	/** connection character set (always utf8mb4) */
	std::string characterSet = "utf8mb4";
	ConnectionTimeZone timeZone;
	std::chrono::milliseconds connectTimeout{0};
	std::chrono::milliseconds socketTimeout{0};
	SslMode sslMode = SslMode::PREFERRED;
	ZeroDateTimeBehavior zeroDateTimeBehavior = ZeroDateTimeBehavior::EXCEPTION;
	bool useAffectedRows = false;
	/** all URL parameters in URL order (names as written, values decoded) */
	std::vector<std::pair<std::string, std::string>> parameters;

	/**
	 * Parses a JDBC URL.
	 * @throws SQLException if the URL is not a valid MySQL/MariaDB JDBC URL or a supported parameter has an invalid value
	 */
	static ConnectionProperties parse(std::string_view url);

	/**
	 * Parses a JDBC URL and applies the given credentials. Like Connector/J, credentials passed separately override the ones from the URL.
	 * Deviation: empty strings count as "not given" here, because the C++ config cannot distinguish a missing key from an empty value.
	 */
	static ConnectionProperties parse(std::string_view url, std::string_view user, std::string_view password);

	/** @return the value of the URL parameter (case-insensitive name), if present */
	std::optional<std::string> getParameter(std::string_view name) const;
};

} // namespace aion::commons::database
