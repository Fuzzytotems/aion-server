#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace aion::commons::database {

class Connection;
class ResultSet;

/**
 * Java: java.sql.DatabaseMetaData, only the parts used by the servers. Obtained via Connection::getMetaData() and valid as long as the
 * connection.
 */
class DatabaseMetaData {
public:
	/**
	 * Java: getExportedKeys(catalog, schema, table) - the foreign key columns that reference the primary key of the given table, ordered by
	 * FKTABLE_CAT, FKTABLE_NAME, KEY_SEQ. Columns like JDBC: PKTABLE_CAT, PKTABLE_SCHEM, PKTABLE_NAME, PKCOLUMN_NAME, FKTABLE_CAT, FKTABLE_SCHEM,
	 * FKTABLE_NAME, FKCOLUMN_NAME, KEY_SEQ, UPDATE_RULE, DELETE_RULE, FK_NAME, PK_NAME, DEFERRABILITY.
	 * @param catalog the database name; nullopt matches all databases
	 * @param schema ignored (MySQL has no schemas below databases)
	 */
	std::unique_ptr<ResultSet> getExportedKeys(const std::optional<std::string>& catalog, const std::optional<std::string>& schema, std::string_view table);

	std::string getDatabaseProductVersion() const;

	Connection& getConnection() const noexcept { return *connection; }

private:
	friend class Connection;
	explicit DatabaseMetaData(Connection& connection) noexcept : connection(&connection) {}

	Connection* connection;
};

} // namespace aion::commons::database
