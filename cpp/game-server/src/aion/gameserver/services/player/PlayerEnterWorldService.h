#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/collections/ConcurrentLinkedQueue.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/services/player/fwd.h"

namespace aion::gameserver::services::player {

/**
 * @author ATracer, Neon
 */
class PlayerEnterWorldService final {
private:
	// Java: private static final String VERSION_INFO = "Server " + GameServer.versionInfo.getBuildInfo(GSConfig.TIME_ZONE_ID). Not a C++ static:
	// its initializer needs the loaded config and GameServer's version info, so the port computes it in PlayerEnterWorldService.cpp on first use.
	static inline runtime::ConcurrentLinkedQueue<int32_t> enteringWorld{AION_LOCK_CLASS(PlayerEnterWorldService::enteringWorld)};
public:
	static void enterWorld(network::aion::AionConnection* client, int32_t objectId);

	/**
	 * C++ only (tests, m5a-plan.md S-10 destroy case b): puts an object id into `enteringWorld` / takes it out again, so a test can drive the
	 * duplicate-enter branch (PlayerEnterWorldService.java:159) that falls through without sending a packet.
	 * @return whether the id was added / removed
	 */
	static bool addEnteringWorldForTests(int32_t objectId);
	static bool removeEnteringWorldForTests(int32_t objectId);
private:
	static void enterWorld(network::aion::AionConnection* client, model::gameobjects::player::Player& player);
	static void updateEnergyOfRepose(model::gameobjects::player::Player& player, int64_t secondsOffline);
	static void activatePassiveSkillEffects(model::gameobjects::player::Player& player);
	static bool validateFortressZone(model::gameobjects::player::Player& player);
	/**
	 * Checks if the player is allowed to be in the current vortex zone. He will be sent to the locations home point if not.
	 */
	static void validateVortexZone(model::gameobjects::player::Player& player);
	static void sendItemInfos(network::aion::AionConnection* client, model::gameobjects::player::Player& player);
	static void sendWarehouseItemInfos(network::aion::AionConnection* client, model::gameobjects::player::Player& player);
	static void sendMacroList(network::aion::AionConnection* client, model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::services::player
