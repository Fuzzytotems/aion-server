#include "aion/commons/database/DB.h"

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"

namespace aion::commons::database {

namespace {

const auto log = logging::LoggerFactory::getLogger("com.aionemu.commons.database.DB");

} // namespace

bool DB::select(std::string_view query, const ReadStH& reader) {
	return select(query, ParamReadStH{nullptr, reader});
}

bool DB::select(std::string_view query, const std::function<void(PreparedStatement&)>& setParams, const ReadStH& handleRead) {
	return select(query, ParamReadStH{setParams, handleRead});
}

bool DB::select(std::string_view query, const ParamReadStH& reader) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(query);
		if (reader.setParams)
			reader.setParams(*stmt);
		auto rset = stmt->executeQuery();
		if (reader.handleRead)
			reader.handleRead(*rset);
	} catch (const std::exception& e) {
		log.error("Error executing select query " + std::string(query), e);
		return false;
	}
	return true;
}

bool DB::call(std::string_view query, const ReadStH& reader) {
	return call(query, CallReadStH{nullptr, reader});
}

bool DB::call(std::string_view query, const CallReadStH& reader) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareCall(query);
		if (reader.setParams)
			reader.setParams(*stmt);
		auto rset = stmt->executeQuery();
		if (reader.handleRead)
			reader.handleRead(*rset);
	} catch (const std::exception& e) {
		log.error("Error calling stored procedure " + std::string(query), e);
		return false;
	}
	return true;
}

bool DB::insertUpdate(std::string_view query) {
	return insertUpdate(query, nullptr);
}

bool DB::insertUpdate(std::string_view query, const IUStH& batch) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(query);
		if (batch)
			batch(*stmt);
		else
			stmt->executeUpdate();
	} catch (const std::exception& e) {
		log.error("Failed to execute IU query " + std::string(query), e);
		return false;
	}
	return true;
}

Transaction DB::beginTransaction() {
	return Transaction(DatabaseFactory::getConnection());
}

std::unique_ptr<PreparedStatement> DB::prepareStatement(std::string_view sql) {
	return prepareStatement(sql, ResultSet::TYPE_FORWARD_ONLY, ResultSet::CONCUR_READ_ONLY);
}

std::unique_ptr<PreparedStatement> DB::prepareStatement(std::string_view sql, int32_t resultSetType, int32_t resultSetConcurrency) {
	try {
		PooledConnection c = DatabaseFactory::getConnection();
		std::unique_ptr<PreparedStatement> ps = c->prepareStatement(sql, resultSetType, resultSetConcurrency);
		ps->setOwnedConnection(std::make_shared<PooledConnection>(std::move(c)));
		return ps;
	} catch (const std::exception& e) {
		// the connection (if any) was recycled by its handle
		log.error("Can't create PreparedStatement for query: " + std::string(sql), e);
	}
	return nullptr;
}

int32_t DB::executeUpdate(PreparedStatement* statement) {
	try {
		if (!statement)
			throw utils::IllegalArgumentException("statement is null");
		return statement->executeUpdate();
	} catch (const std::exception& e) {
		log.error("Can't execute update for PreparedStatement", e);
	}
	return -1;
}

void DB::executeUpdateAndClose(std::unique_ptr<PreparedStatement>& statement) {
	executeUpdate(statement.get());
	close(statement);
}

std::unique_ptr<ResultSet> DB::executeQuerry(PreparedStatement* statement) {
	try {
		if (!statement)
			throw utils::IllegalArgumentException("statement is null");
		return statement->executeQuery();
	} catch (const std::exception& e) {
		log.error("Error while executing query", e);
	}
	return nullptr;
}

void DB::close(std::unique_ptr<PreparedStatement>& statement) {
	if (!statement) // e.g. nullptr from failed DB::prepareStatement call
		return;
	if (statement->isClosed()) {
		// Deviation: Java returns here without touching the statement; resetting the pointer also releases a connection it might still own
		log.warn("Attempt to close PreparedStatement that is closed already", utils::Exception(""));
		statement.reset();
		return;
	}
	statement.reset();
}

} // namespace aion::commons::database
