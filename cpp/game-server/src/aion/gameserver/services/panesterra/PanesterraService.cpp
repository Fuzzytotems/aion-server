#include "aion/gameserver/services/panesterra/PanesterraService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"
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


void PanesterraService::prepareFortressSiege(model::siege::FortressLocation& loc) {
	AION_UNPORTED();
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

void PanesterraService::spawnAdvanceCorridors() {
	AION_UNPORTED();
}

void PanesterraService::spawnCorridor(model::templates::spawns::SpawnTemplate& template_, int32_t staticId) {
	AION_UNPORTED();
}

void PanesterraService::startAhserionRaid() {
	AION_UNPORTED();
}

void PanesterraService::stopAhserionRaid() {
	AION_UNPORTED();
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

void PanesterraService::spawnAhserionCorridors(int32_t fortressId) {
	AION_UNPORTED();
}

void PanesterraService::onEnterPanesterra(model::gameobjects::player::Player& player) {
	int32_t siegeId = getSiegeId(player.getWorldId());
	if (siegeId == 0)
		return;
	// M5a (plan E1-07) ports the path of maps outside Panesterra. Java continues with the team check (Transidium Annex or an active siege: bind
	// location, origin position or the team's faction) or the owning faction of the fortresses 10111/10211/10311/10411 (PanesterraFaction and
	// SiegeRace companions), ported with the Panesterra work
	AION_UNPORTED();
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

bool PanesterraService::isAhserionRaidStarted() {
	AION_UNPORTED();
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
// getTeam(Player&) stays AION_UNPORTED: it can only be reached on a Panesterra map, activeFactionTeams is filled by createTeams alone (itself
// unported), and the M5b-1 scripted path and real-client checklist never enter Belus, Aspida, Atanatos, Disillon or the Transidium Annex.
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

bool PanesterraService::reviveInEventLocation(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PanesterraService::teleportToEventLocation(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PanesterraService::teleport(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

PanesterraService& PanesterraService::getInstance() {
	static PanesterraService instance; // Java SingletonHolder
	return instance;
}

} // namespace aion::gameserver::services::panesterra
