#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/ban/fwd.h"

namespace aion::gameserver::services::ban {

/**
 * C++: a static-only class (hub-headers.md §11.1).
 *
 * @author ViAl, Neon
 */
class ChatBanService {
private:
	static inline runtime::ConcurrentHashMap<int32_t, int64_t> chatBans{AION_LOCK_CLASS(ChatBanService::chatBans#stripe)}; // Java: = new ConcurrentHashMap<>()
public:
	/** Bans a player from all chats. */
	static void banPlayer(model::gameobjects::player::Player& player, int64_t durationMillis);
	static void unbanPlayer(model::gameobjects::player::Player& player);
private:
	static void registerUnban(model::gameobjects::player::Player& player, int64_t delay);
public:
	static bool isBanned(model::gameobjects::player::Player& player);
	/**
	 * Checks time left for the players ban.<br>
	 * If ban is over, this method automatically unbans the player.<br>
	 * If not and unban task is missing (e.g. due to logout), an unban task will be started.
	 */
	static int32_t getBanMinutes(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::services::ban
