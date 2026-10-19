#include "aion/commons/configs/DatabaseConfig.h"

#include "aion/commons/configuration/ConfigurableProcessor.h"
#include "aion/commons/configuration/transformers/JavaString.h"

namespace aion::commons::configs {

void DatabaseConfig::bind(configuration::ConfigurableProcessor& processor) {
	processor.bindNamed<configuration::transformers::BasicJavaString>("DATABASE_URL", "database.url", DATABASE_URL);
	processor.bindNamed<configuration::transformers::BasicJavaString>("DATABASE_USER", "database.user", DATABASE_USER);
	processor.bindNamed<configuration::transformers::BasicJavaString>("DATABASE_PASSWORD", "database.password", DATABASE_PASSWORD);
	processor.bindNamed<configuration::transformers::BasicJavaString>("DATABASE_CONNECTIONS_MAX", "database.connectionpool.connections.max",
		DATABASE_CONNECTIONS_MAX, "5");
	processor.bindNamed<configuration::transformers::BasicJavaString>("DATABASE_TIMEOUT", "database.connectionpool.timeout", DATABASE_TIMEOUT, "5000");
	// C++ addition, no default: see DatabaseFactory::Options
	processor.bindNamed<configuration::transformers::BasicJavaString>("DATABASE_SOCKET_TIMEOUT", "database.socket_timeout", DATABASE_SOCKET_TIMEOUT);
}

} // namespace aion::commons::configs
