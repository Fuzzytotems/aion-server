#include "aion/gameserver/services/teleport/TeleportService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::services::teleport {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
// anonymous RequestResponseHandler at TeleportService.java:467 (com.aionemu.gameserver.services.teleport.TeleportService$1); local handler; storage:
// stored in ResponseRequester

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.teleport.TeleportService");

// Java implements Runnable
class TeleportService::SpawnTask {
public:
	const runtime::Ref<model::gameobjects::player::Player> player;
	const int32_t worldId;
	const int32_t instanceId;
	const float x;
	const float y;
	const float z;
	const int8_t h;
	const model::animations::TeleportAnimation animation;

	SpawnTask(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y, float z, int8_t h,
		model::animations::TeleportAnimation animation);
	void run();
};

TeleportService::SpawnTask::SpawnTask(model::gameobjects::player::Player& value, int32_t worldIdValue, int32_t instanceIdValue, float xValue,
	float yValue, float zValue, int8_t hValue, model::animations::TeleportAnimation animationValue)
	: player(value), worldId(worldIdValue), instanceId(instanceIdValue), x(xValue), y(yValue), z(zValue), h(hValue), animation(animationValue) {
}

void TeleportService::SpawnTask::run() {
	AION_UNPORTED();
}

void TeleportService::teleportToFirstTeleportLocation(model::gameobjects::player::Player& player, model::gameobjects::Npc& teleporter,
	model::animations::TeleportAnimation animation) {
	AION_UNPORTED();
}

void TeleportService::teleport(model::gameobjects::player::Player& player, const model::templates::teleport::TeleportLocation* location,
	model::animations::TeleportAnimation animation) {
	AION_UNPORTED();
}

const model::templates::teleport::TeleporterTemplate* TeleportService::validateTeleporterAndGetTemplate(model::gameobjects::player::Player& player,
	model::gameobjects::Npc& teleporter) {
	AION_UNPORTED();
}

bool TeleportService::checkKinahForTransportation(const model::templates::teleport::TeleportLocation* location,
	model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void TeleportService::sendLoc(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y, float z, int8_t h,
	model::animations::TeleportAnimation animation) {
	AION_UNPORTED();
}

void TeleportService::abortPlayerActions(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void TeleportService::spawnOnSameMap(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void TeleportService::teleportTo(model::gameobjects::player::Player& player, world::WorldPosition& pos) {
	AION_UNPORTED();
}

void TeleportService::teleportDeadTo(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y, float z,
	int8_t heading) {
	AION_UNPORTED();
}

void TeleportService::teleportTo(model::gameobjects::player::Player& player, int32_t worldId, float x, float y, float z) {
	AION_UNPORTED();
}

void TeleportService::teleportTo(model::gameobjects::player::Player& player, int32_t worldId, float x, float y, float z, int8_t h) {
	AION_UNPORTED();
}

void TeleportService::teleportTo(model::gameobjects::player::Player& player, int32_t worldId, float x, float y, float z, int8_t h,
	model::animations::TeleportAnimation animation) {
	AION_UNPORTED();
}

void TeleportService::teleportTo(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y, float z) {
	AION_UNPORTED();
}

void TeleportService::teleportTo(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y, float z,
	int8_t h) {
	AION_UNPORTED();
}

void TeleportService::teleportTo(model::gameobjects::player::Player& player, world::WorldMapInstance& instance, float x, float y, float z) {
	AION_UNPORTED();
}

void TeleportService::teleportTo(model::gameobjects::player::Player& player, world::WorldMapInstance& instance, float x, float y, float z, int8_t h) {
	AION_UNPORTED();
}

void TeleportService::teleportTo(model::gameobjects::player::Player& player, world::WorldMapInstance& instance, float x, float y, float z, int8_t h,
	model::animations::TeleportAnimation animation) {
	AION_UNPORTED();
}

void TeleportService::teleportTo(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y, float z,
	int8_t heading, model::animations::TeleportAnimation animation) {
	AION_UNPORTED();
}

void TeleportService::showMap(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

void TeleportService::teleportToPrison(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void TeleportService::teleportToNpc(model::gameobjects::player::Player& player, int32_t npcId) {
	AION_UNPORTED();
}

void TeleportService::sendObeliskBindPoint(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void TeleportService::sendKiskBindPoint(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void TeleportService::moveToBindLocation(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void TeleportService::moveToTargetWithDistance(model::gameobjects::VisibleObject& object, model::gameobjects::player::Player& player,
	int32_t direction, int32_t distance) {
	AION_UNPORTED();
}

void TeleportService::moveToInstanceExit(model::gameobjects::player::Player& player, int32_t worldId, model::Race race) {
	AION_UNPORTED();
}

void TeleportService::useTeleportScroll(model::gameobjects::player::Player& player, std::string_view portalName, int32_t worldId) {
	AION_UNPORTED();
}

void TeleportService::changeChannel(model::gameobjects::player::Player& player, int32_t channel) {
	AION_UNPORTED();
}

void TeleportService::setEventPos(world::WorldPosition& pos, model::Race race) {
	AION_UNPORTED();
}

void TeleportService::teleportToEvent(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool TeleportService::sendTeleportRequest(model::gameobjects::player::Player& player, int32_t npcId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::teleport
