#include "aion/commons/database/Transaction.h"

#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::commons::database {

namespace {

const auto log = logging::LoggerFactory::getLogger("com.aionemu.commons.database.Transaction");

} // namespace

Transaction::Transaction(PooledConnection con) : connection(std::move(con)) {
	connection->setAutoCommit(false);
}

void Transaction::insertUpdate(std::string_view sql) {
	insertUpdate(sql, nullptr);
}

void Transaction::insertUpdate(std::string_view sql, const IUStH& iusth) {
	auto statement = connection->prepareStatement(sql);
	if (iusth)
		iusth(*statement);
	else
		statement->executeUpdate();
}

Savepoint Transaction::setSavepoint(std::string_view name) {
	return connection->setSavepoint(name);
}

void Transaction::releaseSavepoint(const Savepoint& savepoint) {
	connection->releaseSavepoint(savepoint);
}

void Transaction::commit() {
	commit(std::nullopt);
}

void Transaction::commit(const std::optional<Savepoint>& rollBackToOnError) {
	try {
		connection->commit();
	} catch (const SQLException& e) {
		log.warn("Error while commiting transaction", e);
		try {
			if (rollBackToOnError)
				connection->rollback(*rollBackToOnError);
			else
				connection->rollback();
		} catch (const SQLException& e1) {
			log.error("Can't rollback transaction", e1);
		}
	}
	connection->setAutoCommit(true);
	connection.close();
}

} // namespace aion::commons::database
