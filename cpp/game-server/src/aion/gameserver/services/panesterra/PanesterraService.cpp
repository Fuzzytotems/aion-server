#include "aion/gameserver/services/panesterra/PanesterraService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraTeam.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/world/WorldMapType.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"

namespace aion::gameserver::services::panesterra {

static const auto log = commons::logging::LoggerFactory::getLogger("SIEGE_LOG");

void PanesterraService::prepareFortressSiege(model::siege::FortressLocation& loc) {
	AION_UNPORTED();
}

void PanesterraService::prepareBases(const model::templates::siegelocation::SiegeRelatedBases* relatedBases) {
	AION_UNPORTED();
}

void PanesterraService::startFortressSiege(model::siege::FortressLocation& loc) {
	AION_UNPORTED();
}

void PanesterraService::stopFortressSiege(model::siege::FortressLocation& loc) {
	AION_UNPORTED();
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

runtime::Ptr<ahserion::PanesterraTeam> PanesterraService::handleTeamElimination(ahserion::PanesterraFaction faction) {
	AION_UNPORTED();
}

void PanesterraService::createTeams(int32_t siegeId) {
	AION_UNPORTED();
}

void PanesterraService::removeTeams(std::initializer_list<ahserion::PanesterraFaction> factions) {
	AION_UNPORTED();
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

int32_t PanesterraService::getTeamMemberCount(ahserion::PanesterraFaction faction) {
	AION_UNPORTED();
}

runtime::Ptr<ahserion::PanesterraTeam> PanesterraService::getTeam(ahserion::PanesterraFaction faction) {
	AION_UNPORTED();
}

runtime::Ptr<ahserion::PanesterraTeam> PanesterraService::getTeam(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool PanesterraService::teleportToStartPosition(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
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
