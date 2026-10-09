#include "aion/gameserver/services/SiegeService.h"

#include <algorithm>
#include <chrono>
#include <optional>
#include <unordered_map>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/dao/SiegeDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SiegeLocationData.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/siege/AgentLocation.h"
#include "aion/gameserver/model/siege/ArtifactLocation.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/siege/OutpostLocation.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_ARTIFACT_INFO3.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SHIELD_EFFECT.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/siege/Siege.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

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

/** Java DataManager.SIEGE_LOCATION_DATA.getAgentLoc() in the enabled branch; null while sieges are disabled (the field is final in Java) */
static runtime::Ref<model::siege::AgentLocation> initialAgentLocation() {
	if (!configs::main::SiegeConfig::SIEGE_ENABLED.load())
		return nullptr;
	return runtime::Ref<model::siege::AgentLocation>(dataholders::DataManager::SIEGE_LOCATION_DATA->getAgentLoc());
}

/** copies a location map of the static data into the service's map (Java assigns the data holder's map itself) */
template <class Location>
static void copyLocations(const dataholders::detail::LinkedMap<int32_t, runtime::Ref<Location>>& source,
	runtime::HashMap<int32_t, runtime::Ref<Location>>& target) {
	for (const auto& [id, location] : source)
		target.put(id, location);
}

SiegeService::SiegeService() : agent(initialAgentLocation()) {
	if (configs::main::SiegeConfig::SIEGE_ENABLED.load()) {
		log.info("Initializing sieges...");

		// initialize current siege locations
		copyLocations(dataholders::DataManager::SIEGE_LOCATION_DATA->getArtifacts(), artifacts);
		copyLocations(dataholders::DataManager::SIEGE_LOCATION_DATA->getFortress(), fortresses);
		copyLocations(dataholders::DataManager::SIEGE_LOCATION_DATA->getOutpost(), outposts);
		copyLocations(dataholders::DataManager::SIEGE_LOCATION_DATA->getSiegeLocations(), locations);
		std::unordered_map<int32_t, runtime::Ptr<model::siege::SiegeLocation>> locationsById;
		for (const auto& [id, location] : dataholders::DataManager::SIEGE_LOCATION_DATA->getSiegeLocations())
			locationsById.emplace(id, location);
		dao::SiegeDAO::loadSiegeLocations(locationsById);
	} else {
		// Java: Collections.emptyMap() for artifacts, fortresses, outposts and locations; the member maps stay empty
		log.info("Sieges are disabled in config.");
	}
}

void SiegeService::updateNextStateUpdateTime() {
	// Java: getTimeAfter(new Date()), in the CronExpression's default zone (the server time zone); null if there is no next fire time
	std::optional<std::chrono::sys_seconds> next = SIEGE_LOCATION_STATUS_BROADCAST_SCHEDULE.getTimeAfter(
		std::chrono::floor<std::chrono::seconds>(commons::database::Timestamp(std::chrono::milliseconds(commons::utils::currentTimeMillis()))),
		configs::main::GSConfig::TIME_ZONE_ID.load());
	nextStateUpdateTime.set(next ? std::optional<commons::database::Timestamp>(std::chrono::time_point_cast<std::chrono::milliseconds>(*next))
		: std::nullopt);
}

void SiegeService::initSieges() {
	if (!isInitialized.compareAndSet(false, true) || !configs::main::SiegeConfig::SIEGE_ENABLED.load())
		return;

	// M5a keeps sieges disabled (plan D1): despawning the spawn engine's siege NPCs, the siege spawns, SiegeSchedules, the siege start
	// cron jobs, the standalone artifact sieges and the hourly status broadcast follow with the siege work (plan O-05)
	AION_UNPORTED();
}

void SiegeService::checkSiegeStart(int32_t locationId) {
	AION_UNPORTED();
}

void SiegeService::startPreparations(int32_t locationId) {
	AION_UNPORTED();
}

void SiegeService::startSiege(int32_t siegeLocationId) {
	AION_UNPORTED();
}

void SiegeService::stopSiege(int32_t siegeLocationId) {
	AION_UNPORTED();
}

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
	std::optional<commons::database::Timestamp> nextStateUpdate = nextStateUpdateTime.get();
	if (!nextStateUpdate) // null if siege service is deactivated
		return 0;
	// Java: (int) (millis difference) / 1000, the cast binds before the division
	return static_cast<int32_t>(nextStateUpdate->time_since_epoch().count() - commons::utils::currentTimeMillis()) / 1000;
}

int32_t SiegeService::getRemainingSiegeTimeInSeconds(int32_t siegeLocationId) {
	runtime::Ptr<siege::Siege> siege = getSiege(siegeLocationId);
	if (!siege || siege->isFinished() || !siege->isStarted())
		return 0;

	int64_t endTime = siege->getStartTime() / 1000 + siege->getSiegeLocation()->getSiegeDuration();
	int32_t secondsLeft = static_cast<int32_t>(endTime - commons::utils::currentTimeMillis() / 1000);

	return std::max(secondsLeft, 0);
}

runtime::Ptr<siege::Siege> SiegeService::getSiege(model::siege::SiegeLocation& loc) {
	return getSiege(loc.getLocationId());
}

runtime::Ptr<siege::Siege> SiegeService::getSiege(int32_t siegeLocationId) {
	return activeSieges.get(siegeLocationId);
}

bool SiegeService::isSiegeInProgress(int32_t fortressId) {
	return activeSieges.containsKey(fortressId);
}

runtime::Ptr<model::siege::OutpostLocation> SiegeService::getOutpost(int32_t id) {
	return outposts.get(id);
}

runtime::Ptr<model::siege::FortressLocation> SiegeService::getFortress(int32_t id) {
	return fortresses.get(id);
}

runtime::Ptr<model::siege::ArtifactLocation> SiegeService::getArtifact(int32_t id) {
	return getArtifacts().get(id);
}

std::vector<runtime::Ptr<model::siege::ArtifactLocation>> SiegeService::getStandaloneArtifacts() {
	std::vector<runtime::Ptr<model::siege::ArtifactLocation>> standaloneArtifacts;
	for (const runtime::Ptr<model::siege::ArtifactLocation>& artifact : artifacts.values()) {
		if (artifact->isStandAlone())
			standaloneArtifacts.push_back(artifact);
	}
	return standaloneArtifacts;
}

runtime::Ptr<model::siege::ArtifactLocation> SiegeService::getFortressArtifact(int32_t siegeLocId) {
	runtime::Ptr<model::siege::ArtifactLocation> loc = getArtifact(siegeLocId);
	return !loc || !loc->getOwningFortress() ? nullptr : loc;
}

const model::templates::siegelocation::DoorRepairData* SiegeService::getDoorRepairData(int32_t siegeId) {
	AION_UNPORTED();
}

const model::templates::siegelocation::DoorRepairStone* SiegeService::getRepairStone(int32_t siegeId, int32_t repairStoneStaticId) {
	AION_UNPORTED();
}

runtime::Ptr<model::siege::SiegeLocation> SiegeService::getSiegeLocation(int32_t id) {
	return locations.get(id);
}

std::map<int32_t, runtime::Ptr<model::siege::SiegeLocation>> SiegeService::getSiegeLocations(int32_t worldId) {
	std::map<int32_t, runtime::Ptr<model::siege::SiegeLocation>> mapLocations;
	for (const runtime::Ptr<model::siege::SiegeLocation>& location : getSiegeLocations().values())
		if (location->getWorldId() == worldId)
			mapLocations.insert_or_assign(location->getLocationId(), location);

	return mapLocations;
}

runtime::Ref<siege::Siege> SiegeService::newSiege(int32_t siegeLocationId) {
	AION_UNPORTED();
}

void SiegeService::cleanLegionId(int32_t legionId) {
	// P5-11 lease (m5h-plan.md I-02, S-06)
	for (const runtime::Ref<model::siege::SiegeLocation>& loc : getSiegeLocations().values()) {
		if (loc->getLegionId() == legionId) {
			loc->setLegionId(0);
			dao::SiegeDAO::updateSiegeLocation(*loc);
		}
	}
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
	// Java SiegeService.java:511-522. Reached on every npc death through NpcAI.ask(ALLOW_RESPAWN) (NpcAI.java:148), so the plain-npc path
	// below - instanceof false, return true - is the hot one. `npc` is an Npc, not a SiegeNpc, so getSpawn() is VisibleObject::getSpawn and
	// getRespawnTime() the spawn group's, exactly as Java has it.
	if (runtime::Ptr<model::gameobjects::siege::SiegeNpc> siegeNpc = runtime::as<model::gameobjects::siege::SiegeNpc>(npc)) {
		runtime::Ptr<model::siege::FortressLocation> fort = getFortress(siegeNpc->getSiegeId());
		if (fort) {
			if (fort->isVulnerable())
				return false;
			else if (fort->getNextState() == model::siege::SiegeLocation::STATE_VULNERABLE)
				return npc.getSpawn()->getRespawnTime() < getSecondsUntilNextFortressState();
		}
	}
	return true;
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
	for (const runtime::Ptr<model::siege::FortressLocation>& fortress : getFortresses().values()) {
		if (fortress->getWorldId() == worldId && fortress->isInsideLocation(x, y, z))
			return fortress;
	}
	return nullptr;
}

void SiegeService::onPlayerLogin(model::gameobjects::player::Player& player) {
	// not on login
	// PacketSendUtility.sendPacket(player, new SM_ABYSS_ARTIFACT_INFO(getSiegeLocations().values()));
	// PacketSendUtility.sendPacket(player, new SM_ABYSS_ARTIFACT_INFO2(getSiegeLocations().values()));

	// Chk login when teleporter is dead
	// for (FortressLocation loc : getFortresses().values()) {
	// // remove teleportation to dead teleporters
	// if (!loc.isCanTeleport(player))
	// PacketSendUtility.sendPacket(player, new SM_FORTRESS_INFO(loc.getLocationId(), false));
	// }

	// First part will be sent to all
	if (configs::main::SiegeConfig::SIEGE_ENABLED.load()) {
		// M5a keeps sieges disabled (plan D1); the enabled branch sends SM_INFLUENCE_RATIO, SM_SIEGE_LOCATION_INFO, SM_AFTER_SIEGE_LOCINFO_475 and
		// SM_RIFT_ANNOUNCE with the Silentera states of outposts 3111 and 2111 (plan O-05)
		AION_UNPORTED();
	}
}

// Java SiegeService.java:590-605. Reached from CM_LEVEL_READY on every arrival in a siege world (Inggison, Gelkmaros, Reshanta), with no
// siege-config guard: with sieges disabled both maps are empty and the two packets are sent empty (m5f-plan.md W-03).
void SiegeService::onEnterSiegeWorld(model::gameobjects::player::Player& player) {
	// Second part only for siege world
	// Java: two LinkedHashMaps keyed by location id, filled in the iteration order of the service's maps; put() of a present key replaces the
	// value in place. The C++ source maps are runtime::HashMaps (docs/deviations/P5-12a.md, the location-map rows), so the order differs.
	std::vector<int32_t> worldLocationIds;
	std::vector<runtime::Ptr<model::siege::SiegeLocation>> worldLocations;
	std::vector<int32_t> worldArtifactIds;
	std::vector<runtime::Ptr<model::siege::ArtifactLocation>> worldArtifacts;

	for (const runtime::Ptr<model::siege::SiegeLocation>& location : getSiegeLocations().values())
		if (location->getWorldId() == player.getWorldId()) {
			auto it = std::find(worldLocationIds.begin(), worldLocationIds.end(), location->getLocationId());
			if (it == worldLocationIds.end()) {
				worldLocationIds.push_back(location->getLocationId());
				worldLocations.push_back(location);
			} else {
				worldLocations[static_cast<size_t>(it - worldLocationIds.begin())] = location;
			}
		}

	for (const runtime::Ptr<model::siege::ArtifactLocation>& artifact : getArtifacts().values())
		if (artifact->getWorldId() == player.getWorldId()) {
			auto it = std::find(worldArtifactIds.begin(), worldArtifactIds.end(), artifact->getLocationId());
			if (it == worldArtifactIds.end()) {
				worldArtifactIds.push_back(artifact->getLocationId());
				worldArtifacts.push_back(artifact);
			} else {
				worldArtifacts[static_cast<size_t>(it - worldArtifactIds.begin())] = artifact;
			}
		}

	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SHIELD_EFFECT(worldLocations));
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_ABYSS_ARTIFACT_INFO3(worldArtifacts));
}

void SiegeService::onAbyssPointsAdded(model::gameobjects::player::Player& player, model::gameobjects::VisibleObject& obj, int32_t abyssPoints) {
	AION_UNPORTED();
}

// Java SiegeService.java:612-674. The first call of every npc teleport (TeleportService.java:81, m5f-plan.md W-02).
int32_t SiegeService::getSiegeIdByLocId(int32_t locId) {
	switch (locId) {
		case 49:
		case 61:
			return 1011; // Divine Fortress
		case 36:
		case 54:
			return 1131; // Siel's Western Fortress
		case 37:
		case 55:
			return 1132; // Siel's Eastern Fortress
		case 39:
		case 56:
			return 1141; // Sulfur Archipelago
		case 44:
		case 62:
			return 1211; // Roah Fortress
		case 45:
		case 57:
		case 72:
		case 75:
			return 1221; // Krotan Refuge
		case 46:
		case 58:
		case 73:
		case 76:
			return 1231; // Kysis Fortress
		case 47:
		case 59:
		case 74:
		case 77:
			return 1241; // Miren Fortress
		case 48:
		case 60:
			return 1251; // Asteria Fortress
		case 90:
			return 2011; // Temple of Scales
		case 91:
			return 2021; // Altar of Avarice
		case 93:
			return 3011; // Vorgaltem Citadel
		case 94:
			return 3021; // Crimson Temple
		case 322:
		case 323:
		case 358:
		case 359:
			return 7011; // Wealhtheow Fortress
		case 316:
		case 317:
		case 368:
		case 369:
			return 7012; // Hero's Fall Artifact
		case 370:
		case 371:
			return 7013; // Ashen Glade Artifact
		case 372:
		case 373:
			return 7014; // Molten Cliffs Artifact
		default:
			return 0;
	}
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
