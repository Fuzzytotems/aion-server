#include "aion/gameserver/model/house/House.h"

#include <algorithm>
#include <chrono>
#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/configs/main/HousingConfig.h"
#include "aion/gameserver/controllers/HouseController.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dao/HouseScriptsDAO.h"
#include "aion/gameserver/dao/HousesDAO.h"
#include "aion/gameserver/dao/PlayerRegisteredItemsDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/TribeClass.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Friend.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/HouseOwnerStateInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerScripts.h"
#include "aion/gameserver/model/house/HouseBids.h"
#include "aion/gameserver/model/house/HouseDoorStateInfo.h"
#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/templates/housing/Building.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/model/templates/housing/HouseType.h"
#include "aion/gameserver/model/templates/housing/HousingLand.h"
#include "aion/gameserver/model/templates/housing/Sale.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnType.h"
#include "aion/gameserver/model/town/Town.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/services/TownService.h"
#include "aion/gameserver/taskmanager/tasks/housing/AuctionEndTask.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/knownlist/PlayerAwareKnownList.h"

namespace aion::gameserver::model::house {

namespace {

/** Java address.getLand().getDefaultBuilding(): NullPointerException for an address without land */
const templates::housing::Building* defaultBuildingOf(const templates::housing::HouseAddress* address) {
	if (address == nullptr || address->getLand() == nullptr)
		throw runtime::NullPointerException("Cannot invoke \"HousingLand.getDefaultBuilding()\" because the house address or its land is null");
	return address->getLand()->getDefaultBuilding();
}

} // namespace

House::House(CreateKey key, const templates::housing::HouseAddress* addressValue, int32_t instanceId)
	: House(key, utils::idfactory::IDFactory::getInstance().nextId(), defaultBuildingOf(addressValue), addressValue, instanceId) {
}

House::House(CreateKey key, int32_t objectId, const templates::housing::Building* buildingValue, const templates::housing::HouseAddress* addressValue,
	int32_t instanceId)
	: VisibleObject(key, objectId, std::make_unique<controllers::HouseController>(), nullptr, nullptr, nullptr, false), address(addressValue),
	  building(buildingValue) {
	static_cast<void>(instanceId); // unused in Java too
}

House::~House() = default;

void House::postConstruct() {
	VisibleObject::postConstruct();
	getController().setOwner(*this);
	setKnownlist(std::make_unique<world::knownlist::PlayerAwareKnownList>(*this));
	resetDoorState();
	setPersistentState(PersistentState::UPDATED);
}

controllers::HouseController& House::getController() const {
	return static_cast<controllers::HouseController&>(VisibleObject::getController());
}

std::string House::getName() {
	return "HOUSE_" + std::to_string(address->getId());
}

const templates::housing::HousingLand* House::getLand() {
	return address->getLand();
}

world::WorldType House::getWorldType() {
	return world::World::getInstance().getWorldMap(getAddress()->getMapId())->getWorldType();
}

void House::setBuilding(const templates::housing::Building* buildingValue) {
	this->building.set(buildingValue);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

float House::getVisibleDistance() {
	return configs::main::HousingConfig::VISIBILITY_DISTANCE.load();
}

void House::setOwnerId(int32_t ownerIdValue) {
	this->ownerId.set(ownerIdValue);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

std::optional<std::string> House::getOwnerName() {
	if (ownerId.get() == 0)
		return std::nullopt;
	runtime::Ptr<gameobjects::Npc> butler = getButler();
	return !butler ? services::player::PlayerService::getPlayerName(ownerId.get()) : butler->getMasterName();
}

void House::setAcquiredTime(std::optional<commons::database::Timestamp> acquiredTimeValue) {
	this->acquiredTime.set(acquiredTimeValue);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

int32_t House::getPermissionsForDB() {
	int32_t permissions = showOwnerName.get() ? 1 : 0;
	permissions |= getId(doorState.get()) << 8;
	return permissions;
}

void House::setPermissionsFromDB(int32_t permissions) {
	showOwnerName.set((permissions & 0xFF) == 1);
	std::optional<HouseDoorState> state = houseDoorStateOf(static_cast<int8_t>(permissions >> 8));
	if (state) {
		doorState.set(*state);
	} else {
		// Java stores null, so resetDoorState() always changes the state (and requests an update): CLOSED_EXCEPT_FRIENDS stands in for null,
		// since resetDoorState() only ever picks OPEN or CLOSED
		doorState.set(HouseDoorState::CLOSED_EXCEPT_FRIENDS);
		resetDoorState();
	}
}

bool House::resetDoorState() {
	return setDoorState(inactive.get() || (ownerId.get() == 0 && !bids.get()) ? HouseDoorState::CLOSED : HouseDoorState::OPEN);
}

bool House::setDoorState(HouseDoorState doorStateValue) {
	if (this->doorState.get() == doorStateValue)
		return false;
	this->doorState.set(doorStateValue);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
	if (getPosition() && isSpawned())
		world::geo::GeoService::getInstance().setHouseDoorState(address->getMapId(), getInstanceId(), address->getId(), getDoorState());
	return true;
}

void House::setShowOwnerName(bool showOwnerNameValue) {
	this->showOwnerName.set(showOwnerNameValue);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

bool House::isFeePaid() {
	std::optional<commons::database::Timestamp> pay = nextPay.get();
	return !pay || pay->time_since_epoch().count() >= commons::utils::currentTimeMillis();
}

void House::setNextPay(std::optional<commons::database::Timestamp> nextPayValue) {
	this->nextPay.set(nextPayValue);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

void House::setBids(runtime::Ptr<HouseBids> bidsValue, bool resetDoorStateOfUnoccupiedHouse) {
	this->bids.set(bidsValue);
	if (resetDoorStateOfUnoccupiedHouse && ownerId.get() == 0 && resetDoorState())
		save();
}

runtime::Ptr<gameobjects::Npc> House::getButler() {
	return getSpawn(templates::spawns::SpawnType::MANAGER);
}

runtime::Ptr<gameobjects::Npc> House::getRelationshipCrystal() {
	return getSpawn(templates::spawns::SpawnType::TELEPORT);
}

runtime::Ptr<gameobjects::Npc> House::getCurrentSign() {
	return getSpawn(templates::spawns::SpawnType::SIGN);
}

runtime::Ptr<gameobjects::Npc> House::getSpawn(templates::spawns::SpawnType type) {
	SYNCHRONIZED(spawns) {
		return spawns.get(type);
	}
}

void House::updateSpawn(templates::spawns::SpawnType type, runtime::Ptr<gameobjects::Npc> npc) {
	runtime::Ptr<gameobjects::Npc> oldSpawn;
	SYNCHRONIZED(spawns) {
		oldSpawn = spawns.put(type, runtime::Ref<gameobjects::Npc>(npc));
	}
	if (oldSpawn)
		oldSpawn->getController().delete_();
}

void House::clearSpawns() {
	SYNCHRONIZED(spawns) {
		spawns.clear();
	}
}

runtime::Ptr<HouseRegistry> House::getRegistry() {
	if (!houseRegistry.get())
		reloadHouseRegistry();
	return houseRegistry.get();
}

void House::resetRegistry() {
	SYNCHRONIZED(*this) {
		getRegistry()->reset();
		houseRegistry.set(nullptr);
	}
}

void House::reloadHouseRegistry() {
	SYNCHRONIZED(*this) {
		houseRegistry.set(HouseRegistry::create(*this));
		if (ownerId.get() != 0 && !isInactive())
			// lockdep: Java's synchronized reloadHouseRegistry loads the registry under the house monitor (House.java:250-254)
			dao::PlayerRegisteredItemsDAO::loadRegistry(*houseRegistry.get());
	}
}

runtime::Ptr<gameobjects::player::PlayerScripts> House::getPlayerScripts() {
	if (!playerScripts.get())
		reloadPlayerScripts();
	return playerScripts.get();
}

void House::reloadPlayerScripts() {
	SYNCHRONIZED(*this) {
		// lockdep: Java's synchronized reloadPlayerScripts loads the scripts under the house monitor (House.java:262-264)
		playerScripts.set(dao::HouseScriptsDAO::getPlayerScripts(getObjectId()));
	}
}

templates::housing::HouseType House::getHouseType() {
	std::optional<templates::housing::HouseType> size = getBuilding()->getSize();
	if (!size) // Java returns null (a switch on it throws NullPointerException)
		throw runtime::NullPointerException("Building " + std::to_string(getBuilding()->getId()) + " has no size");
	return *size;
}

void House::save() {
	SYNCHRONIZED(*this) {
		// lockdep: Java's synchronized save stores the house under its monitor (House.java:270-274)
		dao::HousesDAO::storeHouse(*this);
		if (runtime::Ptr<HouseRegistry> registry = houseRegistry.get())
			registry->save();
	}
}

void House::setPersistentState(PersistentState persistentStateValue) {
	switch (persistentStateValue) {
		case PersistentState::DELETED:
			if (this->persistentState.get() == PersistentState::NEW)
				this->persistentState.set(PersistentState::NOACTION);
			else
				this->persistentState.set(PersistentState::DELETED);
			break;
		case PersistentState::UPDATE_REQUIRED:
			if (this->persistentState.get() == PersistentState::NEW)
				break;
			[[fallthrough]];
		default:
			this->persistentState.set(persistentStateValue);
	}
}

int8_t House::getHouseOwnerStates() {
	using gameobjects::player::HouseOwnerState;
	// logic and enum names surely aren't right, but it's what allegedly got sent on retail some time in the past (?):
	// 1|2 or 2 without owner, 1|2 or 1|4 with owner - we can make it right once we know what these values control
	if (isInactive()) // only houses with owner can be inactive
		return gameobjects::player::getId(HouseOwnerState::BIDDING_ALLOWED);
	else if (ownerId.get() == 0 && !getBids())
		return gameobjects::player::getId(HouseOwnerState::SINGLE_HOUSE);
	else
		return static_cast<int8_t>(gameobjects::player::getId(HouseOwnerState::HAS_OWNER) | gameobjects::player::getId(HouseOwnerState::BIDDING_ALLOWED));
}

void House::setSignNotice(std::string_view notice) {
	signNotice.set(std::string(notice));
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

bool House::canEnter(gameobjects::player::Player& player) {
	if ((getOwnerId() != player.getObjectId() || isInactive()) && !player.hasAccess(configs::administration::AdminConfig::HOUSE_ENTER_ALL.load())) {
		switch (getDoorState()) {
			case HouseDoorState::CLOSED:
				return false;
			case HouseDoorState::CLOSED_EXCEPT_FRIENDS:
				if (!player.getFriendList().getFriend(getOwnerId()) && (!player.getLegion() || !player.getLegion()->isMember(getOwnerId())))
					return false;
				break;
			default:
				break;
		}
	}
	return true;
}

int64_t House::getDefaultAuctionPrice() {
	const templates::housing::Sale* saleOptions = getLand()->getSaleOptions();
	switch (getHouseType()) {
		case templates::housing::HouseType::HOUSE:
			if (configs::main::HousingConfig::HOUSE_MIN_BID.load() > 0)
				return configs::main::HousingConfig::HOUSE_MIN_BID.load();
			break;
		case templates::housing::HouseType::MANSION:
			if (configs::main::HousingConfig::MANSION_MIN_BID.load() > 0)
				return configs::main::HousingConfig::MANSION_MIN_BID.load();
			break;
		case templates::housing::HouseType::ESTATE:
			if (configs::main::HousingConfig::ESTATE_MIN_BID.load() > 0)
				return configs::main::HousingConfig::ESTATE_MIN_BID.load();
			break;
		case templates::housing::HouseType::PALACE:
			if (configs::main::HousingConfig::PALACE_MIN_BID.load() > 0)
				return configs::main::HousingConfig::PALACE_MIN_BID.load();
			break;
		default:
			break;
	}
	if (saleOptions == nullptr) // Java: NullPointerException
		throw runtime::NullPointerException("Cannot invoke \"Sale.getGoldPrice()\" because \"saleOptions\" is null");
	return saleOptions->getGoldPrice();
}

int8_t House::getTeleportHeading() {
	runtime::Ptr<templates::spawns::SpawnTemplate> crystalSpawn = getRelationshipCrystal()->getSpawn();
	return utils::PositionUtil::getHeadingTowards(getX(), getY(), crystalSpawn->getX(), crystalSpawn->getY());
}

int32_t House::getTownLevel() {
	if (getAddress()->getTownId() == 0)
		return 0;
	return services::TownService::getInstance().getTownById(getAddress()->getTownId())->getLevel();
}

int32_t House::secondsUntilGraceEnd() {
	if (isInactive()) {
		commons::database::Timestamp graceEndTime = findGraceEndTime();
		return std::max(0, static_cast<int32_t>((graceEndTime.time_since_epoch().count() - commons::utils::currentTimeMillis()) / 1000));
	}
	return -1;
}

commons::database::Timestamp House::findGraceEndTime() {
	// Java: NullPointerException when the acquired time or a next run is null
	auto required = [](const std::optional<commons::database::Timestamp>& date) {
		if (!date)
			throw runtime::NullPointerException("Cannot invoke \"java.util.Date.getTime()\" because the date is null");
		return *date;
	};
	commons::database::Timestamp acquired = required(getAcquiredTime());
	int64_t maxGraceEndTimeMillis = acquired.time_since_epoch().count() + std::chrono::milliseconds(std::chrono::days(14)).count();
	taskmanager::tasks::housing::AuctionEndTask& auctionEndTask = taskmanager::tasks::housing::AuctionEndTask::getInstance();
	commons::database::Timestamp auctionEndTime = required(auctionEndTask.getNextRunAfter(acquired));
	commons::database::Timestamp graceEndTime = auctionEndTime;
	while ((auctionEndTime = required(auctionEndTask.getNextRunAfter(auctionEndTime))).time_since_epoch().count() <= maxGraceEndTimeMillis)
		graceEndTime = auctionEndTime;
	return graceEndTime;
}

bool House::matchesLandRace(Race race) {
	const templates::npc::NpcTemplate* managerTemplate = dataholders::DataManager::NPC_DATA->getNpcTemplate(getLand()->getManagerNpcId());
	if (managerTemplate == nullptr) // Java: NullPointerException on getTribe()
		throw runtime::NullPointerException("No npc template for house manager " + std::to_string(getLand()->getManagerNpcId()));
	bool isEly = managerTemplate->getTribe() == TribeClass::GENERAL;
	return (race == Race::ELYOS && isEly) || (race == Race::ASMODIANS && !isEly);
}

void House::sendScripts(gameobjects::player::Player& player) {
	runtime::Ptr<gameobjects::player::PlayerScripts> scripts = playerScripts.get();
	if (!scripts)
		return;
	scripts->sendToPlayer(player, address->getId());
}

} // namespace aion::gameserver::model::house
