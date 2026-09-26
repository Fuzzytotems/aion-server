#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>

#include "aion/commons/database/IUStH.h"
#include "aion/commons/database/ParamReadStH.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ReadStH.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/Transaction.h"

namespace aion::commons::database {

/**
 * <b>DB Documentation</b>
 * <p>
 * This class is used for making SQL query's utilizing the database connection defined in database.properties. All methods get a connection from
 * DatabaseFactory and recycle it after completion. Errors are logged (logger com.aionemu.commons.database.DB) and reported by the return value,
 * except for beginTransaction.
 * <p>
 * <b>SELECT</b>
 * <pre>
 * DB::select("SELECT name FROM test_table WHERE id=?", [&](PreparedStatement& stmt) { stmt.setInt(1, 50); }, [&](ResultSet& rset) {
 * 	while (rset.next())
 * 		var = rset.getString("name");
 * });
 * </pre>
 * <b>INSERT / UPDATE</b> - if an IUStH is given, it MUST execute the statement or batch itself; otherwise the query is executed as it is:
 * <pre>
 * DB::insertUpdate("UPDATE test_table SET some_column=1");
 * DB::insertUpdate("INSERT INTO test_table VALUES (?)", [&](PreparedStatement& stmt) {
 * 	for (std::string_view n : {"bob", "mike", "joe"}) {
 * 		stmt.setString(1, n);
 * 		stmt.addBatch();
 * 	}
 * 	stmt.executeBatch(); // REQUIRED
 * });
 * </pre>
 *
 * @author Disturbing
 */
class DB {
public:
	DB() = delete;

	/**
	 * Executes Select Query. Uses ReadSth to utilize params and return data. Recycles connection after completion.
	 * @return true if the query ran successfully
	 */
	static bool select(std::string_view query, const ReadStH& reader);

	/** Java: select(query, ParamReadStH) - setParams (if set) is called before execution. */
	static bool select(std::string_view query, const ParamReadStH& reader);

	/** C++ convenience for select(query, ParamReadStH{setParams, handleRead}) */
	static bool select(std::string_view query, const std::function<void(PreparedStatement&)>& setParams, const ReadStH& handleRead);

	/**
	 * Call stored procedure
	 * @return true if the call ran successfully
	 */
	static bool call(std::string_view query, const ReadStH& reader);
	static bool call(std::string_view query, const CallReadStH& reader);

	/**
	 * Executes Insert or Update Query not needing any further modification or batching. Recycles connection after completion.
	 * @return true if the query ran successfully
	 */
	static bool insertUpdate(std::string_view query);

	/**
	 * Executes Insert / Update Query. Utilizes IUSth for Batching and Query Editing. MUST MANUALLY EXECUTE QUERY / BATCH IN IUSth (No need to close
	 * Statement after execution). Recycles connection after completion. An empty IUStH executes the query as it is.
	 * @return true if the query ran successfully
	 */
	static bool insertUpdate(std::string_view query, const IUStH& batch);

	/**
	 * Begins new transaction
	 * @throws SQLException if was unable to create transaction
	 */
	static Transaction beginTransaction();

	/**
	 * Creates PreparedStatement with given sql string. Statements are created with ResultSet::TYPE_FORWARD_ONLY and ResultSet::CONCUR_READ_ONLY.
	 * The statement owns its pooled connection, which is recycled when the statement is closed or destroyed (see close()).
	 * @return Prepared statement if ok or nullptr if error happened while creating
	 */
	static std::unique_ptr<PreparedStatement> prepareStatement(std::string_view sql);

	/**
	 * Creates PreparedStatement with given sql and result set type/concurrency (see prepareStatement(sql)).
	 * @return Prepared Statement if ok or nullptr if error happened while creating
	 */
	static std::unique_ptr<PreparedStatement> prepareStatement(std::string_view sql, int32_t resultSetType, int32_t resultSetConcurrency);

	/**
	 * Executes PreparedStatement
	 * @return result of PreparedStatement::executeUpdate() or -1 in case of error
	 */
	static int32_t executeUpdate(PreparedStatement* statement);

	/** Executes PreparedStatement and closes it and it's connection */
	static void executeUpdateAndClose(std::unique_ptr<PreparedStatement>& statement);

	/**
	 * Executes query and returns ResultSet
	 * @return ResultSet or nullptr if error
	 */
	static std::unique_ptr<ResultSet> executeQuerry(PreparedStatement* statement);

	/** Closes PreparedStatement and it's connection (the pointer becomes empty). Does nothing for an empty pointer. */
	static void close(std::unique_ptr<PreparedStatement>& statement);
};

} // namespace aion::commons::database
