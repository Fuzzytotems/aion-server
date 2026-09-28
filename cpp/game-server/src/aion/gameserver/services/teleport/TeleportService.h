#pragma once

#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/animations/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/teleport/fwd.h"
#include "aion/gameserver/services/teleport/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::services::teleport {

/**
 * @author xTz, Neon
 */
class TeleportService {
private:
	/** Java: static class SpawnTask implements Runnable (used only by the bodies, defined in TeleportService.cpp) */
	class SpawnTask;
	static inline runtime::Field<runtime::Ref<runtime::Array<double>>> eventPosAsmodians{};
	static inline runtime::Field<runtime::Ref<runtime::Array<double>>> eventPosElyos{};
public:
	static void teleportToFirstTeleportLocation(model::gameobjects::player::Player& player, model::gameobjects::Npc& teleporter,
		model::animations::TeleportAnimation animation);
	static void teleport(model::gameobjects::player::Player& player, const model::templates::teleport::TeleportLocation* location,
		model::animations::TeleportAnimation animation);
	static const model::templates::teleport::TeleporterTemplate* validateTeleporterAndGetTemplate(model::gameobjects::player::Player& player,
		model::gameobjects::Npc& teleporter);
private:
	static bool checkKinahForTransportation(const model::templates::teleport::TeleportLocation* location, model::gameobjects::player::Player& player);
	static void sendLoc(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y, float z, int8_t h,
		model::animations::TeleportAnimation animation);
	static void abortPlayerActions(model::gameobjects::player::Player& player);
	static void spawnOnSameMap(model::gameobjects::player::Player& player);
public:
	static void teleportTo(model::gameobjects::player::Player& player, world::WorldPosition& pos);
	static void teleportDeadTo(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y, float z,
		int8_t heading);
	static void teleportTo(model::gameobjects::player::Player& player, int32_t worldId, float x, float y, float z);
	static void teleportTo(model::gameobjects::player::Player& player, int32_t worldId, float x, float y, float z, int8_t h);
	static void teleportTo(model::gameobjects::player::Player& player, int32_t worldId, float x, float y, float z, int8_t h,
		model::animations::TeleportAnimation animation);
	static void teleportTo(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y, float z);
	static void teleportTo(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y, float z, int8_t h);
	static void teleportTo(model::gameobjects::player::Player& player, world::WorldMapInstance& instance, float x, float y, float z);
	static void teleportTo(model::gameobjects::player::Player& player, world::WorldMapInstance& instance, float x, float y, float z, int8_t h);
	static void teleportTo(model::gameobjects::player::Player& player, world::WorldMapInstance& instance, float x, float y, float z, int8_t h,
		model::animations::TeleportAnimation animation);
	static void teleportTo(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y, float z, int8_t heading,
		model::animations::TeleportAnimation animation);
	static void showMap(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc);
	static void teleportToPrison(model::gameobjects::player::Player& player);
	static void teleportToNpc(model::gameobjects::player::Player& player, int32_t npcId);
	/** This method will send the set bind point packet */
	static void sendObeliskBindPoint(model::gameobjects::player::Player& player);
	static void sendKiskBindPoint(model::gameobjects::player::Player& player);
	static void moveToBindLocation(model::gameobjects::player::Player& player);
	static void moveToTargetWithDistance(model::gameobjects::VisibleObject& object, model::gameobjects::player::Player& player, int32_t direction,
		int32_t distance);
	static void moveToInstanceExit(model::gameobjects::player::Player& player, int32_t worldId, model::Race race);
	static void useTeleportScroll(model::gameobjects::player::Player& player, std::string_view portalName, int32_t worldId);
	static void changeChannel(model::gameobjects::player::Player& player, int32_t channel);
	static void setEventPos(world::WorldPosition& pos, model::Race race);
	static void teleportToEvent(model::gameobjects::player::Player& player);
	/** Sends a teleport request to the player. He will only be teleported to the Npc if he accepts the request. */
	static bool sendTeleportRequest(model::gameobjects::player::Player& player, int32_t npcId);
};

} // namespace aion::gameserver::services::teleport
