#include "aion/gameserver/services/player/PlayerEnterWorldService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services::player {

static const auto log = commons::logging::LoggerFactory::getLogger("GAMECONNECTION_LOG");

/** Java: package-private top-level class GeneralUpdateTask implements Runnable (PlayerEnterWorldService.java; used only by enterWorld) */
class GeneralUpdateTask {
private:
	const int32_t playerId;
public:
	explicit GeneralUpdateTask(int32_t playerId);
	void run();
};

/** Java: package-private top-level class ItemUpdateTask implements Runnable (PlayerEnterWorldService.java; used only by enterWorld) */
class ItemUpdateTask {
private:
	const int32_t playerId;
public:
	explicit ItemUpdateTask(int32_t playerId);
	void run();
};

// Java: GeneralUpdateTask.log and ItemUpdateTask.log
static const auto generalUpdateTaskLog = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.player.GeneralUpdateTask");
static const auto itemUpdateTaskLog = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.player.ItemUpdateTask");

void PlayerEnterWorldService::enterWorld(network::aion::AionConnection* client, int32_t objectId) {
	AION_UNPORTED();
}

void PlayerEnterWorldService::enterWorld(network::aion::AionConnection* client, model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerEnterWorldService::updateEnergyOfRepose(model::gameobjects::player::Player& player, int64_t secondsOffline) {
	AION_UNPORTED();
}

void PlayerEnterWorldService::activatePassiveSkillEffects(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool PlayerEnterWorldService::validateFortressZone(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerEnterWorldService::validateVortexZone(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerEnterWorldService::sendItemInfos(network::aion::AionConnection* client, model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerEnterWorldService::sendWarehouseItemInfos(network::aion::AionConnection* client, model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerEnterWorldService::sendMacroList(network::aion::AionConnection* client, model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

GeneralUpdateTask::GeneralUpdateTask(int32_t value) : playerId(value) {
}

void GeneralUpdateTask::run() {
	AION_UNPORTED();
}

ItemUpdateTask::ItemUpdateTask(int32_t value) : playerId(value) {
}

void ItemUpdateTask::run() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::player
