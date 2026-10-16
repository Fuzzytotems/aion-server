#include "aion/gameserver/services/panesterra/PanesterraService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include <string>
#include <vector>
#include "aion/gameserver/model/base/BaseLocation.h"
#include "aion/gameserver/model/base/BaseOccupier.h"
#include "aion/gameserver/model/base/BaseType.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/model/templates/base/BaseTemplate.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeRelatedBases.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/BaseService.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/siege/SiegeRaceInfo.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraFactionInfo.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/services/panesterra/ahserion/AhserionRaid.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraFaction.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraTeam.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/world/WorldMapType.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"

namespace aion::gameserver::services::panesterra {

static const auto log = commons::logging::LoggerFactory::getLogger("SIEGE_LOG");

namespace {
/** Java: relatedBases.getBaseIds().stream().map(BaseService.getInstance()::getBaseLocation) - a missing list or location is Java's NullPointerException */
std::vector<runtime::Ptr<model::base::BaseLocation>> relatedBaseLocations(const model::templates::siegelocation::SiegeRelatedBases& relatedBases) {
	if (!relatedBases.getBaseIds())
		throw runtime::NullPointerException("SiegeRelatedBases.baseIds");
	std::vector<runtime::Ptr<model::base::BaseLocation>> locations;
	for (int32_t id : *relatedBases.getBaseIds()) {
		runtime::Ptr<model::base::BaseLocation> baseLoc = BaseService::getInstance().getBaseLocation(id);
		if (!baseLoc) // Java: the switch on null.getType()
			throw runtime::NullPointerException("BaseLocation " + std::to_string(id));
		locations.push_back(baseLoc);
	}
	return locations;
}
} // namespace


// Java PanesterraService.java:63-67
void PanesterraService::prepareFortressSiege(model::siege::FortressLocation& loc) {
	createTeams(loc.getLocationId());
	spawnAdvanceCorridors();
	prepareBases(loc.getTemplate()->getSiegeRelatedBases());
}

// Java PanesterraService.java:69-80
void PanesterraService::prepareBases(const model::templates::siegelocation::SiegeRelatedBases* relatedBases) {
	if (relatedBases == nullptr)
		return;

	for (const runtime::Ptr<model::base::BaseLocation>& baseLoc : relatedBaseLocations(*relatedBases)) {
		switch (baseLoc->getType()) {
			case model::base::BaseType::PANESTERRA:
				BaseService::getInstance().capture(baseLoc->getId(), model::base::BaseOccupier::PEACE);
				break;
			case model::base::BaseType::PANESTERRA_ARTIFACT:
				BaseService::getInstance().start(baseLoc->getId());
				break;
			case model::base::BaseType::PANESTERRA_FACTION_CAMP:
				BaseService::getInstance().capture(baseLoc->getId(), baseLoc->getTemplate()->getDefaultOccupier());
				break;
			default:
				break;
		}
	}
}

// Java PanesterraService.java:82-89
void PanesterraService::startFortressSiege(model::siege::FortressLocation& loc) {
	const model::templates::siegelocation::SiegeRelatedBases* relatedBases = loc.getTemplate()->getSiegeRelatedBases();
	if (relatedBases != nullptr) {
		for (const runtime::Ptr<model::base::BaseLocation>& baseLoc : relatedBaseLocations(*relatedBases))
			if (baseLoc->getType() == model::base::BaseType::PANESTERRA_ARTIFACT)
				BaseService::getInstance().capture(baseLoc->getId(), model::base::BaseOccupier::BALAUR);
	}
}

// Java PanesterraService.java:91-111
void PanesterraService::stopFortressSiege(model::siege::FortressLocation& loc) {
	// Remove Teams
	switch (loc.getLocationId()) {
		case 10111:
			removeTeams({ahserion::PanesterraFaction::IVY_TEMPLE, ahserion::PanesterraFaction::HIGHLAND_TEMPLE, ahserion::PanesterraFaction::ALPINE_TEMPLE, ahserion::PanesterraFaction::GRANDWEIR_TEMPLE});
			break;
		case 10211:
			removeTeams({ahserion::PanesterraFaction::NOERREN_TEMPLE, ahserion::PanesterraFaction::BOREALIS_TEMPLE, ahserion::PanesterraFaction::MYRKREN_TEMPLE, ahserion::PanesterraFaction::GLUMVEILEN_TEMPLE});
			break;
		case 10311:
			removeTeams({ahserion::PanesterraFaction::MEMORIA_TEMPLE, ahserion::PanesterraFaction::SYBILLINE_TEMPLE, ahserion::PanesterraFaction::AUSTERITY_TEMPLE, ahserion::PanesterraFaction::SERENITY_TEMPLE});
			break;
		case 10411:
			removeTeams({ahserion::PanesterraFaction::NECROLUCE_TEMPLE, ahserion::PanesterraFaction::ESMERAUDUS_TEMPLE, ahserion::PanesterraFaction::VOLTAIC_TEMPLE, ahserion::PanesterraFaction::ILLUMINATUS_TEMPLE});
			break;
	}
	// Change base states
	const model::templates::siegelocation::SiegeRelatedBases* relatedBases = loc.getTemplate()->getSiegeRelatedBases();
	if (relatedBases != nullptr) {
		for (const runtime::Ptr<model::base::BaseLocation>& baseLoc : relatedBaseLocations(*relatedBases)) {
			switch (baseLoc->getType()) {
				case model::base::BaseType::PANESTERRA:
					BaseService::getInstance().capture(baseLoc->getId(), model::base::BaseOccupier::BALAUR);
					break;
				case model::base::BaseType::PANESTERRA_ARTIFACT:
					BaseService::getInstance().stop(baseLoc->getId());
					break;
				case model::base::BaseType::PANESTERRA_FACTION_CAMP:
					BaseService::getInstance().capture(baseLoc->getId(), model::base::BaseOccupier::PEACE);
					break;
				default:
					break;
			}
		}
	}
}

// Java PanesterraService.java:125-144
void PanesterraService::spawnAdvanceCorridors() {
	// Elyos
	utils::PacketSendUtility::broadcastToMap(*world::World::getInstance().getWorldMap(110070000)->getMainWorldMapInstance(),
		network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_SVS_INVADE_DIRECT_PORTAL_OPEN());
	// Governor exclusive
	spawnCorridor(*spawnengine::SpawnEngine::newSingleTimeSpawn(110070000, 730940, 503.624f, 460.202f, 132.081f, int8_t{90}), 257);
	// Advance Corridor for Contributors | Officer 5-Star to Commander
	spawnCorridor(*spawnengine::SpawnEngine::newSingleTimeSpawn(110070000, 730942, 490.262f, 409.850f, 126.79f, int8_t{90}), 256);
	// Walk of Honor | Officer 1-Star to 4-Star
	spawnCorridor(*spawnengine::SpawnEngine::newSingleTimeSpawn(110070000, 731193, 518.142f, 409.967f, 126.79f, int8_t{90}), 252);
	// Asmodians
	utils::PacketSendUtility::broadcastToMap(*world::World::getInstance().getWorldMap(120080000)->getMainWorldMapInstance(),
		network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_SVS_INVADE_DIRECT_PORTAL_OPEN());
	// Governor exclusive
	spawnCorridor(*spawnengine::SpawnEngine::newSingleTimeSpawn(120080000, 730941, 342.298f, 251.135f, 98.553f, int8_t{0}), 338);
	// Advance Corridor for Contributors | Officer 5-Star to Commander
	spawnCorridor(*spawnengine::SpawnEngine::newSingleTimeSpawn(120080000, 730943, 393.321f, 236.963f, 93.113f, int8_t{0}), 337);
	// Walk of Glory | Officer 1-Star to 4-Star
	spawnCorridor(*spawnengine::SpawnEngine::newSingleTimeSpawn(120080000, 731194, 393.476f, 263.704f, 93.113f, int8_t{0}), 336);
}

// Java PanesterraService.java:146-149
void PanesterraService::spawnCorridor(model::templates::spawns::SpawnTemplate& template_, int32_t staticId) {
	template_.setStaticId(staticId);
	spawnengine::SpawnEngine::spawnObject(template_, 1);
}

// Java PanesterraService.java:151-160
void PanesterraService::startAhserionRaid() {
	for (int32_t id : {10111, 10211, 10311, 10411}) {
		if (SiegeService::getInstance().getSiege(id) != nullptr) {
			log.error("Ahserion raid cannot be started while any Panesterra fortress is under siege.");
			return;
		}
	}
	createTeams(-1);
	spawnengine::SpawnEngine::spawnObject(*spawnengine::SpawnEngine::newSingleTimeSpawn(110070000, 802223, 485.692f, 401.079f, 127.789f, int8_t{0}), 1);
	spawnengine::SpawnEngine::spawnObject(*spawnengine::SpawnEngine::newSingleTimeSpawn(120080000, 802225, 400.772f, 231.517f, 93.113f, int8_t{30}), 1);
	ahserion::AhserionRaid::getInstance().start();
}

// Java PanesterraService.java:162-171
void PanesterraService::stopAhserionRaid() {
	ahserion::AhserionRaid::getInstance().stop();
	if (!activeFactionTeams.isEmpty()) {
		for (const runtime::Ptr<ahserion::PanesterraTeam>& team : activeFactionTeams.values()) {
			team->setIsEliminated(true);
			team->moveTeamMembersToOriginPosition();
		}
		activeFactionTeams.clear();
	}
}

// Java PanesterraService.java:173-181
runtime::Ptr<ahserion::PanesterraTeam> PanesterraService::handleTeamElimination(ahserion::PanesterraFaction faction) {
	runtime::Ptr<ahserion::PanesterraTeam> team = activeFactionTeams.get(faction);
	if (!team)
		return nullptr; // Using the //base command

	team->setIsEliminated(true);
	team->moveTeamMembersToOriginPosition();
	return team;
}

// Java PanesterraService.java:183-220
void PanesterraService::createTeams(int32_t siegeId) {
	const auto put = [this](ahserion::PanesterraFaction faction) { activeFactionTeams.put(faction, ahserion::PanesterraTeam::create(faction)); };
	const auto notBalaur = [](int32_t fortressId) {
		runtime::Ptr<model::siege::SiegeLocation> location = SiegeService::getInstance().getSiegeLocation(fortressId);
		if (!location) // Java: getSiegeLocation(id).getRace() on null
			throw runtime::NullPointerException("SiegeLocation " + std::to_string(fortressId));
		return location->getRace() != model::siege::SiegeRace::BALAUR;
	};
	switch (siegeId) {
		case -1: // Transidium Annex
			if (notBalaur(10111))
				put(ahserion::PanesterraFaction::BELUS);
			if (notBalaur(10211))
				put(ahserion::PanesterraFaction::ASPIDA);
			if (notBalaur(10311))
				put(ahserion::PanesterraFaction::ATANATOS);
			if (notBalaur(10411))
				put(ahserion::PanesterraFaction::DISILLON);
			break;
		case 10111: // Belus
			put(ahserion::PanesterraFaction::IVY_TEMPLE);
			put(ahserion::PanesterraFaction::HIGHLAND_TEMPLE);
			put(ahserion::PanesterraFaction::ALPINE_TEMPLE);
			put(ahserion::PanesterraFaction::GRANDWEIR_TEMPLE);
			break;
		case 10211: // Aspida
			put(ahserion::PanesterraFaction::NOERREN_TEMPLE);
			put(ahserion::PanesterraFaction::BOREALIS_TEMPLE);
			put(ahserion::PanesterraFaction::MYRKREN_TEMPLE);
			put(ahserion::PanesterraFaction::GLUMVEILEN_TEMPLE);
			break;
		case 10311: // Atanatos
			put(ahserion::PanesterraFaction::MEMORIA_TEMPLE);
			put(ahserion::PanesterraFaction::SYBILLINE_TEMPLE);
			put(ahserion::PanesterraFaction::AUSTERITY_TEMPLE);
			put(ahserion::PanesterraFaction::SERENITY_TEMPLE);
			break;
		case 10411: // Disillon
			put(ahserion::PanesterraFaction::NECROLUCE_TEMPLE);
			put(ahserion::PanesterraFaction::ESMERAUDUS_TEMPLE);
			put(ahserion::PanesterraFaction::VOLTAIC_TEMPLE);
			put(ahserion::PanesterraFaction::ILLUMINATUS_TEMPLE);
			break;
	}
}

// Java PanesterraService.java:222-227
void PanesterraService::removeTeams(std::initializer_list<ahserion::PanesterraFaction> factions) {
	for (ahserion::PanesterraFaction faction : factions) {
		runtime::Ptr<ahserion::PanesterraTeam> team = activeFactionTeams.remove(faction);
		if (!team) // Java: team.moveTeamMembersToOriginPosition() on null
			throw runtime::NullPointerException("PanesterraTeam");
		team->moveTeamMembersToOriginPosition();
	}
}

// Java PanesterraService.java:229-236
void PanesterraService::spawnAhserionCorridors(int32_t fortressId) {
	switch (fortressId) {
		case 10111:
			spawnCorridor(*spawnengine::SpawnEngine::newSingleTimeSpawn(400020000, 802219, 1024.159f, 1076.24f, 1530.2688f, int8_t{90}), 0);
			break;
		case 10211:
			spawnCorridor(*spawnengine::SpawnEngine::newSingleTimeSpawn(400040000, 802221, 1024.159f, 1076.24f, 1530.2688f, int8_t{90}), 0);
			break;
		case 10311:
			spawnCorridor(*spawnengine::SpawnEngine::newSingleTimeSpawn(400050000, 802223, 1024.159f, 1076.24f, 1530.2688f, int8_t{90}), 0);
			break;
		case 10411:
			spawnCorridor(*spawnengine::SpawnEngine::newSingleTimeSpawn(400060000, 802225, 1024.159f, 1076.24f, 1530.2688f, int8_t{90}), 0);
			break;
	}
}

// Java PanesterraService.java:238-266
void PanesterraService::onEnterPanesterra(model::gameobjects::player::Player& player) {
	int32_t siegeId = getSiegeId(player.getWorldId());
	if (siegeId == 0)
		return;
	// Player is in Transidium Annex or on a map with an active siege
	if (siegeId == -1 || SiegeService::getInstance().isSiegeInProgress(siegeId)) {
		runtime::Ptr<ahserion::PanesterraTeam> team = getTeam(player);
		if (!team)
			teleport::TeleportService::moveToBindLocation(player);
		else if (team->isEliminated())
			team->movePlayerToOriginPosition(player);
		else
			player.setPanesterraFaction(team->getFaction());

	} else {
		// Check if the player's faction owns any related fortress
		ahserion::PanesterraFaction faction = ahserion::PanesterraFaction::PEACE;
		for (int32_t id : {10111, 10211, 10311, 10411}) {
			runtime::Ptr<model::siege::FortressLocation> fortress = SiegeService::getInstance().getFortress(id);
			if (!fortress) // Java: getFortress(id).getRace() on null
				throw runtime::NullPointerException("FortressLocation " + std::to_string(id));
			if (fortress->getRace() == model::siege::getByRace(player.getRace())) {
				faction = ahserion::getByFortressId(id);
				break;
			}
		}

		if (faction == ahserion::PanesterraFaction::PEACE)
			teleport::TeleportService::moveToBindLocation(player);

		player.setPanesterraFaction(faction);
	}
}

int32_t PanesterraService::getSiegeId(int32_t worldId) {
	std::optional<world::WorldMapType> world = world::getWorldMapType(worldId);
	if (!world)
		return 0; // Java: case null
	switch (*world) {
		case world::WorldMapType::BELUS:
			return 10111; // Belus
		case world::WorldMapType::TRANSIDIUM_ANNEX:
			return -1; // Transidium Annex
		case world::WorldMapType::ASPIDA:
			return 10211; // Aspida
		case world::WorldMapType::ATANATOS:
			return 10311; // Atanatos
		case world::WorldMapType::DISILLON:
			return 10411; // Disillon
		default:
			return 0;
	}
}

// Java PanesterraService.java:276-278
bool PanesterraService::isAhserionRaidStarted() {
	return ahserion::AhserionRaid::getInstance().isStarted();
}

// Java PanesterraService.java:280-283
int32_t PanesterraService::getTeamMemberCount(ahserion::PanesterraFaction faction) {
	runtime::Ptr<ahserion::PanesterraTeam> team = activeFactionTeams.get(faction);
	return team ? team->getMemberCount() : 0;
}

// Java PanesterraService.java:285-287
runtime::Ptr<ahserion::PanesterraTeam> PanesterraService::getTeam(ahserion::PanesterraFaction faction) {
	return activeFactionTeams.get(faction);
}

// Java PanesterraService.java:289-295
runtime::Ptr<ahserion::PanesterraTeam> PanesterraService::getTeam(model::gameobjects::player::Player& player) {
	for (const runtime::Ptr<ahserion::PanesterraTeam>& team : activeFactionTeams.values()) {
		if (team->isTeamMember(player.getObjectId()))
			return team;
	}
	return nullptr;
}

// Java PanesterraService.java:297-307
//
// Ported for m5b-plan.md C-03: PlayerReviveService::bindRevive calls this unconditionally for every character that is neither in prison nor in
// EVENT_MODE (PlayerReviveService.java:124), which is the M5b-1 gate's death case on Poeta. Java's first statement answers false for every map
// outside Panesterra, so the whole body below is dead on that path - but it has to *return* false there rather than throw.
bool PanesterraService::teleportToStartPosition(model::gameobjects::player::Player& player) {
	if (!world::isPanesterraMap(player.getWorldId()))
		return false;

	runtime::Ptr<ahserion::PanesterraTeam> team = getTeam(player);
	if (team && !team->isEliminated()) {
		team->movePlayerToStartPosition(player);
		return true;
	}
	return false;
}

// Java PanesterraService.java:310-316
bool PanesterraService::reviveInEventLocation(model::gameobjects::player::Player& player) {
	if (!world::isPanesterraMap(player.getWorldId()))
		return false;

	teleportToEventLocation(player);
	return true;
}

// Java PanesterraService.java:318-321
void PanesterraService::teleportToEventLocation(model::gameobjects::player::Player& player) {
	teleport(player);
	PanesterraService::getInstance().onEnterPanesterra(player);
}

// Java PanesterraService.java:323-338
void PanesterraService::teleport(model::gameobjects::player::Player& player) {
	switch (player.getRace()) {
		case model::Race::ELYOS: {
			// North + South
			runtime::Ref<world::WorldPosition> pos = commons::utils::Rnd::nextBoolean()
				? world::WorldPosition::create(400020000, 11.173f, 1024.187f, 1428.60f, int8_t{0})
				: world::WorldPosition::create(400020000, 2037.754f, 1023.808f, 1428.60f, int8_t{0});
			teleport::TeleportService::teleportTo(player, *pos);
			break;
		}
		case model::Race::ASMODIANS: {
			// West + East
			runtime::Ref<world::WorldPosition> pos = commons::utils::Rnd::nextBoolean()
				? world::WorldPosition::create(400020000, 1023.702f, 10.531f, 1428.60f, int8_t{90})
				: world::WorldPosition::create(400020000, 1024.310f, 2036.593f, 1428.60f, int8_t{90});
			teleport::TeleportService::teleportTo(player, *pos);
			break;
		}
		default:
			break;
	}
}

PanesterraService& PanesterraService::getInstance() {
	static PanesterraService instance; // Java SingletonHolder
	return instance;
}

} // namespace aion::gameserver::services::panesterra
