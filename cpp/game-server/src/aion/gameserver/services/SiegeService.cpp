#include "aion/gameserver/services/SiegeService.h"

#include <algorithm>
#include <chrono>
#include <optional>
#include <unordered_map>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/configs/schedule/SiegeSchedules.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dao/SiegeDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SiegeLocationData.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/siege/AgentLocation.h"
#include "aion/gameserver/model/siege/ArtifactLocation.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/siege/Influence.h"
#include "aion/gameserver/model/siege/OutpostLocation.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/siege/SiegeModType.h"
#include "aion/gameserver/model/siege/SiegeRaceInfo.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/templates/siegelocation/DoorRepairData.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_ARTIFACT_INFO3.h"
#include "aion/gameserver/network/aion/serverpackets/SM_AFTER_SIEGE_LOCINFO_475.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FORTRESS_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FORTRESS_STATUS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INFLUENCE_RATIO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RIFT_ANNOUNCE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SHIELD_EFFECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SIEGE_LOCATION_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/services/panesterra/PanesterraService.h"
#include "aion/gameserver/services/siege/AgentSiege.h"
#include "aion/gameserver/services/siege/ArtifactSiege.h"
#include "aion/gameserver/services/siege/FortressSiege.h"
#include "aion/gameserver/services/siege/OutpostSiege.h"
#include "aion/gameserver/services/siege/Siege.h"
#include "aion/gameserver/services/siege/SiegeCounter.h"
#include "aion/gameserver/services/siege/SiegeException.h"
#include "aion/gameserver/services/siege/SiegeStartRunnable.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldType.h"

namespace aion::gameserver::services {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous Consumer at SiegeService.java:549 (com.aionemu.gameserver.services.SiegeService$1); argument 1 of forEachPlayer(); storage: sync

using model::siege::SiegeModType;
using model::siege::SiegeRace;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
namespace StringUtils = commons::utils::StringUtils;

static const auto log = commons::logging::LoggerFactory::getLogger("SIEGE_LOG");

/** Java SiegeRace.getL10n() (the L10n default method): the localized name of the race */
static std::string l10nOf(SiegeRace race) {
	return utils::ChatUtil::l10n(model::siege::getL10nId(race));
}

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
template <class Location, class Target>
static void copyLocations(const dataholders::detail::LinkedMap<int32_t, runtime::Ref<Location>>& source, Target& target) {
	for (const auto& [id, location] : source)
		target.put(id, location);
}

SiegeService::SiegeService()
	: agent(initialAgentLocation()),
	  rvrEventPlayers(runtime::RcHashSet<runtime::Ref<model::gameobjects::player::Player>>::create(AION_LOCK_CLASS(SiegeService::rvrEventPlayers))) {
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

// Java SiegeService.java:99-173
void SiegeService::initSieges() {
	if (!isInitialized.compareAndSet(false, true) || !configs::main::SiegeConfig::SIEGE_ENABLED.load())
		return;

	// despawn all NPCs spawned by spawn engine.
	// Siege spawns should be controlled by siege service
	for (int32_t i : getSiegeLocations().keySet()) {
		deSpawnNpcs(i);
	}

	// spawn fortress common npcs
	for (const runtime::Ptr<model::siege::FortressLocation>& f : getFortresses().values()) {
		spawnNpcs(f->getLocationId(), f->getRace(), SiegeModType::PEACE);
	}

	// spawn outpost protectors...
	for (const runtime::Ptr<model::siege::OutpostLocation>& o : getOutposts().values()) {
		spawnNpcs(o->getLocationId(), o->getRace(), SiegeModType::PEACE);
	}

	// spawn artifacts
	for (const runtime::Ptr<model::siege::ArtifactLocation>& a : getStandaloneArtifacts()) {
		spawnNpcs(a->getLocationId(), a->getRace(), SiegeModType::PEACE);
	}

	// initialize siege schedule
	std::unique_ptr<configs::schedule::SiegeSchedules> siegeSchedules = configs::schedule::SiegeSchedules::load();

	// Schedule fortresses sieges protector spawn
	for (const configs::schedule::SiegeSchedules::Fortress& f : siegeSchedules->getFortresses()) {
		for (const std::string& siegeTime : f.getSiegeTimes()) {
			std::string preparationCron = getPreparationCronString(siegeTime, f.getId());
			cron::CronService::getInstance().schedule(cron::CronJob(siege::SiegeStartRunnable{.locationId = f.getId()}), preparationCron);
			log.debug("Scheduled siege of fortressID {} based on cron expression: {}", f.getId(), preparationCron);
		}
	}

	// Schedule agent fights
	for (const configs::schedule::SiegeSchedules::AgentFight& a : siegeSchedules->getAgentFights()) {
		for (const std::string& siegeTime : a.getSiegeTimes()) {
			cron::CronService::getInstance().schedule(cron::CronJob(siege::SiegeStartRunnable{.locationId = a.getId()}), siegeTime);
			log.debug("Scheduled agent fight based on cron expression: {}", siegeTime);
		}
	}

	// Start siege of artifacts
	for (const runtime::Ptr<model::siege::ArtifactLocation>& artifact : artifacts.values()) {
		if (artifact->isStandAlone()) {
			log.debug("Starting siege of artifact #" + std::to_string(artifact->getLocationId()));
			startSiege(artifact->getLocationId());
		} else {
			log.debug("Artifact #{} siege was not started, it belongs to fortress", artifact->getLocationId());
		}
	}

	// We should set valid next state for fortress on startup. No need to broadcast state here, no players @ server ATM
	updateNextStateUpdateTime();
	updateFortressNextState();

	// Schedule siege status broadcast (every hour)
	cron::CronService::getInstance().schedule(cron::CronJob({this}, [this] {
		updateNextStateUpdateTime();
		updateFortressNextState();
		world::World::getInstance().forEachPlayer([this](model::gameobjects::player::Player& player) {
			for (const runtime::Ptr<model::siege::FortressLocation>& fortress : getFortresses().values())
				utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_FORTRESS_INFO(fortress->getLocationId(), false));
			utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_FORTRESS_STATUS());
			for (const runtime::Ptr<model::siege::FortressLocation>& fortress : getFortresses().values())
				utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_FORTRESS_INFO(fortress->getLocationId(), true));
		});
	}), SIEGE_LOCATION_STATUS_BROADCAST_SCHEDULE);
	log.debug("Broadcasting Siege Location status based on expression: {}", SIEGE_LOCATION_STATUS_BROADCAST_SCHEDULE.getCronExpression());
}

// Java SiegeService.java:175-180
void SiegeService::checkSiegeStart(int32_t locationId) {
	if (agent->getLocationId() == locationId)
		startSiege(locationId);
	else
		startPreparations(locationId);
}

// Java SiegeService.java:182-196
void SiegeService::startPreparations(int32_t locationId) {
	log.debug("Starting preparations of siege Location:{}", locationId);
	runtime::Ptr<model::siege::FortressLocation> loc = getFortress(locationId);

	// Set siege start timer..
	utils::ThreadPoolManager::getInstance().schedule(this, [this, locationId] { startSiege(locationId); }, 300 * 1000);

	if (loc->getTemplate()->getMaxOccupyCount() > 0 && loc->getOccupiedCount() >= loc->getTemplate()->getMaxOccupyCount()
		&& loc->getRace() != SiegeRace::BALAUR) {
		log.debug("Resetting fortress to balaur control due to exceeded occupy count! locId:{}", locationId);
		resetSiegeLocation(*loc);
	}

	// TODO:
	if (locationId > 10000) { // Panesterra
		panesterra::PanesterraService::getInstance().prepareFortressSiege(*loc);
	}
}

// Java SiegeService.java:198-218
void SiegeService::startSiege(int32_t siegeLocationId) {
	SYNCHRONIZED(*this) {
		log.debug("Starting siege of siege location: " + std::to_string(siegeLocationId));

		// Siege should not be started two times
		if (activeSieges.containsKey(siegeLocationId)) {
			log.error("Attempt to start siege twice for siege location: " + std::to_string(siegeLocationId), commons::utils::Exception(""));
			return;
		}

		runtime::Ref<siege::Siege> siege = newSiege(siegeLocationId);
		activeSieges.put(siegeLocationId, siege);
		siege->startSiege();

		// certain sieges are endless
		// should end only manually on siege boss death
		if (siege->isEndless())
			return;

		// schedule siege end
		utils::ThreadPoolManager::getInstance().schedule(this, [this, siegeLocationId] { stopSiege(siegeLocationId); },
			siege->getSiegeLocation()->getSiegeDuration() * 1000LL);
	}
}

// Java SiegeService.java:220-231
void SiegeService::stopSiege(int32_t siegeLocationId) {
	SYNCHRONIZED(*this) {
		log.debug("Stopping siege of siege location: {}", siegeLocationId);

		runtime::Ptr<siege::Siege> siege = activeSieges.remove(siegeLocationId);
		if (!siege) {
			log.debug("Siege of siege location {} is not in progress, it was captured earlier?", siegeLocationId);
			return;
		}
		if (siege->isFinished())
			return;
		siege->stopSiege();
	}
}

// Java SiegeService.java:236-262
void SiegeService::captureSiege(SiegeRace sr, int32_t legionId, int32_t locId) {
	SYNCHRONIZED(*this) {
		runtime::Ptr<model::siege::SiegeLocation> loc = getSiegeLocation(locId);
		runtime::Ptr<siege::Siege> s = getSiege(locId);
		if (s) {
			s->getSiegeCounter()->addRaceDamage(sr, s->getBoss()->getLifeStats()->getMaxHp() + 1);
			s->setBossKilled(true);
			stopSiege(locId);
			loc->setLegionId(legionId);
		} else {
			deSpawnNpcs(locId);
			loc->setVulnerable(false);
			loc->setUnderShield(false);
			loc->setRace(sr);
			loc->setLegionId(legionId);
			spawnNpcs(locId, sr, SiegeModType::PEACE);
			// lockdep: Java writes the location under the synchronized method too (SiegeService.java:251)
			dao::SiegeDAO::updateSiegeLocation(*loc);
			switch (locId) {
				case 2011:
				case 2021:
				case 3011:
				case 3021:
					updateOutpostSiegeState(*runtime::cast<model::siege::FortressLocation>(loc));
					break;
			}
		}
		broadcastUpdate(*loc);
	}
}

// Java SiegeService.java:267-304
void SiegeService::resetSiegeLocation(model::siege::SiegeLocation& loc) {
	SYNCHRONIZED(*this) {
		// Despawn old npc
		deSpawnNpcs(loc.getLocationId());
		loc.clearLocation(); // remove all players

		// Store old owner for msg
		const int32_t oldOwnerRaceId = model::siege::getRaceId(loc.getRace());
		const int32_t legionId = loc.getLegionId();
		const std::string legionName = legionId != 0 ? LegionService::getInstance().getLegion(legionId)->getName() : "";
		const std::string locL10n = loc.getTemplate()->getL10n();

		// Reset owner
		loc.setRace(SiegeRace::BALAUR);
		loc.setLegionId(0);

		if (runtime::as<model::siege::FortressLocation>(loc)) {
			runtime::Ptr<model::siege::ArtifactLocation> artifact = getFortressArtifact(loc.getLocationId());
			if (artifact) {
				artifact->setRace(SiegeRace::BALAUR);
				artifact->setLegionId(0);
			}
		}
		loc.setOccupiedCount(0);

		// On start preparations msg
		world::World::getInstance().forEachPlayer([&](model::gameobjects::player::Player& player) {
			if (legionId != 0 && model::getRaceId(player.getRace()) == oldOwnerRaceId)
				utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_ABYSS_GUILD_CASTLE_TAKEN(legionName, locL10n));
			utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_ABYSS_WIN_CASTLE(l10nOf(loc.getRace()), locL10n));
			utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SIEGE_LOCATION_INFO(loc));
		});
		broadcastUpdate(loc);

		// Spawn new npc
		spawnNpcs(loc.getLocationId(), SiegeRace::BALAUR, SiegeModType::PEACE);

		// lockdep: Java writes the location under the synchronized method too (SiegeService.java:303)
		dao::SiegeDAO::updateSiegeLocation(loc);
	}
}

// Java SiegeService.java:309-320
void SiegeService::updateFortressNextState() {
	std::unordered_map<int32_t, commons::database::Timestamp> startDatesByLocId = collectNextSiegeStartDates();

	// update each fortress next state
	for (const auto& [locationId, date] : startDatesByLocId) {
		// update fortress state that will be valid within the next hour
		runtime::Ptr<model::siege::SiegeLocation> fortress = getSiegeLocation(locationId);
		const std::optional<commons::database::Timestamp> nextStateUpdate = nextStateUpdateTime.get();
		if (!nextStateUpdate)
			throw runtime::NullPointerException("nextStateUpdateTime"); // Java: Date.after(null)
		if (!(date > *nextStateUpdate)) // date is > now and <= next update time (this check also accounts for the preparation time)
			fortress->setNextState(model::siege::SiegeLocation::STATE_VULNERABLE);
		else
			fortress->setNextState(model::siege::SiegeLocation::STATE_INVULNERABLE);
	}
}

// Java SiegeService.java:322-334
std::unordered_map<int32_t, commons::database::Timestamp> SiegeService::collectNextSiegeStartDates() {
	// Java: findNextFireTimes(SiegeStartRunnable.class, true) - one entry per scheduled runnable (each schedule() above is a new one)
	std::vector<std::pair<runtime::Ref<cron::JobDetail>, std::chrono::sys_seconds>> dates =
		cron::CronService::getInstance().findNextFireTimes<siege::SiegeStartRunnable>();
	std::unordered_map<int32_t, commons::database::Timestamp> nextSiegeStartDates;
	for (const auto& [job, fireTime] : dates) {
		const commons::database::Timestamp date = std::chrono::time_point_cast<std::chrono::milliseconds>(fireTime);
		const int32_t locationId = job->getJob().target<siege::SiegeStartRunnable>()->getLocationId();
		auto it = nextSiegeStartDates.find(locationId);
		if (it == nextSiegeStartDates.end())
			nextSiegeStartDates.emplace(locationId, date);
		else if (it->second > date) // Java: oldDate.after(date)
			it->second = date;
	}
	return nextSiegeStartDates;
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

// Java SiegeService.java:401-406
const model::templates::siegelocation::DoorRepairData* SiegeService::getDoorRepairData(int32_t siegeId) {
	runtime::Ptr<model::siege::FortressLocation> fortressLocation = getFortress(siegeId);
	if (!fortressLocation)
		return nullptr;
	return fortressLocation->getTemplate()->getDoorRepairData();
}

// Java SiegeService.java:408-413
const model::templates::siegelocation::DoorRepairStone* SiegeService::getRepairStone(int32_t siegeId, int32_t repairStoneStaticId) {
	const model::templates::siegelocation::DoorRepairData* doorRepairData = getDoorRepairData(siegeId);
	if (doorRepairData == nullptr)
		return nullptr;
	return doorRepairData->getRepairStone(repairStoneStaticId);
}

runtime::Ptr<model::siege::SiegeLocation> SiegeService::getSiegeLocation(int32_t id) {
	return locations.get(id);
}

std::vector<std::pair<int32_t, runtime::Ptr<model::siege::SiegeLocation>>> SiegeService::getSiegeLocations(int32_t worldId) {
	// Java: new LinkedHashMap<>() put in the iteration order of `locations` (ids are unique, so each put appends)
	std::vector<std::pair<int32_t, runtime::Ptr<model::siege::SiegeLocation>>> mapLocations;
	for (const runtime::Ptr<model::siege::SiegeLocation>& location : getSiegeLocations().values())
		if (location->getWorldId() == worldId)
			mapLocations.emplace_back(location->getLocationId(), location);

	return mapLocations;
}

// Java SiegeService.java:436-447
runtime::Ref<siege::Siege> SiegeService::newSiege(int32_t siegeLocationId) {
	if (fortresses.containsKey(siegeLocationId))
		return siege::FortressSiege::create(*fortresses.get(siegeLocationId));
	else if (outposts.containsKey(siegeLocationId))
		return siege::OutpostSiege::create(*outposts.get(siegeLocationId));
	else if (artifacts.containsKey(siegeLocationId))
		return siege::ArtifactSiege::create(*artifacts.get(siegeLocationId));
	else if (agent && agent->getLocationId() == siegeLocationId)
		return siege::AgentSiege::create(*agent);
	else
		throw siege::SiegeException("Unknown siege handler for siege location: " + std::to_string(siegeLocationId));
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

// Java SiegeService.java:458-489
void SiegeService::updateOutpostSiegeState(model::siege::FortressLocation& fortressLoc) {
	SYNCHRONIZED(*this) {
		for (const runtime::Ptr<model::siege::OutpostLocation>& outpost : getOutposts().values()) {
			const std::vector<int32_t> dependencies = outpost->getFortressDependency();
			if (std::find(dependencies.begin(), dependencies.end(), fortressLoc.getLocationId()) == dependencies.end())
				continue;

			if (std::any_of(dependencies.begin(), dependencies.end(), [this](int32_t dependency) { return getSiegeLocation(dependency)->isVulnerable(); }))
				break;

			const SiegeRace validRaceForSiege = outpost->getLocationId() == 2111 ? SiegeRace::ASMODIANS : SiegeRace::ELYOS;
			bool isSiegeAllowed = true;

			for (int32_t fortressId : dependencies) {
				const SiegeRace dependencyFortressRace = getFortresses().get(fortressId)->getRace();
				if (validRaceForSiege != dependencyFortressRace) {
					isSiegeAllowed = false;
					break;
				}
			}

			stopSiege(outpost->getLocationId());
			deSpawnNpcs(outpost->getLocationId());

			// broadcast to all new Silentera infiltration route state
			broadcastStatusAndUpdate(*outpost, outpost->isSilenteraAllowed());

			// spawn NPC's or sieges
			if (isSiegeAllowed)
				startSiege(outpost->getLocationId());
			else
				spawnNpcs(outpost->getLocationId(), outpost->getRace(), SiegeModType::PEACE);
		}
	}
}

// Java SiegeService.java:491-503
void SiegeService::spawnNpcs(int32_t siegeLocationId, model::siege::SiegeRace race, model::siege::SiegeModType type) {
	const std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>>* siegeSpawns = dataholders::DataManager::SPAWNS_DATA->getSiegeSpawnsByLocId(siegeLocationId);
	if (siegeSpawns == nullptr)
		return;
	for (const runtime::Ref<model::templates::spawns::SpawnGroup>& group : *siegeSpawns) {
		for (const runtime::Ptr<model::templates::spawns::SpawnTemplate>& template_ : group->getSpawnTemplates().snapshot()) {
			runtime::Ptr<model::templates::spawns::siegespawns::SiegeSpawnTemplate> siegetemplate =
				runtime::cast<model::templates::spawns::siegespawns::SiegeSpawnTemplate>(template_);
			if (siegetemplate->getSiegeRace() == race && siegetemplate->getSiegeModType() == type) {
				spawnengine::SpawnEngine::spawnObject(*siegetemplate, 1);
			}
		}
	}
}

// Java SiegeService.java:505-509
void SiegeService::deSpawnNpcs(int32_t siegeLocationId) {
	// iterate over an array copy, since onDelete directly modifies the underlying collection
	for (const runtime::Ptr<model::gameobjects::siege::SiegeNpc>& siegeNpc : world::World::getInstance().getLocalSiegeNpcs(siegeLocationId))
		siegeNpc->getController().delete_();
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

// Java SiegeService.java:524-532
void SiegeService::broadcastUpdate(model::siege::SiegeLocation& loc) {
	model::siege::Influence::getInstance().recalculateInfluence();
	network::aion::serverpackets::SM_SIEGE_LOCATION_INFO pkt1(loc);
	network::aion::serverpackets::SM_INFLUENCE_RATIO pkt2;
	world::World::getInstance().forEachPlayer([&](model::gameobjects::player::Player& player) {
		utils::PacketSendUtility::sendPacket(player, pkt1);
		utils::PacketSendUtility::sendPacket(player, pkt2);
	});
}

// Java SiegeService.java:534-546
void SiegeService::broadcastStatusAndUpdate(model::siege::OutpostLocation& outpost, bool oldSilentraState) {
	std::optional<SM_SYSTEM_MESSAGE> info; // Java: null
	if (oldSilentraState != outpost.isSilenteraAllowed()) {
		if (outpost.isSilenteraAllowed())
			info = outpost.getLocationId() == 2111 ? SM_SYSTEM_MESSAGE::STR_FIELDABYSS_LIGHTUNDERPASS_SPAWN()
				: SM_SYSTEM_MESSAGE::STR_FIELDABYSS_DARKUNDERPASS_SPAWN();
		else
			info = outpost.getLocationId() == 2111 ? SM_SYSTEM_MESSAGE::STR_FIELDABYSS_LIGHTUNDERPASS_DESPAWN()
				: SM_SYSTEM_MESSAGE::STR_FIELDABYSS_DARKUNDERPASS_DESPAWN();
	}

	network::aion::serverpackets::SM_RIFT_ANNOUNCE rift(getOutpost(3111)->isSilenteraAllowed(), getOutpost(2111)->isSilenteraAllowed());
	broadcast(rift, info ? &*info : nullptr);
}

// Java SiegeService.java:548-559. The anonymous Consumer (SiegeService$1, K5 confined) is the lambda: forEachPlayer runs it synchronously.
void SiegeService::broadcast(network::aion::serverpackets::SM_RIFT_ANNOUNCE& rift, network::aion::serverpackets::SM_SYSTEM_MESSAGE* info) {
	world::World::getInstance().forEachPlayer([&rift, info](model::gameobjects::player::Player& player) {
		utils::PacketSendUtility::sendPacket(player, rift);
		if (info != nullptr && player.getWorldType() == world::WorldType::BALAUREA)
			utils::PacketSendUtility::sendPacket(player, *info);
	});
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
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_INFLUENCE_RATIO());
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SIEGE_LOCATION_INFO());
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_AFTER_SIEGE_LOCINFO_475());
		utils::PacketSendUtility::sendPacket(player,
			network::aion::serverpackets::SM_RIFT_ANNOUNCE(getOutpost(3111)->isSilenteraAllowed(), getOutpost(2111)->isSilenteraAllowed()));
	}
}

// Java SiegeService.java:590-605. Reached from CM_LEVEL_READY on every arrival in a siege world (Inggison, Gelkmaros, Reshanta), with no
// siege-config guard: with sieges disabled both maps are empty and the two packets are sent empty (m5f-plan.md W-03).
void SiegeService::onEnterSiegeWorld(model::gameobjects::player::Player& player) {
	// Second part only for siege world
	// Java: two LinkedHashMaps keyed by location id, filled in the iteration order of the service's maps; put() of a present key replaces the
	// value in place. The service's maps are runtime::LinkedHashMaps (header request 5a-pre-10b), so the order is Java's.
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

// Java SiegeService.java:607-610
void SiegeService::onAbyssPointsAdded(model::gameobjects::player::Player& player, model::gameobjects::VisibleObject& obj, int32_t abyssPoints) {
	runtime::Ptr<model::gameobjects::siege::SiegeNpc> siegeNpc;
	if (runtime::as<model::gameobjects::player::Player>(obj)
		|| ((siegeNpc = runtime::as<model::gameobjects::siege::SiegeNpc>(obj)) && siegeNpc->getSpawn()->getSiegeModType() != SiegeModType::PEACE))
		for (const runtime::Ptr<siege::Siege>& a : activeSieges.values())
			a->onAbyssPointsAdded(player, abyssPoints);
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

// Java SiegeService.java:683-686
void SiegeService::checkRvrEventPlayer(runtime::Ptr<model::gameobjects::player::Player> player) {
	runtime::Ptr<runtime::RcHashSet<runtime::Ref<model::gameobjects::player::Player>>> players = rvrEventPlayers.get();
	if (player && !players->contains(runtime::Ref<model::gameobjects::player::Player>(player)))
		players->add(runtime::Ref<model::gameobjects::player::Player>(player));
}

// Java SiegeService.java:688-690
void SiegeService::clearRvrEventPlayers() {
	rvrEventPlayers.set(runtime::RcHashSet<runtime::Ref<model::gameobjects::player::Player>>::create(AION_LOCK_CLASS(SiegeService::rvrEventPlayers)));
}

/** Java Byte.parseByte(s): Integer.parseInt plus the byte range check, NumberFormatException otherwise */
static int8_t parseByte(std::string_view s) {
	const int32_t value = commons::utils::parseInt(s);
	if (value < -128 || value > 127)
		throw commons::utils::NumberFormatException("Value out of range. Value:\"" + std::string(s) + "\" Radix:10");
	return static_cast<int8_t>(value);
}

// Java SiegeService.java:697-715
std::string SiegeService::getPreparationCronString(std::string_view siegeTime, int32_t fortressId) {
	try {
		std::vector<std::string> cronParts = StringUtils::splitJava(siegeTime, " ");
		auto part = [&cronParts](size_t index) -> const std::string& { // Java: cronParts[index], an uncaught ArrayIndexOutOfBoundsException
			if (index >= cronParts.size())
				throw runtime::ArrayIndexOutOfBoundsException("Index " + std::to_string(index) + " out of bounds for length " + std::to_string(cronParts.size()));
			return cronParts[index];
		};
		int8_t minutes = parseByte(part(1));
		int8_t hours = parseByte(part(2));
		minutes = static_cast<int8_t>(minutes - static_cast<int8_t>(fortressId < 10000 ? 5 : 10)); // Java: byte -= (byte) ...
		if (minutes < 0) {
			minutes = static_cast<int8_t>(minutes + 60);
			hours = static_cast<int8_t>(hours - 1);
			if (hours < 0)
				throw commons::utils::UnsupportedOperationException("Failed converting cron expression: " + std::string(siegeTime) +
					"\nPreparation over midnight not supported.");
		}
		cronParts[1] = std::to_string(minutes);
		cronParts[2] = std::to_string(hours);
		return StringUtils::join(cronParts, " ");
	} catch (const commons::utils::NumberFormatException&) {
		throw commons::utils::UnsupportedOperationException("Failed converting cron expression: " + std::string(siegeTime), std::current_exception());
	}
}

} // namespace aion::gameserver::services
