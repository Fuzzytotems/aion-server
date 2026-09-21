#include "aion/gameserver/services/HousingService.h"

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/HousingConfig.h"
#include "aion/gameserver/controllers/HouseController.h"
#include "aion/gameserver/dao/HousesDAO.h"
#include "aion/gameserver/dao/PlayerDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/HouseBuildingData.h"
#include "aion/gameserver/dataholders/HouseData.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Letter.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerScripts.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/housing/Building.h"
#include "aion/gameserver/model/templates/housing/BuildingType.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/model/templates/housing/HousingLand.h"
#include "aion/gameserver/model/templates/housing/Sale.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_ACQUIRE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_OWNER_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/services/HousingBidService.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.HousingService");

using model::house::House;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using utils::PacketSendUtility;

namespace {

/** Java DataManager.HOUSE_DATA.getLands() handed to HousesDAO.loadHouses (a Collection<HousingLand>) */
std::vector<const model::templates::housing::HousingLand*> allLands() {
	std::vector<const model::templates::housing::HousingLand*> lands;
	for (const model::templates::housing::HousingLand& land : dataholders::DataManager::HOUSE_DATA->getLands())
		lands.push_back(&land);
	return lands;
}

} // namespace

HousingService::HousingService() {
	customHouses.putAll(dao::HousesDAO::loadHouses(allLands(), false));
	studios.putAll(dao::HousesDAO::loadHouses(allLands(), true));
	updateInactiveStateForAllHouses();
	revokeOwnershipOfDeletedPlayers();
	log.info("Loaded " + std::to_string(customHouses.size()) + " houses and " + std::to_string(studios.size()) + " studios");
}

HousingService::~HousingService() = default;

HousingService& HousingService::getInstance() {
	static HousingService instance; // Java SingletonHolder
	return instance;
}

void HousingService::revokeOwnershipOfDeletedPlayers() {
	std::vector<int32_t> usedIds = dao::PlayerDAO::getUsedIDs();
	std::unordered_set<int32_t> playerIds(usedIds.begin(), usedIds.end());
	std::vector<Ptr<House>> houses = customHouses.values();
	std::vector<Ptr<House>> studioHouses = studios.values();
	houses.insert(houses.end(), studioHouses.begin(), studioHouses.end());
	for (const Ptr<House>& house : houses) {
		// houses table has no player_id foreign key because houses need to stay in DB even on player deletion (to keep bidding possible for example)
		if (house->getOwnerId() > 0 && !playerIds.contains(house->getOwnerId())) {
			log.warn("Player with ID " + std::to_string(house->getOwnerId()) + " got deleted from DB, revoking house ownership for house " +
				std::to_string(house->getAddress()->getId()));
			changeOwner(*house, 0);
		}
	}
}

void HousingService::updateInactiveStateForAllHouses() {
	std::vector<int32_t> ownerIds; // Java: mapToInt(House::getOwnerId).distinct(), in encounter order
	for (const Ptr<House>& house : customHouses.values()) {
		if (std::find(ownerIds.begin(), ownerIds.end(), house->getOwnerId()) == ownerIds.end())
			ownerIds.push_back(house->getOwnerId());
	}
	for (int32_t ownerId : ownerIds)
		updateInactiveStateForPlayerHouses(ownerId);
}

void HousingService::updateInactiveStateForPlayerHouses(int32_t playerObjId) {
	std::vector<Ptr<House>> houses;
	for (const Ptr<House>& house : customHouses.values()) {
		if (house->getOwnerId() == playerObjId)
			houses.push_back(house);
	}
	if (playerObjId == 0) { // not occupied houses
		for (const Ptr<House>& house : houses)
			house->setInactive(false);
	} else {
		// Java: sorted(Comparator.comparing(House::getAcquiredTime)) is a stable sort that throws NullPointerException when it compares a null time
		if (houses.size() > 1 && std::ranges::any_of(houses, [](const Ptr<House>& house) { return !house->getAcquiredTime(); }))
			throw runtime::NullPointerException("Cannot compare the acquired time of the houses of player " + std::to_string(playerObjId) + ": null");
		std::ranges::stable_sort(houses, [](const Ptr<House>& a, const Ptr<House>& b) { return *a->getAcquiredTime() < *b->getAcquiredTime(); });
		for (size_t i = 0; i < houses.size(); i++)
			houses[i]->setInactive(i != 0); // first house (oldest) should be active, rest inactive
	}
}

void HousingService::changeOwner(model::house::House& house, int32_t newOwnerId) {
	int32_t oldOwnerId = house.getOwnerId();
	if (oldOwnerId == newOwnerId)
		return;

	SYNCHRONIZED(house) {
		bool newOwnerHasAnotherHouse = false;
		if (newOwnerId != 0) {
			for (const Ptr<House>& h : customHouses.values()) {
				if (h->getOwnerId() == newOwnerId) {
					newOwnerHasAnotherHouse = true;
					break;
				}
			}
		}
		house.resetRegistry();
		house.getPlayerScripts()->removeAll();
		house.setOwnerId(newOwnerId);
		if (newOwnerId == 0 && removeStudio(house)) {
			// lockdep: Java deletes the studio inside synchronized (house) (HousingService.java:103-106)
			dao::HousesDAO::deleteHouse(oldOwnerId);
			notifyAboutOwnerChange(oldOwnerId, house.getAddress()->getId(), false);
			return;
		}
		house.setInactive(newOwnerHasAnotherHouse);
		house.resetDoorState();
		house.setShowOwnerName(true);
		house.setSignNotice(""); // Java: null; the sign notice column and SM_HOUSE_* write it as an empty string
		house.setAcquiredTime(newOwnerId == 0 ? std::nullopt : std::optional(commons::database::Timestamp(std::chrono::milliseconds(commons::utils::currentTimeMillis()))));
		house.setNextPay(std::nullopt);

		const model::templates::housing::Building* defaultBuilding = house.getLand()->getDefaultBuilding();
		if (defaultBuilding != house.getBuilding())
			switchHouseBuilding(house, defaultBuilding->getId());
		else // in else clause because building switch also saves the house
			house.save();
	}
	Ptr<House> newHouseOfOldOwner = findInactiveHouse(oldOwnerId); // other house of seller that should get activated
	if (newHouseOfOldOwner && newHouseOfOldOwner->getPosition() && newHouseOfOldOwner->isSpawned()) {
		newHouseOfOldOwner->setInactive(false);
		newHouseOfOldOwner->reloadHouseRegistry();
		newHouseOfOldOwner->getController().updateSign();
		newHouseOfOldOwner->getController().updateAppearance();
	}
	notifyAboutOwnerChange(oldOwnerId, house.getAddress()->getId(), false);
	notifyAboutOwnerChange(newOwnerId, house.getAddress()->getId(), true);
	if (house.getPosition() && house.isSpawned()) {
		house.getController().updateHouseSpawns();
		house.getController().kickVisitors(nullptr, true, true);
	}
}

void HousingService::notifyAboutOwnerChange(int32_t ownerId, int32_t addressId, bool isNewOwner) {
	if (ownerId == 0)
		return;
	Ptr<model::gameobjects::player::Player> player = world::World::getInstance().getPlayer(ownerId);
	if (player) {
		player->resetHouses();
		if (!isNewOwner)
			PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_HOUSE_ACQUIRE(player->getObjectId(), addressId, false));
		PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_HOUSE_OWNER_INFO(*player));
		if (isNewOwner)
			PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_HOUSE_ACQUIRE(player->getObjectId(), addressId, true));
	}
}

void HousingService::spawnHouses(world::WorldMapInstance& instance, int32_t registeredId) {
	if (registeredId > 0) {
		spawnStudio(instance.getMapId(), instance.getInstanceId(), registeredId);
		return;
	}
	int32_t spawnedCounter = 0;
	for (const model::templates::housing::HouseAddress* address : dataholders::DataManager::HOUSE_DATA->getAddresses(instance.getMapId())) {
		if (address->getLand()->getDefaultBuilding()->getType() == model::templates::housing::BuildingType::PERSONAL_INS)
			continue; // ignore studios

		Ptr<House> customHouse = customHouses.get(address->getId());
		runtime::Ref<House> newHouse;
		if (!customHouse) {
			newHouse = model::gameobjects::VisibleObject::create<House>(address, instance.getInstanceId());
			customHouse = newHouse;
			// house without owner when acquired will be inserted to DB
			customHouse->setPersistentState(model::gameobjects::Persistable::PersistentState::NEW);
			customHouses.put(address->getId(), newHouse);
		}
		runtime::Ref<world::WorldPosition> position = world::World::getInstance().createPosition(address->getMapId(), address->getX(), address->getY(),
			address->getZ(), int8_t{0}, instance.getInstanceId());
		customHouse->setPosition(position);
		spawnengine::SpawnEngine::bringIntoWorld(*customHouse);
		spawnedCounter++;
	}
	if (spawnedCounter > 0) {
		log.info("Spawned " + std::to_string(spawnedCounter) + " houses in " + instance.toString());
	}
}

void HousingService::spawnStudio(int32_t worldId, int32_t instanceId, int32_t registeredId) {
	Ptr<House> studio = getPlayerStudio(registeredId);
	if (!studio || studio->getAddress()->getMapId() != worldId)
		return;
	if (!studio->getPosition() || studio->getInstanceId() != instanceId) {
		const model::templates::housing::HouseAddress* addr = studio->getAddress();
		studio->setPosition(world::World::getInstance().createPosition(addr->getMapId(), addr->getX(), addr->getY(), addr->getZ(), int8_t{0}, instanceId));
		spawnengine::SpawnEngine::bringIntoWorld(*studio);
	}
}

std::vector<runtime::Ptr<model::house::House>> HousingService::findPlayerHouses(int32_t playerObjId) {
	if (studios.containsKey(playerObjId)) {
		return {studios.get(playerObjId)};
	}
	std::vector<Ptr<House>> houses;
	for (const Ptr<House>& house : customHouses.values()) {
		if (house->getOwnerId() == playerObjId)
			houses.push_back(house);
	}
	return houses;
}

runtime::Ptr<model::house::House> HousingService::findActiveHouse(int32_t playerObjId) {
	if (studios.containsKey(playerObjId))
		return studios.get(playerObjId);
	for (const Ptr<House>& house : customHouses.values()) {
		if (house->getOwnerId() == playerObjId && !house->isInactive())
			return house;
	}
	return nullptr;
}

runtime::Ptr<model::house::House> HousingService::findInactiveHouse(int32_t playerObjId) {
	for (const Ptr<House>& house : customHouses.values()) {
		if (house->getOwnerId() == playerObjId && house->isInactive())
			return house;
	}
	return nullptr;
}

runtime::Ptr<model::house::House> HousingService::getHouseByAddress(int32_t address) {
	return customHouses.get(address);
}

runtime::Ptr<model::house::House> HousingService::getPlayerStudio(int32_t playerId) {
	return studios.get(playerId);
}

bool HousingService::removeStudio(model::house::House& studio) {
	return studios.values().remove(Ptr<House>(studio));
}

void HousingService::registerPlayerStudio(model::gameobjects::player::Player& player) {
	createStudio(player, false);
}

void HousingService::recreatePlayerStudio(model::gameobjects::player::Player& player) {
	createStudio(player, true);
}

void HousingService::createStudio(model::gameobjects::player::Player& player, bool chargeFee) {
	if (!player.getHouses()->isEmpty()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_INS_CANT_OWN_MORE_HOUSE());
		return;
	}
	const model::templates::housing::HouseAddress* address = dataholders::DataManager::HOUSE_DATA->getStudioAddress(player.getRace());
	if (chargeFee && !player.getInventory().tryDecreaseKinah(address->getLand()->getSaleOptions()->getGoldPrice())) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_NOT_ENOUGH_MONEY());
		return;
	}

	runtime::Ref<House> studio = model::gameobjects::VisibleObject::create<House>(address, 0);
	studios.put(player.getObjectId(), studio);
	studio->setPersistentState(model::gameobjects::Persistable::PersistentState::NEW);
	changeOwner(*studio, player.getObjectId());

	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_INS_OWN_SUCCESS());
}

bool HousingService::canOwnHouse(model::gameobjects::player::Player& player, bool notify) {
	int32_t questId = player.getRace() == model::Race::ELYOS ? 18802 : 28802;
	Ptr<questEngine::model::QuestState> qs = player.getQuestStateList()->getQuestState(questId);
	if (!qs || qs->getStatus() != questEngine::model::QuestStatus::COMPLETE) {
		if (notify)
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_OWN_NOT_COMPLETE_QUEST(questId));
		return false;
	}
	return true;
}

void HousingService::switchHouseBuilding(model::house::House& currentHouse, int32_t newBuildingId) {
	currentHouse.setBuilding(dataholders::DataManager::HOUSE_BUILDING_DATA->getBuilding(newBuildingId));
	currentHouse.save();
	currentHouse.reloadHouseRegistry(); // load new defaults
	currentHouse.getController().spawnObjects();
}

std::vector<runtime::Ptr<model::house::House>> HousingService::getCustomHouses() {
	return customHouses.values();
}

runtime::Ptr<model::house::House> HousingService::findHouse(int32_t objId) {
	for (const Ptr<House>& house : customHouses.values()) {
		if (house->getObjectId() == objId)
			return house;
	}
	return nullptr;
}

runtime::Ptr<model::house::House> HousingService::findStudio(int32_t objId) {
	for (const Ptr<House>& studio : studios.values()) {
		if (studio->getObjectId() == objId)
			return studio;
	}
	return nullptr;
}

runtime::Ptr<model::house::House> HousingService::findHouseOrStudio(int32_t objId) {
	Ptr<House> studio = findStudio(objId);
	return !studio ? findHouse(objId) : studio;
}

void HousingService::onPlayerDeleted(int32_t playerObjId) {
	HousingBidService::getInstance().disableBids(playerObjId);
	for (const Ptr<House>& house : findPlayerHouses(playerObjId))
		changeOwner(*house, 0);
}

void HousingService::onPlayerLogin(model::gameobjects::player::Player& player) {
	Ptr<House> activeHouse = player.getActiveHouse();
	if (activeHouse) {
		if (configs::main::HousingConfig::ENABLE_HOUSE_PAY.load() && activeHouse->getNextPay() &&
			activeHouse->getNextPay()->time_since_epoch().count() <= commons::utils::currentTimeMillis())
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_OVERDUE());
	} else {
		for (const Ptr<model::gameobjects::Letter>& letter : player.getMailbox()->getNewSystemLetters("$$HS_OVERDUE_")) {
			std::string senderName = letter->getSenderName();
			if (senderName.ends_with("FINAL") || senderName.ends_with("3RD")) {
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_SEQUESTRATE());
				break;
			}
		}
	}
	PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_HOUSE_OWNER_INFO(player));
}

} // namespace aion::gameserver::services
