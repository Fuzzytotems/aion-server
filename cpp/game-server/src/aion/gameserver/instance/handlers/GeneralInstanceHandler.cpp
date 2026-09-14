#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::instance::handlers {

const commons::logging::Logger GeneralInstanceHandler::log = commons::logging::LoggerFactory::getLogger("INSTANCE_LOG");

GeneralInstanceHandler::GeneralInstanceHandler(world::WorldMapInstance& instanceValue)
	: instance(instanceValue), mapId(instanceValue.getMapId()) {
}

GeneralInstanceHandler::~GeneralInstanceHandler() = default;

runtime::Ref<GeneralInstanceHandler> GeneralInstanceHandler::create(world::WorldMapInstance& instanceValue) {
	return runtime::makeRef<GeneralInstanceHandler>(instanceValue);
}

void GeneralInstanceHandler::onLeaveInstance(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::VisibleObject> GeneralInstanceHandler::spawn(int32_t npcId, float x, float y, float z, int8_t heading) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::VisibleObject> GeneralInstanceHandler::spawn(int32_t npcId, float x, float y, float z, int8_t heading,
	int32_t staticId) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::VisibleObject> GeneralInstanceHandler::spawnAndSetRespawn(int32_t npcId, float x, float y, float z, int8_t heading,
	int32_t respawnTime) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::Npc> GeneralInstanceHandler::getNpc(int32_t npcId) {
	AION_UNPORTED();
}

void GeneralInstanceHandler::deleteAliveNpcs(std::initializer_list<int32_t> npcIds) {
	AION_UNPORTED();
}

void GeneralInstanceHandler::sendMsg(network::aion::serverpackets::SM_SYSTEM_MESSAGE& msg) {
	AION_UNPORTED();
}

void GeneralInstanceHandler::sendMsg(network::aion::serverpackets::SM_SYSTEM_MESSAGE& msg, int32_t delay) {
	AION_UNPORTED();
}

void GeneralInstanceHandler::onDespawn(model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

void GeneralInstanceHandler::onDie(model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

void GeneralInstanceHandler::logNpcWithReason(model::gameobjects::Npc& npc, std::string_view reason) {
	AION_UNPORTED();
}

bool GeneralInstanceHandler::isBoss(model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

void GeneralInstanceHandler::portToStartPosition(model::gameobjects::player::Player& player) {
	// Java: throw new UnsupportedOperationException();
	AION_UNPORTED();
}

float GeneralInstanceHandler::getExpMultiplier() {
	AION_UNPORTED();
}

bool GeneralInstanceHandler::allowKiskRevive() {
	AION_UNPORTED();
}

bool GeneralInstanceHandler::allowInstanceRevive() {
	AION_UNPORTED();
}

bool GeneralInstanceHandler::isRestrictedToInstance(model::gameobjects::Item& item) {
	AION_UNPORTED();
}

void GeneralInstanceHandler::removeInstanceItems(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::instance::handlers
