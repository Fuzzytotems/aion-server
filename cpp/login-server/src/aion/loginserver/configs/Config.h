#pragma once

#include <cstdint>
#include <string>

#include "aion/commons/configuration/Properties.h"
#include "aion/commons/logging/Logging.h"
#include "aion/commons/utils/InetSocketAddress.h"

namespace aion::commons::configuration {
class ConfigurableProcessor;
}

namespace aion::loginserver::configs {

/**
 * The login server configuration: the fields bound from ./config/main/*.properties, ./config/network/*.properties and the optional override file
 * ./config/myls.properties (see load()).
 * <p>
 * <b>Threads:</b> the login server loads its configuration once at startup, before any other thread exists, and never rebinds it (there is no
 * reload command). After load() the fields are only read, so they are plain fields and may be read concurrently from any thread (see
 * CONVENTIONS.md › Configuration). Do not assign them while the server runs.
 * <p>
 * Java: com.aionemu.loginserver.configs.Config
 *
 * @author -Nemesiss-, SoulKeeper, Neon
 */
struct Config {
	/** Local address where LS will listen for Aion client connections (0.0.0.0 = bind any local IP) */
	static inline commons::utils::InetSocketAddress CLIENT_SOCKET_ADDRESS;

	/** Local address where LS will listen for GS connections (0.0.0.0 = bind any local IP) */
	static inline commons::utils::InetSocketAddress GAMESERVER_SOCKET_ADDRESS;

	/** Number of login tries before ban */
	static inline int32_t LOGIN_TRY_BEFORE_BAN = 0;

	/** Ban time in minutes */
	static inline int32_t WRONG_LOGIN_BAN_TIME = 0;

	/**
	 * Number of threads dedicated to be doing io read & write. There is always 1 acceptor thread. If value is < 1 - acceptor thread will also
	 * handle read & write. If value is > 0 - there will be given amount of read & write threads + 1 acceptor thread.
	 */
	static inline int32_t NIO_READ_WRITE_THREADS = 0;

	/** Should server automatically create accounts for users or not? */
	static inline bool ACCOUNT_AUTO_CREATION = false;

	/** URL for external authentication, that is used to receive an JSON encoded string, holding the auth status */
	static inline std::string EXTERNAL_AUTH_URL;

	/** Enable\disable brute-force protector from 1 IP on account login */
	static inline bool ENABLE_BRUTEFORCE_PROTECTION = false;

	/** Log successful gameserver logins including connection data to DB */
	static inline bool LOG_LOGINS = false;

	/** @return true if an external authentication URL is configured (Java: !EXTERNAL_AUTH_URL.isBlank()) */
	static bool useExternalAuth();

	/**
	 * Load configs from files: binds this class, CommonsConfig and DatabaseConfig and warns about every property that none of them uses
	 * ("Config property x is unknown and therefore ignored."), except the logging properties that logback.xml reads in Java
	 * (Logging::getPropertyKeys("loginserver")).
	 * <p>
	 * Deviation: Java only excludes the properties referenced in the logback.xml named by the logback.configurationFile system property (set by
	 * the start scripts). The C++ server has no logback.xml, so the keys Logging reads are excluded unconditionally.
	 *
	 * @throws commons::utils::Exception "Can't load loginserver configuration:" with the I/O or parse error as cause (Java: Error)
	 * @throws commons::configuration::TransformationException if a property value is invalid
	 */
	static void load();

	/**
	 * C++ addition: the settings for Logging::init, which the startup code calls before load() like Java's LoginServer calls Logging.init()
	 * before Config.load(). Java's config/logback.xml reads its properties itself, so this mirrors it: config/main/logging.properties, then
	 * config/myls.properties (values of the later file win, values are trimmed, missing files are ignored). Nothing is logged unless a file cannot
	 * be read (a warning, like logback's status listener prints).
	 *
	 * @return the default Logging::Config with statusDiscordWebhookUrl (loginserver.log.status.discord.webhook_url) and statusDiscordAvatarUrl
	 *         (loginserver.log.status.discord.avatar_url) set
	 */
	static commons::logging::Logging::Config loadLoggingConfig();

	/** Binds the fields above to their property keys (Java: the @Property annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);

private:
	static commons::configuration::Properties loadProperties();
};

} // namespace aion::loginserver::configs
