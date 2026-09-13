#include "aion/commons/database/DatabaseMetaData.h"

#include "aion/commons/database/Connection.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"

namespace aion::commons::database {

std::unique_ptr<ResultSet> DatabaseMetaData::getExportedKeys(const std::optional<std::string>& catalog, const std::optional<std::string>&,
	std::string_view table) {
	// same columns and rule codes as Connector/J's DatabaseMetaDataUsingInfoSchema (importedKeyCascade = 0, ..., importedKeyNotDeferrable = 7)
	auto rule = [](const char* column) {
		return std::string("CASE WHEN R.") + column + "='CASCADE' THEN 0 WHEN R." + column + "='SET NULL' THEN 2 WHEN R." + column +
			"='SET DEFAULT' THEN 4 WHEN R." + column + "='RESTRICT' THEN 1 WHEN R." + column + "='NO ACTION' THEN 3 ELSE 3 END";
	};
	std::string sql = "SELECT A.REFERENCED_TABLE_SCHEMA AS PKTABLE_CAT, NULL AS PKTABLE_SCHEM, A.REFERENCED_TABLE_NAME AS PKTABLE_NAME, "
										"A.REFERENCED_COLUMN_NAME AS PKCOLUMN_NAME, A.TABLE_SCHEMA AS FKTABLE_CAT, NULL AS FKTABLE_SCHEM, A.TABLE_NAME AS FKTABLE_NAME, "
										"A.COLUMN_NAME AS FKCOLUMN_NAME, A.ORDINAL_POSITION AS KEY_SEQ, " +
		rule("UPDATE_RULE") + " AS UPDATE_RULE, " + rule("DELETE_RULE") +
		" AS DELETE_RULE, A.CONSTRAINT_NAME AS FK_NAME, R.UNIQUE_CONSTRAINT_NAME AS PK_NAME, 7 AS DEFERRABILITY "
		"FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE A JOIN INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS R "
		"ON (R.CONSTRAINT_NAME = A.CONSTRAINT_NAME AND R.TABLE_NAME = A.TABLE_NAME AND R.CONSTRAINT_SCHEMA = A.TABLE_SCHEMA) "
		"WHERE A.REFERENCED_TABLE_NAME = ?";
	if (catalog)
		sql += " AND A.REFERENCED_TABLE_SCHEMA = ?";
	sql += " ORDER BY A.TABLE_SCHEMA, A.TABLE_NAME, A.ORDINAL_POSITION";
	auto statement = connection->prepareStatement(sql);
	statement->setString(1, table);
	if (catalog)
		statement->setString(2, *catalog);
	return statement->executeQuery();
}

std::string DatabaseMetaData::getDatabaseProductVersion() const {
	return connection->getServerVersion();
}

} // namespace aion::commons::database
