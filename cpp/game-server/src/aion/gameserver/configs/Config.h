#pragma once

#include <functional>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "aion/commons/configuration/Properties.h"
#include "aion/commons/logging/Logging.h"

namespace aion::commons::configuration {
class ConfigurableProcessor;
}

namespace aion::gameserver::configs {

/**
 * Loads the game server configuration into the config classes (configs/administration, configs/main, configs/network, commons' CommonsConfig and
 * DatabaseConfig, and the C++-only RuntimeConfig).
 * <p>
 * Property sources, later ones win (Java: Config.loadProperties):
 * <ol>
 * <li>./config/administration/*.properties, ./config/main/*.properties, ./config/network/*.properties (defaults)</li>
 * <li>./config/mygs.properties (operator overrides)</li>
 * <li>the properties of active events (Java: EventService.getInstance().getActiveEventConfigProperties()). EventService is not ported yet, so
 * the event service registers a provider with setEventConfigPropertiesProvider; without a provider there are no event properties.</li>
 * </ol>
 * <b>Threads.</b> load() is called at startup and again while the server runs (event start/stop, //reload config, ChatProcessor.reload with
 * CommandsConfig). All config fields are std::atomic or ConfigValue (see detail/ConfigSupport.h), so readers on other threads are safe. Calls to
 * load() are serialized by a Monitor: Java runs concurrent reloads unsynchronized, which is memory safe there but would be a data race in C++ on
 * the plain fields of commons' DatabaseConfig and on the local IP discovery below (deviation, see DEVIATIONS.md). A reload is not transactional,
 * like in Java: readers can observe a mix of old and new values while it runs.
 * <p>
 * Java: com.aionemu.gameserver.configs.Config
 *
 * @author Nemesiss, SoulKeeper
 */
struct Config {
	/** A config class, identified by its bind function (Java: the Class object). */
	using BindFunction = void (*)(commons::configuration::ConfigurableProcessor&);

	/** One entry of getClasses(). */
	struct ConfigClass {
		/** Java Class.getSimpleName(), e.g. "GSConfig" */
		std::string_view simpleName;
		/** Java Class.getName(), e.g. "com.aionemu.gameserver.configs.main.GSConfig" (RuntimeConfig: its C++ name, it has no Java class) */
		std::string_view name;
		BindFunction bind;
	};

	/** Returns the config properties of the active events (Java: EventService.getActiveEventConfigProperties()). */
	using EventConfigPropertiesProvider = std::function<commons::configuration::Properties()>;

	/** Returns the outbound local IPv4 address (Java: NetworkUtils.findLocalIPv4()), or std::nullopt if it cannot be determined. */
	using LocalIPv4Finder = std::function<std::optional<std::string>()>;

	/**
	 * Loads all configs (Java: load() without arguments). Warns "Config property x is unknown and therefore ignored." for every property that no
	 * config class uses, except the logging properties that logback.xml reads in Java (Logging::getPropertyKeys("gameserver")).
	 * <p>
	 * If NetworkConfig::CLIENT_CONNECT_ADDRESS is a wildcard address (0.0.0.0), it is replaced by the local IPv4 address and the same port, and
	 * "No IP for Aion client advertisement configured, using x" is logged.
	 *
	 * @throws commons::utils::Exception "Can't load gameserver configuration:" with the I/O error as cause (Java: GameServerError), or "No IP for
	 *           Aion client advertisement configured and local IP discovery failed. ..." (Java: GameServerError)
	 * @throws commons::configuration::TransformationException if a property value is invalid (fields bound before keep their new values)
	 */
	static void load();

	/**
	 * Loads the given configs only (Java: load(Class...)), e.g. {@code Config::load({&CommandsConfig::bind})}. No unknown-property warnings are
	 * logged. An empty list loads all configs like load().
	 *
	 * @throws commons::utils::IllegalArgumentException "x is not an allowed config" if a bind function is not one of getClasses()
	 */
	static void load(std::initializer_list<BindFunction> allowedConfigs);
	static void load(std::span<const BindFunction> allowedConfigs);

	/** Java: getClasses() - all config classes in the order they are bound. */
	static std::span<const ConfigClass> getClasses();

	/**
	 * C++ addition (EventService is not ported): registers the provider of event config properties for later load() calls; an empty function
	 * removes it. The provider runs on the thread calling load(), outside the load Monitor, and must not call load() itself. Thread-safe.
	 */
	static void setEventConfigPropertiesProvider(EventConfigPropertiesProvider provider);

	/**
	 * C++ addition: replaces NetworkUtils::findLocalIPv4 for the client connect address discovery (tests); an empty function restores it.
	 * Thread-safe.
	 */
	static void setLocalIPv4Finder(LocalIPv4Finder finder);

	/**
	 * C++ addition: the settings for Logging::init, which the startup code calls before load() like Java's GameServer calls Logging.init() before
	 * Config.load(). Java's config/logback.xml reads its properties itself; this mirrors it: config/main/gameserver.properties,
	 * config/main/logging.properties, then config/mygs.properties (values of later files win, values are trimmed, missing files are ignored,
	 * an unreadable file logs a warning).
	 *
	 * @return the default Logging::Config with timeZone (gameserver.timezone, nullptr = system default, like logback's empty zone),
	 *         statusDiscordWebhookUrl (gameserver.log.status.discord.webhook_url) and statusDiscordAvatarUrl (gameserver.log.status.discord.avatar_url)
	 * @throws commons::utils::IllegalArgumentException if gameserver.timezone is not a valid time zone ID
	 */
	static commons::logging::Logging::Config loadLoggingConfig();

private:
	static commons::configuration::Properties loadProperties();
};

} // namespace aion::gameserver::configs
