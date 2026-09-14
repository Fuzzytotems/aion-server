#include "aion/gameserver/model/house/House.h"

#include "aion/gameserver/runtime/base/Unported.h"

// Member and part types (docs/design/hub-headers.md §3.3): constructors, destructor, postConstruct, the narrowing accessor and the Field<Ref>
// members need complete types whose headers are not S0b hubs (HouseController, HouseBids, HouseRegistry: P5-11; PlayerScripts: P4-12;
// PlayerAwareKnownList: P4-10) plus the VisibleObject part headers of other S0b groups. Not an S0b transition guard: the chunk that adds the last
// of them removes it.
#if __has_include("aion/gameserver/controllers/HouseController.h") && __has_include("aion/gameserver/model/house/HouseBids.h") && \
	__has_include("aion/gameserver/model/house/HouseRegistry.h") && __has_include("aion/gameserver/model/gameobjects/player/PlayerScripts.h") && \
	__has_include("aion/gameserver/world/knownlist/PlayerAwareKnownList.h") && __has_include("aion/gameserver/world/WorldPosition.h") && \
	__has_include("aion/gameserver/controllers/VisibleObjectController.h")
#define AION_HOUSE_MEMBER_TYPES 1
#include "aion/gameserver/controllers/HouseController.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/PlayerScripts.h"
#include "aion/gameserver/model/house/HouseBids.h"
#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/PlayerAwareKnownList.h"
#else
#define AION_HOUSE_MEMBER_TYPES 0
#endif

namespace aion::gameserver::model::house {

#if AION_HOUSE_MEMBER_TYPES
House::House(CreateKey key, const templates::housing::HouseAddress* addressValue, int32_t instanceId)
	: House(key, utils::idfactory::IDFactory::getInstance().nextId(), nullptr, addressValue, instanceId) {
	// Java: address.getLand().getDefaultBuilding() as the building (the HouseAddress/HousingLand shells do not declare the accessors yet)
	AION_UNPORTED();
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
	// Java: resetDoorState(); setPersistentState(PersistentState.UPDATED);
	AION_UNPORTED();
}

controllers::HouseController& House::getController() const {
	return static_cast<controllers::HouseController&>(VisibleObject::getController());
}
#endif

std::string House::getName() {
	AION_UNPORTED();
}

const templates::housing::HousingLand* House::getLand() {
	AION_UNPORTED();
}

world::WorldType House::getWorldType() {
	AION_UNPORTED();
}

void House::setBuilding(const templates::housing::Building* buildingValue) {
	AION_UNPORTED();
}

float House::getVisibleDistance() {
	AION_UNPORTED();
}

void House::setOwnerId(int32_t ownerIdValue) {
	AION_UNPORTED();
}

std::optional<std::string> House::getOwnerName() {
	AION_UNPORTED();
}

void House::setAcquiredTime(std::optional<commons::database::Timestamp> acquiredTimeValue) {
	AION_UNPORTED();
}

int32_t House::getPermissionsForDB() {
	AION_UNPORTED();
}

void House::setPermissionsFromDB(int32_t permissions) {
	AION_UNPORTED();
}

bool House::resetDoorState() {
	AION_UNPORTED();
}

bool House::setDoorState(HouseDoorState doorStateValue) {
	AION_UNPORTED();
}

void House::setShowOwnerName(bool showOwnerNameValue) {
	AION_UNPORTED();
}

bool House::isFeePaid() {
	AION_UNPORTED();
}

void House::setNextPay(std::optional<commons::database::Timestamp> nextPayValue) {
	AION_UNPORTED();
}

void House::setBids(runtime::Ptr<HouseBids> bidsValue, bool resetDoorStateOfUnoccupiedHouse) {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Npc> House::getButler() {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Npc> House::getRelationshipCrystal() {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::Npc> House::getCurrentSign() {
	AION_UNPORTED();
}

// lint: L7 unported stub; Java synchronizes it, the port adds SYNCHRONIZED
runtime::Ptr<gameobjects::Npc> House::getSpawn(templates::spawns::SpawnType type) {
	AION_UNPORTED();
}

// lint: L7 unported stub; Java synchronizes it, the port adds SYNCHRONIZED
void House::updateSpawn(templates::spawns::SpawnType type, runtime::Ptr<gameobjects::Npc> npc) {
	AION_UNPORTED();
}

// lint: L7 unported stub; Java synchronizes it, the port adds SYNCHRONIZED
void House::clearSpawns() {
	AION_UNPORTED();
}

runtime::Ptr<HouseRegistry> House::getRegistry() {
	AION_UNPORTED();
}

// lint: L7 unported stub; Java synchronizes it, the port adds SYNCHRONIZED
void House::resetRegistry() {
	AION_UNPORTED();
}

// lint: L7 unported stub; Java synchronizes it, the port adds SYNCHRONIZED
void House::reloadHouseRegistry() {
	AION_UNPORTED();
}

runtime::Ptr<gameobjects::player::PlayerScripts> House::getPlayerScripts() {
	AION_UNPORTED();
}

// lint: L7 unported stub; Java synchronizes it, the port adds SYNCHRONIZED
void House::reloadPlayerScripts() {
	AION_UNPORTED();
}

templates::housing::HouseType House::getHouseType() {
	AION_UNPORTED();
}

// lint: L7 unported stub; Java synchronizes it, the port adds SYNCHRONIZED
void House::save() {
	AION_UNPORTED();
}

void House::setPersistentState(PersistentState persistentStateValue) {
	AION_UNPORTED();
}

int8_t House::getHouseOwnerStates() {
	AION_UNPORTED();
}

void House::setSignNotice(std::string_view notice) {
	AION_UNPORTED();
}

bool House::canEnter(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

int64_t House::getDefaultAuctionPrice() {
	AION_UNPORTED();
}

int8_t House::getTeleportHeading() {
	AION_UNPORTED();
}

int32_t House::getTownLevel() {
	AION_UNPORTED();
}

int32_t House::secondsUntilGraceEnd() {
	AION_UNPORTED();
}

commons::database::Timestamp House::findGraceEndTime() {
	AION_UNPORTED();
}

bool House::matchesLandRace(Race race) {
	AION_UNPORTED();
}

void House::sendScripts(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::house
