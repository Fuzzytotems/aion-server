#include "aion/gameserver/services/CubeExpandService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.CubeExpandService");

// anonymous RequestResponseHandler at CubeExpandService.java:50 (fieldmap key CubeExpandService$1); local responseHandler; storage: stored in ResponseRequester
void CubeExpandService::expandCube(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

void CubeExpandService::expand(model::gameobjects::player::Player& player, int32_t type) {
	AION_UNPORTED();
}

void CubeExpandService::questExpand(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void CubeExpandService::itemExpand(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void CubeExpandService::npcExpand(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool CubeExpandService::canExpandByTicket(model::gameobjects::player::Player& player, int32_t ticketLevel) {
	AION_UNPORTED();
}

bool CubeExpandService::canExpand(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
