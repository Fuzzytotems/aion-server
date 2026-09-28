#pragma once

#include <set>
#include <string>
#include <vector>

#include "aion/commons/configuration/Properties.h"
#include "aion/commons/logging/Logging.h"

namespace aion::chatserver::configs {

/**
 * Loads the chat server configuration: the fields of CommonsConfig, main::LoggingConfig, DatabaseConfig and network::NetworkConfig, bound from
 * ./config/main/*.properties, ./config/network/*.properties and the optional override file ./config/mycs.properties.
 * <p>
 * <b>Threads:</b> the configuration is loaded once at startup, before any other thread exists, and never rebound (there is no reload). After
 * load() the fields are only read, so they are plain fields (see CONVENTIONS.md › Configuration).
 * <p>
 * Java: com.aionemu.chatserver.configs.Config
 *
 * @author ATracer
 */
struct Config {
	/**
	 * Load configs from files: binds the config classes and warns about every property that none of them uses ("Config property x is unknown and
	 * therefore ignored."), except the ones config/logback.xml references (getLogbackPropertyKeys()). If the client connect address is the
	 * wildcard address (the default is the socket address, 0.0.0.0:10241), a local IPv4 address is looked up and used instead ("No connect IP for
	 * Aion client configured, using ...").
	 * <p>
	 * C++ addition: overrides (the -Dkey=value command line arguments) are layered over config/mycs.properties; the keys that were applied are
	 * logged.
	 *
	 * @throws commons::utils::Exception "Can't load chatserver configuration:" with the I/O or parse error as cause (Java: Error), or "No connect
	 *           IP for Aion client configured and local IP discovery failed. ..." (Java: Error)
	 * @throws commons::configuration::TransformationException if a property value is invalid
	 * @throws commons::utils::IOException if the connect address cannot be resolved (Java: its InetAddress is null, a NullPointerException)
	 */
	static void load(const commons::configuration::Properties& overrides = {});

	/**
	 * C++ addition: the settings for Logging::init, which ChatServer::startup calls before load() like Java's ChatServer calls Logging.init()
	 * before Config.load(). The status Discord webhook and avatar URL are read from getLogbackProperties().
	 */
	static commons::logging::Logging::Config loadLoggingConfig();

	/**
	 * C++ addition: the properties Java's config/logback.xml reads itself (&lt;property file="config/main/logging.properties" /&gt;, then
	 * &lt;property file="config/mycs.properties" /&gt;; values of the later file win, missing files are ignored with a warning, values are
	 * trimmed by the caller like logback does).
	 */
	static commons::configuration::Properties loadLogbackProperties();

	/** C++ addition: the server properties config/logback.xml references as ${...} (the Discord settings of the status and the chat log). */
	static std::vector<std::string> getLogbackPropertyKeys();

private:
	static commons::configuration::Properties loadProperties();

	/**
	 * Deviation: Java removes the properties whose ${...} reference occurs in the logback.xml in use. The C++ server has no logback.xml, so the
	 * keys of the chat server's logback.xml (getLogbackPropertyKeys()) are removed.
	 */
	static void removePropertiesUsedInLogbackXml(std::set<std::string>& properties);
};

} // namespace aion::chatserver::configs
