#include "aion/chatserver/configs/Config.h"

#include <memory>
#include <optional>

#include "aion/chatserver/configs/main/LoggingConfig.h"
#include "aion/chatserver/configs/network/NetworkConfig.h"
#include "aion/commons/configs/CommonsConfig.h"
#include "aion/commons/configs/DatabaseConfig.h"
#include "aion/commons/configuration/ConfigurableProcessor.h"
#include "aion/commons/configuration/PropertiesUtils.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/NetworkUtils.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::chatserver::configs {

using commons::configuration::ConfigurableProcessor;
using commons::configuration::Properties;
using network::NetworkConfig;
namespace PropertiesUtils = commons::configuration::PropertiesUtils;
namespace Logging = commons::logging::Logging;

namespace {

commons::logging::Logger logger() {
	return commons::logging::LoggerFactory::getLogger("com.aionemu.chatserver.configs.Config");
}

} // namespace

void Config::load(const Properties& overrides) {
	Properties properties = loadProperties();
	properties.putAll(overrides);
	std::set<std::string> unusedProperties = ConfigurableProcessor::process(properties, {&commons::configs::CommonsConfig::bind,
		&main::LoggingConfig::bind, &commons::configs::DatabaseConfig::bind, &NetworkConfig::bind});
	if (!overrides.isEmpty()) {
		std::vector<std::string> applied;
		for (const std::string& key : overrides.stringPropertyNames()) {
			if (!unusedProperties.contains(key))
				applied.push_back(key);
		}
		if (!applied.empty())
			logger().info("Applied override properties from the command line: " + commons::utils::StringUtils::join(applied, ", "));
	}
	if (!unusedProperties.empty()) {
		removePropertiesUsedInLogbackXml(unusedProperties);
		for (const std::string& p : unusedProperties)
			logger().warn("Config property " + p + " is unknown and therefore ignored.");
	}

	// Java resolves the host when the property is bound; the InetAddress of an unresolvable host is null and getAddress() below throws
	NetworkConfig::CLIENT_CONNECT_ADDRESS.resolveAddressBytes();
	if (NetworkConfig::CLIENT_CONNECT_ADDRESS.isAnyLocalAddress()) {
		std::optional<std::string> localIPv4 = commons::utils::NetworkUtils::findLocalIPv4();
		if (!localIPv4)
			throw commons::utils::Exception(
				"No connect IP for Aion client configured and local IP discovery failed. Please configure chatserver.network.client.connect_address");
		NetworkConfig::CLIENT_CONNECT_ADDRESS = {*localIPv4, NetworkConfig::CLIENT_CONNECT_ADDRESS.port};
		logger().info("No connect IP for Aion client configured, using " + *localIPv4);
	}
}

Properties Config::loadProperties() {
	const auto log = logger();
	auto defaults = std::make_shared<Properties>();
	try {
		for (const char* configDir : {"./config/main", "./config/network"}) {
			log.info("Loading default configuration values from: {}/*", configDir);
			PropertiesUtils::loadFromDirectory(*defaults, configDir, false);
		}
		log.info("Loading: ./config/mycs.properties");
		Properties properties = PropertiesUtils::load("./config/mycs.properties", defaults);
		if (properties.isEmpty())
			log.info("No override properties found");
		return properties;
	} catch (const std::exception&) {
		throw commons::utils::Exception("Can't load chatserver configuration:", std::current_exception());
	}
}

void Config::removePropertiesUsedInLogbackXml(std::set<std::string>& properties) {
	for (const std::string& key : getLogbackPropertyKeys())
		properties.erase(key);
}

std::vector<std::string> Config::getLogbackPropertyKeys() {
	std::vector<std::string> keys = Logging::getPropertyKeys("chatserver");
	keys.emplace_back("chatserver.log.chat.discord.webhook_url");
	keys.emplace_back("chatserver.log.chat.discord.avatar_url");
	return keys;
}

Properties Config::loadLogbackProperties() {
	Properties merged;
	for (const char* file : {"config/main/logging.properties", "config/mycs.properties"}) {
		try {
			merged.putAll(PropertiesUtils::load(file));
		} catch (const std::exception& e) {
			logger().warn("Could not read logging properties from " + std::string(file), e);
		}
	}
	return merged;
}

Logging::Config Config::loadLoggingConfig() {
	Properties properties = loadLogbackProperties();
	Logging::Config config;
	// logback trims the values of properties it defines
	const std::string webhookUrl = properties.getProperty("chatserver.log.status.discord.webhook_url", "");
	const std::string avatarUrl = properties.getProperty("chatserver.log.status.discord.avatar_url", "");
	config.statusDiscordWebhookUrl = std::string(commons::utils::StringUtils::trim(webhookUrl));
	config.statusDiscordAvatarUrl = std::string(commons::utils::StringUtils::trim(avatarUrl));
	return config;
}

} // namespace aion::chatserver::configs
