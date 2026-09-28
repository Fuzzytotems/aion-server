#include "aion/gameserver/services/teleport/PortalService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services::teleport {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.teleport.PortalService");

void PortalService::port(const model::templates::portal::PortalPath* portalPath, model::gameobjects::player::Player& player,
	model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

void PortalService::port(const model::templates::portal::PortalPath* portalPath, model::gameobjects::player::Player& player,
	model::gameobjects::Npc& npc, int8_t difficult) {
	AION_UNPORTED();
}

bool PortalService::checkMentor(model::gameobjects::player::Player& player, int32_t mapId) {
	AION_UNPORTED();
}

bool PortalService::checkEnterLevel(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
	const model::templates::portal::PortalPath* portalPath, const model::templates::InstanceCooltime* instanceRestrictions) {
	AION_UNPORTED();
}

bool PortalService::checkRace(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
	const model::templates::portal::PortalPath* portalPath) {
	AION_UNPORTED();
}

bool PortalService::checkSiegeId(model::gameobjects::player::Player& player, int32_t sigeId) {
	AION_UNPORTED();
}

bool PortalService::checkRank(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
	const model::templates::portal::PortalPath* portalPath) {
	AION_UNPORTED();
}

bool PortalService::checkPlayerSize(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
	const model::templates::portal::PortalPath* portalPath, int32_t maxPlayers) {
	AION_UNPORTED();
}

bool PortalService::checkTitle(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
	const model::templates::portal::PortalPath* portalPath) {
	AION_UNPORTED();
}

bool PortalService::checkQuests(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
	const model::templates::portal::PortalPath* portalPath) {
	AION_UNPORTED();
}

bool PortalService::checkAndRemoveRequiredItems(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
	const model::templates::portal::PortalPath* portalPath) {
	AION_UNPORTED();
}

void PortalService::port(model::gameobjects::player::Player& requester, const model::templates::portal::PortalLoc* loc, bool reenter,
	int32_t maxPlayers) {
	AION_UNPORTED();
}

void PortalService::transfer(model::gameobjects::player::Player& player, const model::templates::portal::PortalLoc* loc,
	world::WorldMapInstance& instance, bool reenter) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::teleport
