#include "aion/gameserver/configs/Config.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <exception>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "aion/commons/configs/CommonsConfig.h"
#include "aion/commons/configs/DatabaseConfig.h"
#include "aion/commons/configuration/ConfigurableProcessor.h"
#include "aion/commons/configuration/PropertiesUtils.h"
#include "aion/commons/configuration/transformers/ZoneIdTransformer.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/NetworkUtils.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/configs/main/AutoGroupConfig.h"
#include "aion/gameserver/configs/main/CleaningConfig.h"
#include "aion/gameserver/configs/main/CraftConfig.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/DropConfig.h"
#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/configs/main/FallDamageConfig.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/configs/main/GroupConfig.h"
#include "aion/gameserver/configs/main/HTMLConfig.h"
#include "aion/gameserver/configs/main/HousingConfig.h"
#include "aion/gameserver/configs/main/InstanceConfig.h"
#include "aion/gameserver/configs/main/LegionConfig.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/MembershipConfig.h"
#include "aion/gameserver/configs/main/NameConfig.h"
#include "aion/gameserver/configs/main/PeriodicSaveConfig.h"
#include "aion/gameserver/configs/main/PlayerTransferConfig.h"
#include "aion/gameserver/configs/main/PricesConfig.h"
#include "aion/gameserver/configs/main/PunishmentConfig.h"
#include "aion/gameserver/configs/main/RankingConfig.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/configs/main/RuntimeConfig.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/configs/main/ShutdownConfig.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/configs/main/ThreadConfig.h"
#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/configs/network/PffConfig.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::configs {

using commons::configuration::ConfigurableProcessor;
using commons::configuration::Properties;
namespace PropertiesUtils = commons::configuration::PropertiesUtils;
namespace Logging = commons::logging::Logging;

namespace {

const commons::logging::Logger& logger() {
	static const auto* instance = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.configs.Config"));
	return *instance;
}

#define AION_CONFIG_CLASS(package, simpleName)                                                                                                       \
	Config::ConfigClass {                                                                                                                              \
		#simpleName, "com.aionemu.gameserver.configs." #package "." #simpleName, &package::simpleName::bind                                              \
	}

/** Java: Config.CONFIGS, in the same order, plus the C++-only RuntimeConfig at the end */
constexpr std::array CONFIGS{
  AION_CONFIG_CLASS(administration, AdminConfig),
  AION_CONFIG_CLASS(administration, CommandsConfig),
  AION_CONFIG_CLASS(main, AIConfig),
  AION_CONFIG_CLASS(main, AutoGroupConfig),
  Config::ConfigClass{"CommonsConfig", "com.aionemu.commons.configs.CommonsConfig", &commons::configs::CommonsConfig::bind},
  AION_CONFIG_CLASS(main, CleaningConfig),
  AION_CONFIG_CLASS(main, CraftConfig),
  AION_CONFIG_CLASS(main, CustomConfig),
  AION_CONFIG_CLASS(main, DropConfig),
  AION_CONFIG_CLASS(main, EventsConfig),
  AION_CONFIG_CLASS(main, FallDamageConfig),
  AION_CONFIG_CLASS(main, GSConfig),
  AION_CONFIG_CLASS(main, GeoDataConfig),
  AION_CONFIG_CLASS(main, GroupConfig),
  AION_CONFIG_CLASS(main, HousingConfig),
  AION_CONFIG_CLASS(main, HTMLConfig),
  AION_CONFIG_CLASS(main, InstanceConfig),
  AION_CONFIG_CLASS(main, LegionConfig),
  AION_CONFIG_CLASS(main, LoggingConfig),
  AION_CONFIG_CLASS(main, MembershipConfig),
  AION_CONFIG_CLASS(main, NameConfig),
  AION_CONFIG_CLASS(main, PeriodicSaveConfig),
  AION_CONFIG_CLASS(main, PlayerTransferConfig),
  AION_CONFIG_CLASS(main, PricesConfig),
  AION_CONFIG_CLASS(main, PunishmentConfig),
  AION_CONFIG_CLASS(main, RankingConfig),
  AION_CONFIG_CLASS(main, RatesConfig),
  AION_CONFIG_CLASS(main, SecurityConfig),
  AION_CONFIG_CLASS(main, ShutdownConfig),
  AION_CONFIG_CLASS(main, SiegeConfig),
  AION_CONFIG_CLASS(main, ThreadConfig),
  AION_CONFIG_CLASS(main, WorldConfig),
  Config::ConfigClass{"DatabaseConfig", "com.aionemu.commons.configs.DatabaseConfig", &commons::configs::DatabaseConfig::bind},
  AION_CONFIG_CLASS(network, NetworkConfig),
  AION_CONFIG_CLASS(network, PffConfig),
  Config::ConfigClass{"RuntimeConfig", "aion::gameserver::configs::main::RuntimeConfig", &main::RuntimeConfig::bind},
};

#undef AION_CONFIG_CLASS

/** Serializes load() (see Config.h) */
runtime::Monitor loadLock{AION_LOCK_CLASS(Config::load)};

std::atomic<std::shared_ptr<const Config::EventConfigPropertiesProvider>> eventConfigPropertiesProvider;
std::atomic<std::shared_ptr<const Config::LocalIPv4Finder>> localIPv4Finder;

} // namespace

void Config::load() {
	load(std::span<const BindFunction>());
}

void Config::load(std::initializer_list<BindFunction> allowedConfigs) {
	load(std::span<const BindFunction>(allowedConfigs.begin(), allowedConfigs.size()));
}

void Config::load(std::span<const BindFunction> allowedConfigs) {
	Properties properties = loadProperties();
	if (auto provider = eventConfigPropertiesProvider.load(); provider && *provider)
		properties.putAll((*provider)());

	std::vector<ConfigurableProcessor::Binder> binders;
	for (BindFunction config : allowedConfigs) {
		if (std::ranges::none_of(CONFIGS, [config](const ConfigClass& c) { return c.bind == config; }))
			throw commons::utils::IllegalArgumentException("Config bind function is not an allowed config");
		binders.emplace_back(config);
	}
	const bool processAllConfigs = allowedConfigs.empty();
	if (processAllConfigs) {
		for (const ConfigClass& config : CONFIGS)
			binders.emplace_back(config.bind);
	}

	SYNCHRONIZED(loadLock) {
		std::set<std::string> unusedProperties = ConfigurableProcessor::process(properties, binders);
		if (processAllConfigs && !unusedProperties.empty()) {
			// Java: removePropertiesUsedInLogbackXml(unusedProperties)
			for (const std::string& key : Logging::getPropertyKeys("gameserver"))
				unusedProperties.erase(key);
			for (const std::string& unusedProperty : unusedProperties)
				logger().warn("Config property " + unusedProperty + " is unknown and therefore ignored.");
		}

		auto clientConnectAddress = network::NetworkConfig::CLIENT_CONNECT_ADDRESS.get();
		if (clientConnectAddress->isAnyLocalAddress()) {
			auto finder = localIPv4Finder.load();
			std::optional<std::string> localIPv4 = finder && *finder ? (*finder)() : commons::utils::NetworkUtils::findLocalIPv4();
			if (!localIPv4)
				throw commons::utils::Exception("No IP for Aion client advertisement configured and local IP discovery failed. Please configure "
				                                "gameserver.network.client.connect_address");
			network::NetworkConfig::CLIENT_CONNECT_ADDRESS.set(commons::utils::InetSocketAddress{*localIPv4, clientConnectAddress->port});
			logger().info("No IP for Aion client advertisement configured, using " + *localIPv4);
		}
	}
}

std::span<const Config::ConfigClass> Config::getClasses() {
	return CONFIGS;
}

void Config::setEventConfigPropertiesProvider(EventConfigPropertiesProvider provider) {
	eventConfigPropertiesProvider.store(provider ? std::make_shared<const EventConfigPropertiesProvider>(std::move(provider)) : nullptr);
}

void Config::setLocalIPv4Finder(LocalIPv4Finder finder) {
	localIPv4Finder.store(finder ? std::make_shared<const LocalIPv4Finder>(std::move(finder)) : nullptr);
}

Properties Config::loadProperties() {
	auto defaults = std::make_shared<Properties>();
	try {
		for (const char* configDir : {"./config/administration", "./config/main", "./config/network"}) {
			logger().info("Loading default configuration values from: " + std::string(configDir) + "/*");
			PropertiesUtils::loadFromDirectory(*defaults, configDir, false);
		}
		logger().info("Loading: ./config/mygs.properties");
		Properties properties = PropertiesUtils::load("./config/mygs.properties", defaults);
		if (properties.isEmpty())
			logger().info("No override properties found");
		return properties;
	} catch (const std::exception&) {
		throw commons::utils::Exception("Can't load gameserver configuration:", std::current_exception());
	}
}

Logging::Config Config::loadLoggingConfig() {
	Properties merged;
	// logback.xml: <property file="config/main/gameserver.properties" /> <property file="config/main/logging.properties" />
	// <property file="config/mygs.properties" />
	for (const char* file : {"config/main/gameserver.properties", "config/main/logging.properties", "config/mygs.properties"}) {
		try {
			merged.putAll(PropertiesUtils::load(file));
		} catch (const std::exception& e) {
			logger().warn("Could not read logging properties from " + std::string(file), e);
		}
	}
	// logback trims the values of properties it defines
	auto trimmed = [&merged](std::string_view key) { return std::string(commons::utils::StringUtils::trim(merged.getProperty(key, ""))); };
	Logging::Config config;
	const std::string timeZone = trimmed("gameserver.timezone");
	config.timeZone = timeZone.empty() ? nullptr : commons::configuration::transformers::ZoneIdTransformer::of(timeZone);
	config.statusDiscordWebhookUrl = trimmed("gameserver.log.status.discord.webhook_url");
	config.statusDiscordAvatarUrl = trimmed("gameserver.log.status.discord.avatar_url");
	return config;
}

} // namespace aion::gameserver::configs
