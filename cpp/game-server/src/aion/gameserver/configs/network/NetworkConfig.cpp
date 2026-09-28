#include "aion/gameserver/configs/network/NetworkConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::network {

void NetworkConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.network.client.connect_address", CLIENT_CONNECT_ADDRESS, "0.0.0.0:7777");
	AION_BIND(p, "gameserver.network.client.socket_address", CLIENT_SOCKET_ADDRESS, "0.0.0.0:7777");
	AION_BIND(p, "gameserver.network.login.address", LOGIN_ADDRESS, "localhost:9014");
	AION_BIND(p, "gameserver.network.chat.address", CHAT_ADDRESS, "localhost:9021");
	AION_BIND(p, "gameserver.network.chat.password", CHAT_PASSWORD, "");
	AION_BIND(p, "gameserver.network.login.gsid", GAMESERVER_ID, "1");
	AION_BIND(p, "gameserver.network.login.password", LOGIN_PASSWORD, "");
	AION_BIND(p, "gameserver.network.login.min_accesslevel", MIN_ACCESS_LEVEL, "0");
	AION_BIND(p, "gameserver.network.login.max_players", MAX_ONLINE_PLAYERS, "100");
	AION_BIND(p, "gameserver.network.nio.threads", NIO_READ_WRITE_THREADS, "1");
	AION_BIND(p, "gameserver.network.nio.threads.unsafe.allow", NIO_READ_WRITE_THREADS_UNSAFE_ALLOW, "false");
	AION_BIND(p, "gameserver.network.packet.processor.threads.min", PACKET_PROCESSOR_MIN_THREADS, "4");
	AION_BIND(p, "gameserver.network.packet.processor.threads.max", PACKET_PROCESSOR_MAX_THREADS, "4");
	AION_BIND(p, "gameserver.network.packet.processor.threshold.kill", PACKET_PROCESSOR_THREAD_KILL_THRESHOLD, "3");
	AION_BIND(p, "gameserver.network.packet.processor.threshold.spawn", PACKET_PROCESSOR_THREAD_SPAWN_THRESHOLD, "50");
	AION_BIND(p, "gameserver.network.logging.unknown_packets", LOG_UNKNOWN_PACKETS, "false");
	AION_BIND(p, "gameserver.network.logging.ignored_packets", LOG_IGNORED_PACKETS, "false");
	AION_BIND(p, "gameserver.network.flood.connections", ENABLE_FLOOD_CONNECTIONS, "false");
	AION_BIND(p, "gameserver.network.flood.tick", Flood_Tick, "1000");
	AION_BIND(p, "gameserver.network.flood.short.warn", Flood_SWARN, "10");
	AION_BIND(p, "gameserver.network.flood.short.reject", Flood_SReject, "20");
	AION_BIND(p, "gameserver.network.flood.short.tick", Flood_STick, "10");
	AION_BIND(p, "gameserver.network.flood.long.warn", Flood_LWARN, "30");
	AION_BIND(p, "gameserver.network.flood.long.reject", Flood_LReject, "60");
	AION_BIND(p, "gameserver.network.flood.long.tick", Flood_LTick, "60");
}

} // namespace aion::gameserver::configs::network
