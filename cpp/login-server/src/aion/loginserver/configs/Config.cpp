#include "aion/loginserver/configs/Config.h"

#include <memory>
#include <set>

#include "aion/commons/configs/CommonsConfig.h"
#include "aion/commons/configs/DatabaseConfig.h"
#include "aion/commons/configuration/ConfigurableProcessor.h"
#include "aion/commons/configuration/PropertiesUtils.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::loginserver::configs {

using commons::configuration::ConfigurableProcessor;
using commons::configuration::Properties;
namespace PropertiesUtils = commons::configuration::PropertiesUtils;
namespace Logging = commons::logging::Logging;

namespace {

commons::logging::Logger logger() {
	return commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.configs.Config");
}

} // namespace

void Config::bind(ConfigurableProcessor& p) {
	p.bind("loginserver.network.client.socket_address", CLIENT_SOCKET_ADDRESS, "0.0.0.0:2106");
	p.bind("loginserver.network.gameserver.socket_address", GAMESERVER_SOCKET_ADDRESS, "0.0.0.0:9014");
	p.bind("loginserver.network.client.logintrybeforeban", LOGIN_TRY_BEFORE_BAN, "5");
	p.bind("loginserver.network.client.bantimeforbruteforcing", WRONG_LOGIN_BAN_TIME, "15");
	p.bind("loginserver.network.nio.threads", NIO_READ_WRITE_THREADS, "0");
	p.bind("loginserver.accounts.autocreate", ACCOUNT_AUTO_CREATION, "true");
	p.bind("loginserver.accounts.external_auth.url", EXTERNAL_AUTH_URL, "");
	p.bind("loginserver.server.bruteforceprotector", ENABLE_BRUTEFORCE_PROTECTION, "true");
	p.bind("loginserver.log.logins", LOG_LOGINS, "false");
}

bool Config::useExternalAuth() {
	return !commons::utils::StringUtils::isBlank(EXTERNAL_AUTH_URL);
}

void Config::load() {
	Properties properties = loadProperties();
	std::set<std::string> unusedProperties =
		ConfigurableProcessor::process(properties, {&Config::bind, &commons::configs::CommonsConfig::bind, &commons::configs::DatabaseConfig::bind});
	if (!unusedProperties.empty()) {
		// Java: removePropertiesUsedInLogbackXml(unusedProperties)
		for (const std::string& key : Logging::getPropertyKeys("loginserver"))
			unusedProperties.erase(key);
		for (const std::string& unusedProperty : unusedProperties)
			logger().warn("Config property " + unusedProperty + " is unknown and therefore ignored.");
	}
}

Properties Config::loadProperties() {
	const auto log = logger();
	auto defaults = std::make_shared<Properties>();
	try {
		log.info("Loading default configuration values from: ./config/main/*");
		PropertiesUtils::loadFromDirectory(*defaults, "./config/main", false);
		log.info("Loading default configuration values from: ./config/network/*");
		PropertiesUtils::loadFromDirectory(*defaults, "./config/network", false);
		log.info("Loading: ./config/myls.properties");
		Properties properties = PropertiesUtils::load("./config/myls.properties", defaults);
		if (properties.isEmpty())
			log.info("No override properties found");
		return properties;
	} catch (const std::exception&) {
		throw commons::utils::Exception("Can't load loginserver configuration:", std::current_exception());
	}
}

Logging::Config Config::loadLoggingConfig() {
	Properties merged;
	// logback.xml: <property file="config/main/logging.properties" /> <property file="config/myls.properties" />
	for (const char* file : {"config/main/logging.properties", "config/myls.properties"}) {
		try {
			merged.putAll(PropertiesUtils::load(file));
		} catch (const std::exception& e) {
			logger().warn("Could not read logging properties from " + std::string(file), e);
		}
	}
	Logging::Config config;
	// logback trims the values of properties it defines
	const std::string webhookUrl = merged.getProperty("loginserver.log.status.discord.webhook_url", "");
	const std::string avatarUrl = merged.getProperty("loginserver.log.status.discord.avatar_url", "");
	config.statusDiscordWebhookUrl = std::string(commons::utils::StringUtils::trim(webhookUrl));
	config.statusDiscordAvatarUrl = std::string(commons::utils::StringUtils::trim(avatarUrl));
	return config;
}

} // namespace aion::loginserver::configs
