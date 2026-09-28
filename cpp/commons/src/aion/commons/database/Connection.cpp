#include "aion/commons/database/Connection.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/time.h>
#endif
#include <fmt/format.h>
#include <mysql.h>
#include <mysqld_error.h>
#include <errmsg.h>

#include "aion/commons/database/MariaDbLibrary.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::commons::database {

namespace {

const auto log = logging::LoggerFactory::getLogger("com.aionemu.commons.database.Connection");

std::atomic<uint64_t> savepointCounter{0};

bool isCommunicationError(unsigned int errorNumber) noexcept {
	switch (errorNumber) {
		case CR_CONNECTION_ERROR:
		case CR_CONN_HOST_ERROR:
		case CR_SERVER_GONE_ERROR:
		case CR_SERVER_LOST:
		case CR_COMMANDS_OUT_OF_SYNC:
		case CR_SSL_CONNECTION_ERROR:
		case CR_MALFORMED_PACKET:
		case CR_SERVER_LOST_EXTENDED:
		case 1053: // ER_SERVER_SHUTDOWN
		case 1927: // ER_CONNECTION_KILLED
		case 4031: // ER_CLIENT_INTERACTION_TIMEOUT
			return true;
		default:
			return false;
	}
}

/** @return true for errors of establishing the TCP connection itself (before any server response) */
bool isSocketConnectError(unsigned int errorNumber) noexcept {
	return errorNumber == CR_CONNECTION_ERROR || errorNumber == CR_CONN_HOST_ERROR || errorNumber == CR_IPSOCK_ERROR ||
		errorNumber == CR_UNKNOWN_HOST;
}

std::string quoteIdentifier(std::string_view name) {
	return "`" + utils::StringUtils::replace(name, "`", "``") + "`";
}

unsigned int toTimeoutSeconds(std::chrono::milliseconds timeout) noexcept {
	auto seconds = (timeout.count() + 999) / 1000;
	return static_cast<unsigned int>(std::clamp<int64_t>(seconds, 1, 365 * 24 * 3600));
}

/**
 * Temporarily sets the receive and send timeouts of a blocking socket (Connector/C implements its read/write timeouts the same way, see
 * pvio_socket_change_timeout) and restores the previous values on destruction.
 */
class SocketTimeoutOverride {
public:
	SocketTimeoutOverride(my_socket socket, std::chrono::milliseconds timeout) noexcept : socket(socket) {
		if (socket == INVALID_SOCKET_VALUE)
			return;
		for (size_t i = 0; i < OPTIONS.size(); ++i)
			saved[i] = get(OPTIONS[i]);
		for (size_t i = 0; i < OPTIONS.size(); ++i) {
			if (saved[i])
				set(OPTIONS[i], toNative(timeout));
		}
	}

	~SocketTimeoutOverride() {
		for (size_t i = 0; i < OPTIONS.size(); ++i) {
			if (saved[i])
				set(OPTIONS[i], *saved[i]);
		}
	}

	SocketTimeoutOverride(const SocketTimeoutOverride&) = delete;
	SocketTimeoutOverride& operator=(const SocketTimeoutOverride&) = delete;

private:
#ifdef _WIN32
	using NativeTimeout = DWORD; // milliseconds, 0 = none
	static constexpr my_socket INVALID_SOCKET_VALUE = INVALID_SOCKET;
	static NativeTimeout toNative(std::chrono::milliseconds timeout) noexcept {
		return static_cast<DWORD>(std::clamp<int64_t>(timeout.count(), 1, std::numeric_limits<int32_t>::max()));
	}
#else
	using NativeTimeout = timeval;
	static constexpr my_socket INVALID_SOCKET_VALUE = -1;
	static NativeTimeout toNative(std::chrono::milliseconds timeout) noexcept {
		auto millis = std::max<int64_t>(timeout.count(), 1);
		return timeval{.tv_sec = static_cast<time_t>(millis / 1000), .tv_usec = static_cast<suseconds_t>(millis % 1000 * 1000)};
	}
#endif
	static constexpr std::array<int, 2> OPTIONS{SO_RCVTIMEO, SO_SNDTIMEO};

	std::optional<NativeTimeout> get(int option) const noexcept {
		NativeTimeout value{};
		auto length = static_cast<socklen_t>(sizeof(value));
		if (getsockopt(socket, SOL_SOCKET, option, static_cast<char*>(static_cast<void*>(&value)), &length) != 0)
			return std::nullopt;
		return value;
	}

	void set(int option, const NativeTimeout& value) const noexcept {
		setsockopt(socket, SOL_SOCKET, option, static_cast<const char*>(static_cast<const void*>(&value)), static_cast<socklen_t>(sizeof(value)));
	}

	my_socket socket;
	std::array<std::optional<NativeTimeout>, 2> saved{};
};

} // namespace

SQLException Connection::createException(unsigned int errorNumber, std::string_view sqlState, std::string_view message) {
	std::string state(sqlState);
	if (isCommunicationError(errorNumber) && (state.empty() || state == "HY000"))
		state = "08S01";
	return SQLException(std::string(message), std::move(state), static_cast<int32_t>(errorNumber), std::stacktrace::current(1));
}

Connection::Connection(ConnectionProperties properties) : properties(std::move(properties)) {
	MariaDbLibrary::acquire();
}

Connection::~Connection() {
	closeOpenStatements();
	if (mysql)
		mysql_close(mysql);
	MariaDbLibrary::release();
}

namespace {

/**
 * Resolves the host to IP literals, IPv4 addresses first. Java (and therefore Connector/J) prefers IPv4 by default
 * (java.net.preferIPv6Addresses=false), whereas Windows' resolver returns ::1 first for "localhost". Letting Connector/C connect by name
 * against a server that only listens on 127.0.0.1 costs a refused IPv6 attempt of about 2 seconds for every new connection.
 * Deviation: Connector/J tries all resolved addresses in the resolver's order after applying the preference; so do we.
 */
std::vector<std::string> resolveHostPreferIPv4(const std::string& host) {
	std::vector<std::string> v4;
	std::vector<std::string> v6;
	try {
		asio::io_context context;
		asio::ip::tcp::resolver resolver(context);
		for (const auto& entry : resolver.resolve(host, "")) {
			auto address = entry.endpoint().address();
			auto& target = address.is_v4() ? v4 : v6;
			std::string literal = address.to_string();
			if (std::ranges::find(target, literal) == target.end())
				target.push_back(std::move(literal));
		}
	} catch (const std::exception&) {
		// let Connector/C resolve (and report) the name itself
	}
	v4.insert(v4.end(), v6.begin(), v6.end());
	if (v4.empty())
		v4.push_back(host);
	return v4;
}

} // namespace

std::unique_ptr<Connection> Connection::open(const ConnectionProperties& properties) {
	// VERIFY_IDENTITY checks the certificate against the host name, so the name must be passed through unresolved
	std::vector<std::string> hosts = properties.sslMode == SslMode::VERIFY_IDENTITY ? std::vector{properties.host}
																																								: resolveHostPreferIPv4(properties.host);
	for (size_t i = 0;; i++) {
		try {
			return open(properties, hosts[i]);
		} catch (const SQLException& e) {
			// Connector/J (StandardSocketFactory) only tries the next address if the socket connection failed. Server errors like access denied
			// or an unknown database are reported right away.
			if (i + 1 >= hosts.size() || !isSocketConnectError(static_cast<unsigned int>(e.getErrorCode())))
				throw;
		}
	}
}

std::unique_ptr<Connection> Connection::open(const ConnectionProperties& properties, const std::string& hostAddress) {
	std::unique_ptr<Connection> connection(new Connection(properties));
	MYSQL* mysql = mysql_init(nullptr);
	if (!mysql)
		throw SQLException("Could not allocate a MariaDB connection handle", "HY001");
	connection->mysql = mysql;

	mysql_optionsv(mysql, MYSQL_SET_CHARSET_NAME, properties.characterSet.c_str());
	my_bool reconnect = 0;
	mysql_optionsv(mysql, MYSQL_OPT_RECONNECT, &reconnect);
	unsigned int localInfile = 0;
	mysql_optionsv(mysql, MYSQL_OPT_LOCAL_INFILE, &localInfile);
	if (properties.connectTimeout.count() > 0) {
		unsigned int seconds = toTimeoutSeconds(properties.connectTimeout);
		mysql_optionsv(mysql, MYSQL_OPT_CONNECT_TIMEOUT, &seconds);
	}
	if (properties.socketTimeout.count() > 0) {
		unsigned int seconds = toTimeoutSeconds(properties.socketTimeout);
		mysql_optionsv(mysql, MYSQL_OPT_READ_TIMEOUT, &seconds);
		mysql_optionsv(mysql, MYSQL_OPT_WRITE_TIMEOUT, &seconds);
	}
	// Connector/C 3.4 (plugins/auth/my_auth.c) requests TLS if MYSQL_OPT_SSL_ENFORCE is set or the certificate is verified. Without verification
	// a server that does not offer TLS is silently connected in plain text; with verification the connection fails before authentication.
	// - DISABLED: no TLS.
	// - PREFERRED (Connector/J's default): TLS whenever the server supports it, certificate not verified, plain text otherwise.
	// - REQUIRED: like PREFERRED, but a connection without TLS is rejected (checked below).
	// - VERIFY_CA / VERIFY_IDENTITY: TLS with certificate verification; fails if the server does not support TLS.
	my_bool useTls = properties.sslMode != SslMode::DISABLED;
	my_bool verifyServerCertificate = properties.sslMode == SslMode::VERIFY_CA || properties.sslMode == SslMode::VERIFY_IDENTITY;
	mysql_optionsv(mysql, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &verifyServerCertificate);
	mysql_optionsv(mysql, MYSQL_OPT_SSL_ENFORCE, &useTls);

	unsigned long clientFlags = CLIENT_MULTI_RESULTS;
	if (!properties.useAffectedRows)
		clientFlags |= CLIENT_FOUND_ROWS;
	if (!mysql_real_connect(mysql, hostAddress.c_str(), properties.user.c_str(), properties.password.c_str(),
				properties.database.empty() ? nullptr : properties.database.c_str(), properties.port, nullptr, clientFlags))
		connection->throwError();
	if (properties.sslMode == SslMode::REQUIRED && !mysql_get_ssl_cipher(mysql)) {
		// Deviation: Connector/J rejects the server before authenticating. Connector/C offers no hook between the server greeting and the
		// authentication response, so the (scrambled, never plain text) authentication data has already been sent at this point.
		throw SQLException("SSL Connection required, but not supported by server.", "08001");
	}

	connection->initializeSession();
	return connection;
}

void Connection::initializeSession() {
	auto row = querySingleRow("SELECT @@session.auto_increment_increment, @@session.sql_mode");
	if (row.size() >= 2) {
		if (row[0]) {
			uint64_t increment = 0;
			auto [end, ec] = std::from_chars(row[0]->data(), row[0]->data() + row[0]->size(), increment);
			if (ec == std::errc() && increment > 0)
				autoIncrementIncrement = increment;
		}
		// Connector/J jdbcCompliantTruncation: let the server reject truncated data
		std::string sqlMode = row[1].value_or("");
		if (utils::StringUtils::toUpperCase(sqlMode).find("STRICT_TRANS_TABLES") == std::string::npos)
			executeSimple(fmt::format("SET sql_mode='{}{}STRICT_TRANS_TABLES'", sqlMode, sqlMode.empty() ? "" : ","));
	}
	if (!getAutoCommit())
		setAutoCommit(true);
}

void Connection::onError(unsigned int errorNumber, std::string_view sqlState) noexcept {
	if (isCommunicationError(errorNumber) || sqlState.starts_with("08"))
		broken = true;
}

void Connection::throwError() {
	unsigned int errorNumber = mysql_errno(mysql);
	std::string sqlState = mysql_sqlstate(mysql);
	std::string message = mysql_error(mysql);
	onError(errorNumber, sqlState);
	throw createException(errorNumber, sqlState, message);
}

void Connection::checkOpen() const {
	if (!mysql || broken)
		throw SQLException("No operations allowed after connection closed.", "08003");
}

void Connection::executeSimple(std::string_view sql) {
	executeText(sql);
}

Connection::TextResult Connection::executeText(std::string_view sql) {
	checkOpen();
	if (mysql_real_query(mysql, sql.data(), static_cast<unsigned long>(sql.size())) != 0)
		throwError();
	TextResult first;
	bool isFirst = true;
	while (true) {
		MYSQL_RES* result = mysql_store_result(mysql);
		if (result)
			mysql_free_result(result);
		else if (mysql_field_count(mysql) != 0)
			throwError();
		else if (isFirst) {
			my_ulonglong affected = mysql_affected_rows(mysql);
			first.updateCount = affected == static_cast<my_ulonglong>(-1) ? -1 : static_cast<int64_t>(std::min<my_ulonglong>(affected, std::numeric_limits<int64_t>::max()));
			first.insertId = mysql_insert_id(mysql);
		}
		isFirst = false;
		int next = mysql_next_result(mysql);
		if (next > 0)
			throwError();
		if (next < 0)
			break;
	}
	return first;
}

std::vector<std::optional<std::string>> Connection::querySingleRow(std::string_view sql) {
	checkOpen();
	if (mysql_real_query(mysql, sql.data(), static_cast<unsigned long>(sql.size())) != 0)
		throwError();
	std::vector<std::optional<std::string>> values;
	MYSQL_RES* result = mysql_store_result(mysql);
	if (!result) {
		if (mysql_field_count(mysql) != 0)
			throwError();
		return values;
	}
	if (MYSQL_ROW row = mysql_fetch_row(result)) {
		unsigned int count = mysql_num_fields(result);
		unsigned long* lengths = mysql_fetch_lengths(result);
		for (unsigned int i = 0; i < count; ++i) {
			if (row[i])
				values.emplace_back(std::string(row[i], lengths[i]));
			else
				values.emplace_back(std::nullopt);
		}
	}
	mysql_free_result(result);
	while (mysql_next_result(mysql) == 0) {
		if (MYSQL_RES* extra = mysql_store_result(mysql))
			mysql_free_result(extra);
	}
	return values;
}

std::unique_ptr<PreparedStatement> Connection::prepareStatement(std::string_view sql) {
	return prepareStatement(sql, ResultSet::TYPE_FORWARD_ONLY, ResultSet::CONCUR_READ_ONLY);
}

std::unique_ptr<PreparedStatement> Connection::prepareStatement(std::string_view sql, int32_t autoGeneratedKeys) {
	checkOpen();
	return std::unique_ptr<PreparedStatement>(
		new PreparedStatement(*this, sql, autoGeneratedKeys == Statement::RETURN_GENERATED_KEYS, ResultSet::TYPE_FORWARD_ONLY));
}

std::unique_ptr<PreparedStatement> Connection::prepareStatement(std::string_view sql, int32_t resultSetType, int32_t resultSetConcurrency) {
	checkOpen();
	if (resultSetConcurrency == ResultSet::CONCUR_UPDATABLE)
		throw SQLException("Updatable result sets are not supported", "S1C00");
	if (resultSetType != ResultSet::TYPE_FORWARD_ONLY && resultSetType != ResultSet::TYPE_SCROLL_INSENSITIVE &&
		resultSetType != ResultSet::TYPE_SCROLL_SENSITIVE)
		throw SQLException(fmt::format("Illegal value for resultSetType: {}", resultSetType), "S1009");
	return std::unique_ptr<PreparedStatement>(new PreparedStatement(*this, sql, false, resultSetType));
}

std::unique_ptr<PreparedStatement> Connection::prepareCall(std::string_view sql) {
	return prepareStatement(sql);
}

void Connection::setAutoCommit(bool autoCommit) {
	checkOpen();
	if (mysql_autocommit(mysql, autoCommit ? 1 : 0) != 0)
		throwError();
}

bool Connection::getAutoCommit() const noexcept {
	if (!mysql)
		return true;
	unsigned int status = 0;
	mariadb_get_infov(mysql, MARIADB_CONNECTION_SERVER_STATUS, &status);
	return (status & SERVER_STATUS_AUTOCOMMIT) != 0;
}

bool Connection::isInTransaction() const noexcept {
	if (!mysql)
		return false;
	unsigned int status = 0;
	mariadb_get_infov(mysql, MARIADB_CONNECTION_SERVER_STATUS, &status);
	return (status & SERVER_STATUS_IN_TRANS) != 0;
}

void Connection::commit() {
	checkOpen();
	if (getAutoCommit())
		throw SQLException("Can't call commit when autocommit=true");
	if (mysql_commit(mysql) != 0)
		throwError();
}

void Connection::rollback() {
	checkOpen();
	if (getAutoCommit())
		throw SQLException("Can't call rollback when autocommit=true", "08003");
	if (mysql_rollback(mysql) != 0)
		throwError();
}

Savepoint Connection::setSavepoint() {
	return setSavepoint(fmt::format("SAVEPOINT_{}", savepointCounter.fetch_add(1)));
}

Savepoint Connection::setSavepoint(std::string_view name) {
	executeSimple("SAVEPOINT " + quoteIdentifier(name));
	return Savepoint(std::string(name));
}

void Connection::rollback(const Savepoint& savepoint) {
	checkOpen();
	if (getAutoCommit())
		throw SQLException("Can't call rollback when autocommit=true", "08003");
	executeSimple("ROLLBACK TO SAVEPOINT " + quoteIdentifier(savepoint.getSavepointName()));
}

void Connection::releaseSavepoint(const Savepoint& savepoint) {
	executeSimple("RELEASE SAVEPOINT " + quoteIdentifier(savepoint.getSavepointName()));
}

bool Connection::isValid(int32_t timeoutSeconds) {
	if (timeoutSeconds < 0)
		throw SQLException("Invalid value for timeout parameter", "S1009");
	if (!mysql || broken)
		return false;
	// Connector/C has no per-call timeout, so the socket's receive/send timeouts are lowered for the ping (a peer that vanished without a TCP
	// reset would otherwise block until the configured socketTimeout, or until TCP gives up if there is none). A timed out ping leaves the
	// connection unusable, which marks it broken anyway.
	std::optional<SocketTimeoutOverride> timeoutOverride;
	if (timeoutSeconds > 0)
		timeoutOverride.emplace(mysql_get_socket(mysql), std::chrono::seconds(timeoutSeconds));
	if (mysql_ping(mysql) != 0) {
		log.debug("Ping failed: {} ({})", mysql_error(mysql), mysql_errno(mysql));
		broken = true;
		return false;
	}
	return true;
}

std::optional<std::string> Connection::getCatalog() {
	auto row = querySingleRow("SELECT DATABASE()");
	return row.empty() ? std::nullopt : row[0];
}

std::string Connection::getServerVersion() const {
	if (!mysql)
		return {};
	const char* version = mysql_get_server_info(mysql);
	return version ? version : "";
}

void Connection::registerStatement(PreparedStatement* statement) {
	openStatements.push_back(statement);
}

void Connection::unregisterStatement(PreparedStatement* statement) noexcept {
	std::erase(openStatements, statement);
}

void Connection::closeOpenStatements() noexcept {
	auto statements = std::move(openStatements);
	openStatements.clear();
	for (PreparedStatement* statement : statements)
		statement->detach();
}

} // namespace aion::commons::database
