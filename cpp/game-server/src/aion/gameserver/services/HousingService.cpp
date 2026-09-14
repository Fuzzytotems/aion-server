#include "aion/gameserver/services/HousingService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/house/House.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.HousingService");

HousingService::HousingService() {
	AION_UNPORTED();
}

HousingService::~HousingService() = default;

HousingService& HousingService::getInstance() {
	static HousingService instance; // Java SingletonHolder
	return instance;
}

void HousingService::revokeOwnershipOfDeletedPlayers() {
	AION_UNPORTED();
}

void HousingService::updateInactiveStateForAllHouses() {
	AION_UNPORTED();
}

void HousingService::updateInactiveStateForPlayerHouses(int32_t playerObjId) {
	AION_UNPORTED();
}

void HousingService::changeOwner(model::house::House& house, int32_t newOwnerId) {
	AION_UNPORTED();
}

void HousingService::notifyAboutOwnerChange(int32_t ownerId, int32_t addressId, bool isNewOwner) {
	AION_UNPORTED();
}

void HousingService::spawnHouses(world::WorldMapInstance& instance, int32_t registeredId) {
	AION_UNPORTED();
}

void HousingService::spawnStudio(int32_t worldId, int32_t instanceId, int32_t registeredId) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::house::House>> HousingService::findPlayerHouses(int32_t playerObjId) {
	AION_UNPORTED();
}

runtime::Ptr<model::house::House> HousingService::findActiveHouse(int32_t playerObjId) {
	AION_UNPORTED();
}

runtime::Ptr<model::house::House> HousingService::findInactiveHouse(int32_t playerObjId) {
	AION_UNPORTED();
}

runtime::Ptr<model::house::House> HousingService::getHouseByAddress(int32_t address) {
	AION_UNPORTED();
}

runtime::Ptr<model::house::House> HousingService::getPlayerStudio(int32_t playerId) {
	AION_UNPORTED();
}

bool HousingService::removeStudio(model::house::House& studio) {
	AION_UNPORTED();
}

void HousingService::registerPlayerStudio(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void HousingService::recreatePlayerStudio(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void HousingService::createStudio(model::gameobjects::player::Player& player, bool chargeFee) {
	AION_UNPORTED();
}

bool HousingService::canOwnHouse(model::gameobjects::player::Player& player, bool notify) {
	AION_UNPORTED();
}

void HousingService::switchHouseBuilding(model::house::House& currentHouse, int32_t newBuildingId) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::house::House>> HousingService::getCustomHouses() {
	AION_UNPORTED();
}

runtime::Ptr<model::house::House> HousingService::findHouse(int32_t objId) {
	AION_UNPORTED();
}

runtime::Ptr<model::house::House> HousingService::findStudio(int32_t objId) {
	AION_UNPORTED();
}

runtime::Ptr<model::house::House> HousingService::findHouseOrStudio(int32_t objId) {
	AION_UNPORTED();
}

void HousingService::onPlayerDeleted(int32_t playerObjId) {
	AION_UNPORTED();
}

void HousingService::onPlayerLogin(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
