#include "aion/gameserver/services/DuelService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.DuelService");

DuelService::DuelService() {
	log.info("DuelService started.");
}

DuelService::~DuelService() = default;

DuelService& DuelService::getInstance() {
	static DuelService instance; // Java SingletonHolder
	return instance;
}

// anonymous RequestResponseHandler at DuelService.java:83 (fieldmap key DuelService$1); local rrh; storage: stored in ResponseRequester
void DuelService::onDuelRequest(model::gameobjects::player::Player& requester, runtime::Ptr<model::gameobjects::player::Player> targetPlayer) {
	AION_UNPORTED();
}

// anonymous RequestResponseHandler at DuelService.java:119 (fieldmap key DuelService$2); local rrh; storage: stored in ResponseRequester
void DuelService::confirmDuelWith(model::gameobjects::player::Player& requester, model::gameobjects::player::Player& targetPlayer) {
	AION_UNPORTED();
}

void DuelService::rejectDuelRequest(model::gameobjects::player::Player& requester, model::gameobjects::player::Player& responder) {
	AION_UNPORTED();
}

void DuelService::cancelDuelRequest(model::gameobjects::player::Player& canceller, model::gameobjects::player::Player& target) {
	AION_UNPORTED();
}

void DuelService::startDuel(model::gameobjects::player::Player& requester, model::gameobjects::player::Player& responder) {
	AION_UNPORTED();
}

void DuelService::fixTeamVisibility(model::gameobjects::player::Player& hiddenDuelist) {
	AION_UNPORTED();
}

void DuelService::loseDuel(model::gameobjects::player::Player& loser) {
	AION_UNPORTED();
}

void DuelService::endDebuffsByOpponent(model::gameobjects::player::Player& player, int32_t opponentId) {
	AION_UNPORTED();
}

void DuelService::cancelSummonedObjectAttacks(model::gameobjects::player::Player& target, int32_t summonerId) {
	AION_UNPORTED();
}

void DuelService::createTask(model::gameobjects::player::Player& requester, model::gameobjects::player::Player& responder) {
	AION_UNPORTED();
}

void DuelService::onDuelEnd(model::DuelResult duelResult, model::gameobjects::player::Player& player, int32_t opponentId) {
	AION_UNPORTED();
}

std::optional<int32_t> DuelService::getOpponentId(model::gameobjects::player::Player& player) {
	return duels.get(player.getObjectId());
}

bool DuelService::isDueling(model::gameobjects::player::Player& player) {
	std::optional<int32_t> opponentId = getOpponentId(player);
	return opponentId && duels.get(*opponentId);
}

bool DuelService::isDueling(model::gameobjects::player::Player& player, model::gameobjects::player::Player& opponent) {
	std::optional<int32_t> opponentId = getOpponentId(player);
	return opponentId && *opponentId == opponent.getObjectId();
}

void DuelService::registerDuel(int32_t requesterObjId, int32_t responderObjId) {
	duels.put(requesterObjId, responderObjId);
	duels.put(responderObjId, requesterObjId);
}

void DuelService::removeDuel(model::gameobjects::player::Player& player) {
	std::optional<int32_t> opponentId = duels.remove(player.getObjectId());
	if (opponentId) {
		duels.remove(*opponentId);
		removeAndEndTask(player.getObjectId());
		removeAndEndTask(*opponentId);
	}
}

void DuelService::removeAndEndTask(int32_t playerId) {
	runtime::Ptr<runtime::Future> task = drawTasks.remove(playerId);
	if (task)
		task->cancel(false);
}

} // namespace aion::gameserver::services
