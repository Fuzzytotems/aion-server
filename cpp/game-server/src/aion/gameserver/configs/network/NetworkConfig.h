#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include "aion/commons/utils/InetSocketAddress.h"
#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::network {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.network.NetworkConfig
 */
struct NetworkConfig {
	/** Address where Aion clients will attempt to connect to (host/domain name or IP) */
	static inline ConfigValue<commons::utils::InetSocketAddress> CLIENT_CONNECT_ADDRESS;

	/** Local address where GS will listen for Aion client connections (0.0.0.0 = bind any local IP) */
	static inline ConfigValue<commons::utils::InetSocketAddress> CLIENT_SOCKET_ADDRESS;

	/** Address (host/domain name or IP) of the login server */
	static inline ConfigValue<commons::utils::InetSocketAddress> LOGIN_ADDRESS;

	/** Address (host/domain name or IP) of the chat server */
	static inline ConfigValue<commons::utils::InetSocketAddress> CHAT_ADDRESS;

	/** Password for this GameServer ID for authentication at ChatServer. */
	static inline ConfigValue<std::string> CHAT_PASSWORD;

	/** GameServer id that this GameServer will request at LoginServer. */
	static inline std::atomic<int32_t> GAMESERVER_ID{0};

	/** Password for this GameServer ID for authentication at LoginServer. */
	static inline ConfigValue<std::string> LOGIN_PASSWORD;

	/** Minimum required access level for accounts trying to connect (=maintenance mode) */
	static inline std::atomic<int32_t> MIN_ACCESS_LEVEL{0};

	/** Max allowed online players */
	static inline std::atomic<int32_t> MAX_ONLINE_PLAYERS{0};

	/**
	 * Number of Threads dedicated to be doing io read & write. There is always 1 acceptor thread. If value is < 1 - acceptor thread will also handle
	 * read & write. If value is > 0 - there will be given amount of read & write threads + 1 acceptor thread.
	 */
	static inline std::atomic<int32_t> NIO_READ_WRITE_THREADS{0};

	/** Allows values > 1 for nio.threads. Only for development/debugging - the game server is not thread-safe! */
	static inline std::atomic<bool> NIO_READ_WRITE_THREADS_UNSAFE_ALLOW{false};

	/** Number of minimum threads that will be used to execute aion client packets. */
	static inline std::atomic<int32_t> PACKET_PROCESSOR_MIN_THREADS{0};

	/** Number of maximum threads that will be used to execute aion client packets. */
	static inline std::atomic<int32_t> PACKET_PROCESSOR_MAX_THREADS{0};

	/** Threshold that will be used to decide when extra threads are not needed. (it doesn't have any effect if min threads == max threads) */
	static inline std::atomic<int32_t> PACKET_PROCESSOR_THREAD_KILL_THRESHOLD{0};

	/** Threshold that will be used to decide when extra threads should be spawned. (it doesn't have any effect if min threads == max threads) */
	static inline std::atomic<int32_t> PACKET_PROCESSOR_THREAD_SPAWN_THRESHOLD{0};

	/** If aion client packets unknown by the server should be logged. */
	static inline std::atomic<bool> LOG_UNKNOWN_PACKETS{false};

	/** If ignored aion client packets (due to invalid connection state) should be logged. */
	static inline std::atomic<bool> LOG_IGNORED_PACKETS{false};

	static inline std::atomic<bool> ENABLE_FLOOD_CONNECTIONS{false};

	static inline std::atomic<int32_t> Flood_Tick{0};

	static inline std::atomic<int32_t> Flood_SWARN{0};

	static inline std::atomic<int32_t> Flood_SReject{0};

	static inline std::atomic<int32_t> Flood_STick{0};

	static inline std::atomic<int32_t> Flood_LWARN{0};

	static inline std::atomic<int32_t> Flood_LReject{0};

	static inline std::atomic<int32_t> Flood_LTick{0};

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::network
