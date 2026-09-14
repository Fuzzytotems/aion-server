#include "aion/commons/configs/DatabaseConfig.h"

#include "aion/commons/configuration/ConfigurableProcessor.h"

namespace aion::commons::configs {

void DatabaseConfig::bind(configuration::ConfigurableProcessor& processor) {
	processor.bind("database.url", DATABASE_URL);
	processor.bind("database.user", DATABASE_USER);
	processor.bind("database.password", DATABASE_PASSWORD);
	processor.bind("database.connectionpool.connections.max", DATABASE_CONNECTIONS_MAX, "5");
	processor.bind("database.connectionpool.timeout", DATABASE_TIMEOUT, "5000");
	processor.bind("database.socket_timeout", DATABASE_SOCKET_TIMEOUT); // C++ addition, no default: see DatabaseFactory::Options
}

} // namespace aion::commons::configs
