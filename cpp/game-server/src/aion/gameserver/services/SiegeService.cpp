#include "aion/gameserver/services/SiegeService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/siege/AgentLocation.h"
#include "aion/gameserver/model/siege/ArtifactLocation.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/siege/OutpostLocation.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/siege/Siege.h"

namespace aion::gameserver::services {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous Consumer at SiegeService.java:549 (com.aionemu.gameserver.services.SiegeService$1); argument 1 of forEachPlayer(); storage: sync

static const auto log = commons::logging::LoggerFactory::getLogger("SIEGE_LOG");

const cron::CronExpression SiegeService::SIEGE_LOCATION_STATUS_BROADCAST_SCHEDULE("0 0 * ? * *");

SiegeService& SiegeService::getInstance() {
	static SiegeService instance; // Java SingletonHolder
	return instance;
}

SiegeService::SiegeService() {
	AION_UNPORTED();
}

void SiegeService::updateNextStateUpdateTime() {
	AION_UNPORTED();
}

void SiegeService::initSieges() {
	AION_UNPORTED();
}

void SiegeService::checkSiegeStart(int32_t locationId) {
	AION_UNPORTED();
}

void SiegeService::startPreparations(int32_t locationId) {
	AION_UNPORTED();
}

void SiegeService::startSiege(int32_t siegeLocationId) { // lint: L7 unported stub; the port adds the Java synchronized block
	AION_UNPORTED();
}

void SiegeService::stopSiege(int32_t siegeLocationId) { // lint: L7 unported stub; the port adds the Java synchronized block
	AION_UNPORTED();
}

// lint: L7 unported stub; the port adds the Java synchronized block
void SiegeService::captureSiege(model::siege::SiegeRace sr, int32_t legionId, int32_t locId) {
	AION_UNPORTED();
}

void SiegeService::resetSiegeLocation(model::siege::SiegeLocation& loc) {
	AION_UNPORTED();
}

void SiegeService::updateFortressNextState() {
	AION_UNPORTED();
}

std::unordered_map<int32_t, commons::database::Timestamp> SiegeService::collectNextSiegeStartDates() {
	AION_UNPORTED();
}

int32_t SiegeService::getSecondsUntilNextFortressState() {
	AION_UNPORTED();
}

int32_t SiegeService::getRemainingSiegeTimeInSeconds(int32_t siegeLocationId) {
	AION_UNPORTED();
}

runtime::Ptr<siege::Siege> SiegeService::getSiege(model::siege::SiegeLocation& loc) {
	AION_UNPORTED();
}

runtime::Ptr<siege::Siege> SiegeService::getSiege(int32_t siegeLocationId) {
	AION_UNPORTED();
}

bool SiegeService::isSiegeInProgress(int32_t fortressId) {
	AION_UNPORTED();
}

runtime::Ptr<model::siege::OutpostLocation> SiegeService::getOutpost(int32_t id) {
	AION_UNPORTED();
}

runtime::Ptr<model::siege::FortressLocation> SiegeService::getFortress(int32_t id) {
	AION_UNPORTED();
}

runtime::Ptr<model::siege::ArtifactLocation> SiegeService::getArtifact(int32_t id) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::siege::ArtifactLocation>> SiegeService::getStandaloneArtifacts() {
	AION_UNPORTED();
}

runtime::Ptr<model::siege::ArtifactLocation> SiegeService::getFortressArtifact(int32_t siegeLocId) {
	AION_UNPORTED();
}

const model::templates::siegelocation::DoorRepairData* SiegeService::getDoorRepairData(int32_t siegeId) {
	AION_UNPORTED();
}

const model::templates::siegelocation::DoorRepairStone* SiegeService::getRepairStone(int32_t siegeId, int32_t repairStoneStaticId) {
	AION_UNPORTED();
}

runtime::Ptr<model::siege::SiegeLocation> SiegeService::getSiegeLocation(int32_t id) {
	AION_UNPORTED();
}

std::map<int32_t, runtime::Ptr<model::siege::SiegeLocation>> SiegeService::getSiegeLocations(int32_t worldId) {
	AION_UNPORTED();
}

runtime::Ref<siege::Siege> SiegeService::newSiege(int32_t siegeLocationId) {
	AION_UNPORTED();
}

void SiegeService::cleanLegionId(int32_t legionId) {
	AION_UNPORTED();
}

void SiegeService::updateOutpostSiegeState(model::siege::FortressLocation& fortressLoc) {
	AION_UNPORTED();
}

void SiegeService::spawnNpcs(int32_t siegeLocationId, model::siege::SiegeRace race, model::siege::SiegeModType type) {
	AION_UNPORTED();
}

void SiegeService::deSpawnNpcs(int32_t siegeLocationId) {
	AION_UNPORTED();
}

bool SiegeService::isRespawnAllowed(model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

void SiegeService::broadcastUpdate(model::siege::SiegeLocation& loc) {
	AION_UNPORTED();
}

void SiegeService::broadcastStatusAndUpdate(model::siege::OutpostLocation& outpost, bool oldSilentraState) {
	AION_UNPORTED();
}

void SiegeService::broadcast(network::aion::serverpackets::SM_RIFT_ANNOUNCE& rift, network::aion::serverpackets::SM_SYSTEM_MESSAGE& info) {
	AION_UNPORTED();
}

runtime::Ptr<model::siege::FortressLocation> SiegeService::findFortress(int32_t worldId, float x, float y, float z) {
	AION_UNPORTED();
}

void SiegeService::onPlayerLogin(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void SiegeService::onEnterSiegeWorld(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void SiegeService::onAbyssPointsAdded(model::gameobjects::player::Player& player, model::gameobjects::VisibleObject& obj, int32_t abyssPoints) {
	AION_UNPORTED();
}

int32_t SiegeService::getSiegeIdByLocId(int32_t locId) {
	AION_UNPORTED();
}

void SiegeService::checkRvrEventPlayer(runtime::Ptr<model::gameobjects::player::Player> player) {
	AION_UNPORTED();
}

void SiegeService::clearRvrEventPlayers() {
	AION_UNPORTED();
}

std::string SiegeService::getPreparationCronString(std::string_view siegeTime, int32_t fortressId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
