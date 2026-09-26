#pragma once

#include <optional>
#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/IUStH.h"
#include "aion/commons/database/Savepoint.h"

namespace aion::commons::database {

/**
 * This class allows easy manipulations with transactions, it should be used when critical or synchronized data should be commited to two or more
 * tables. This class allows us to avoid data synchronization problems in db.
 * <p/>
 * Class is not designed to be thread-safe, should be synchronized externally.<br>
 * Class is not fail-safe, if error happens - exception will be thrown.
 * <p>
 * Deviation: a Transaction owns its pooled connection. commit() returns it to the pool. If a Transaction is destroyed without commit (e.g. because
 * an exception was thrown), the pool rolls the transaction back and restores auto-commit (Java would leak the connection). Movable.
 *
 * @author SoulKeeper
 */
class Transaction {
public:
	Transaction(Transaction&&) noexcept = default;
	Transaction& operator=(Transaction&&) noexcept = default;

	/**
	 * Adds Insert / Update Query to the transaction
	 * @throws SQLException if something went wrong
	 */
	void insertUpdate(std::string_view sql);

	/**
	 * Adds Insert / Update Query to this transaction. Utilizes IUSth for Batching and Query Editing. MUST MANUALLY EXECUTE QUERY / BATCH IN IUSth
	 * (No need to close Statement after execution)
	 * @throws SQLException if something went wrong
	 */
	void insertUpdate(std::string_view sql, const IUStH& iusth);

	/**
	 * Creates new savepoint
	 * @throws SQLException if can't create save point
	 */
	Savepoint setSavepoint(std::string_view name);

	/**
	 * Releases savepoint of transaction
	 * @throws SQLException if something went wrong
	 */
	void releaseSavepoint(const Savepoint& savepoint);

	/**
	 * Commits transaction
	 * @throws SQLException if something is wrong with transaction
	 */
	void commit();

	/**
	 * Commits transaction. If rollBackToOnError is empty - whole transaction will be rolledback. Errors of the commit itself are logged, not
	 * thrown. The connection is returned to the pool afterwards.
	 * @throws SQLException if auto-commit cannot be restored, or if the transaction was already committed
	 */
	void commit(const std::optional<Savepoint>& rollBackToOnError);

private:
	friend class DB;

	/**
	 * Should be instantiated via DB::beginTransaction()
	 * @throws SQLException if can't disable autocommit mode
	 */
	explicit Transaction(PooledConnection connection);

	PooledConnection connection;
};

} // namespace aion::commons::database
