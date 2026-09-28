#pragma once

#include <string>

namespace aion::commons::database {

/**
 * Java: java.sql.Savepoint (Connector/J MysqlSavepoint). Created by Connection::setSavepoint; unnamed savepoints get a generated unique name.
 */
class Savepoint {
public:
	explicit Savepoint(std::string name) : name(std::move(name)) {}

	const std::string& getSavepointName() const noexcept { return name; }

private:
	std::string name;
};

} // namespace aion::commons::database
