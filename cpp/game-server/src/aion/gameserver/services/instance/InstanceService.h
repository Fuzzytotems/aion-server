#pragma once

#include <cstdint>
#include <functional>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/instance/handlers/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/services/instance/fwd.h"
#include "aion/gameserver/world/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::services::instance {

/**
 * C++: a static-only class (hub-headers.md §11.1). The instance handler supplier creates the handler (Ref).
 *
 * @author ATracer
 */
class InstanceService {
private:
	class EmptyInstanceCheckerTask;
public:
	static runtime::Ptr<world::WorldMapInstance> getNextAvailableInstance(int32_t worldId, int32_t ownerId, int8_t difficultyId, const std::function<runtime::Ref<gameserver::instance::handlers::InstanceHandler>(world::WorldMapInstance&)>& instanceHandlerSupplier, int32_t maxPlayers, bool autoDestroy);
	static runtime::Ptr<world::WorldMapInstance> getNextAvailableInstance(int32_t worldId, int32_t ownerId, int8_t difficult, int32_t maxPlayers, bool autoDestroy);
	static runtime::Ptr<world::WorldMapInstance> getNextAvailableInstance(int32_t worldId, model::gameobjects::player::Player& player);
	static runtime::Ptr<world::WorldMapInstance> getNextAvailableInstance(int32_t worldId, int8_t difficult, int32_t maxPlayers);
	/** Instance will be destroyed All players moved to bind location All objects - deleted */
	static void destroyInstance(world::WorldMapInstance& instance);
	static runtime::Ptr<world::WorldMapInstance> getOrRegisterInstance(int32_t worldId, model::gameobjects::player::Player& player);
	static runtime::Ptr<world::WorldMapInstance> getRegisteredInstance(int32_t worldId, int32_t objectId);
	static runtime::Ptr<world::WorldMapInstance> getOrCreateHouseInstance(model::house::House& house);
private:
	static runtime::Ptr<world::WorldMapInstance> getOrCreatePersonalInstance(int32_t worldId, int32_t ownerId);
public:
	static void onPlayerLogin(model::gameobjects::player::Player& player);
	static void moveToExitPoint(model::gameobjects::player::Player& player);
	static bool instanceExists(int32_t worldId, int32_t instanceId);
	static void onLogout(model::gameobjects::player::Player& player);
	static void onEnterInstance(model::gameobjects::player::Player& player);
	static void onLeaveInstance(model::gameobjects::player::Player& player);
	static void onEnterZone(model::gameobjects::player::Player& player, world::zone::ZoneInstance& zone);
	static void onLeaveZone(model::gameobjects::player::Player& player, world::zone::ZoneInstance& zone);
	static int32_t getInstanceRate(model::gameobjects::player::Player& player, int32_t mapId);
	static int32_t getDestroyDelaySeconds(world::WorldMapInstance& worldMapInstance);
};

} // namespace aion::gameserver::services::instance
