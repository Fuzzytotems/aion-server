#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string_view>

#include "aion/chatserver/network/gameserver/GsAuthResponse.h"

namespace aion::chatserver::service {

/**
 * Registration of the game server: only one game server can be online at a time. Thread safe.
 * <p>
 * Like in Java, setOffline() is called when any game server connection closes, also a connection that was rejected as ALREADY_REGISTERED,
 * so a second game server that tries to connect makes the registered one appear offline.
 * <p>
 * Java: com.aionemu.chatserver.service.GameServerService
 *
 * @author ATracer, KID, Neon
 */
class GameServerService {
public:
	/** the id of the game server registered last (Java: public static byte) */
	static inline std::atomic<int8_t> GAMESERVER_ID = 0;

	static GameServerService& getInstance();

	/**
	 * @return ALREADY_REGISTERED if a game server is online, NOT_AUTHED if the password does not match NetworkConfig::GAMESERVER_PASSWORD, else
	 *         AUTHED (the game server is online from now on)
	 */
	network::gameserver::GsAuthResponse registerGameServer(int8_t gameServerId, std::string_view password);

	/** Logs "Gameserver #&lt;id&gt; is disconnected" and marks the game server offline. */
	void setOffline();

private:
	GameServerService() = default;

	/** guards isOnline (Java: an unsynchronized check-then-act, which two game servers authenticating at once could both pass) */
	std::mutex mutex;
	bool isOnline = false;
};

} // namespace aion::chatserver::service
