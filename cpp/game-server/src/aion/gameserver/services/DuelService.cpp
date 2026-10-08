#include "aion/gameserver/services/DuelService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/InstanceConfig.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/attack/AggroInfo.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/model/DuelResult.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/DeniedStatus.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CLOSE_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DELETE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DUEL.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlot.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.DuelService");

using model::gameobjects::Creature;
using model::gameobjects::player::Player;
using network::aion::serverpackets::SM_CLOSE_QUESTION_WINDOW;
using network::aion::serverpackets::SM_DUEL;
using network::aion::serverpackets::SM_QUESTION_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

/**
 * Java: the anonymous RequestResponseHandler<Player> of onDuelRequest (DuelService.java:83-96, fieldmap key DuelService$1), stored in the
 * target's ResponseRequester until he answers STR_DUEL_DO_YOU_ACCEPT_REQUEST. It captures the immortal DuelService (`this`); the requester is
 * the base's. A friend of DuelService (header request m5j-s1-01): it calls the private rejectDuelRequest and startDuel, as Java's inner class does
 */
class DuelService_RequestHandler final : public model::gameobjects::player::RequestResponseHandler {
	AION_MAKE_REF_FRIEND
public:
	DuelService* const duelService; // captured this [captured this: final immortal reference]

	static runtime::Ref<DuelService_RequestHandler> create(DuelService& service, Player& requester) {
		return runtime::makeRef<DuelService_RequestHandler>(service, requester);
	}

	// Java DuelService.java:85-89
	void denyRequest(runtime::Ptr<Creature> requester, Player& responder) override {
		duelService->rejectDuelRequest(*runtime::cast<Player>(requester), responder);
	}

	// Java DuelService.java:90-95
	void acceptRequest(runtime::Ptr<Creature> requester, Player& responder) override {
		Player& requesterPlayer = *runtime::cast<Player>(requester); // Java's bridge cast of the generic Player requester
		if (!duelService->isDueling(requesterPlayer))
			duelService->startDuel(requesterPlayer, responder);
	}

protected:
	DuelService_RequestHandler(DuelService& service, Player& requester)
		: RequestResponseHandler(runtime::Ptr<Creature>(requester)), duelService(&service) {}
	~DuelService_RequestHandler() override = default;
};

/**
 * Java: the anonymous RequestResponseHandler<Player> of confirmDuelWith (DuelService.java:119-126, fieldmap key DuelService$2), stored in the
 * requester's ResponseRequester until he answers STR_DUEL_DO_YOU_WITHDRAW_REQUEST; its requester is the duel's target. A friend of DuelService
 * (header request m5j-s1-01): it calls the private cancelDuelRequest
 */
class DuelService_WithdrawHandler final : public model::gameobjects::player::RequestResponseHandler {
	AION_MAKE_REF_FRIEND
public:
	DuelService* const duelService; // captured this [captured this: final immortal reference]

	static runtime::Ref<DuelService_WithdrawHandler> create(DuelService& service, Player& targetPlayer) {
		return runtime::makeRef<DuelService_WithdrawHandler>(service, targetPlayer);
	}

	// Java DuelService.java:121-125: acceptRequest(Player targetPlayer, Player responder)
	void acceptRequest(runtime::Ptr<Creature> targetPlayer, Player& responder) override {
		duelService->cancelDuelRequest(responder, *runtime::cast<Player>(targetPlayer));
	}

protected:
	DuelService_WithdrawHandler(DuelService& service, Player& targetPlayer)
		: RequestResponseHandler(runtime::Ptr<Creature>(targetPlayer)), duelService(&service) {}
	~DuelService_WithdrawHandler() override = default;
};

DuelService::DuelService() {
	log.info("DuelService started.");
}

DuelService::~DuelService() = default;

DuelService& DuelService::getInstance() {
	static DuelService instance; // Java SingletonHolder
	return instance;
}

// Java DuelService.java:50-104; the anonymous RequestResponseHandler at :83 (fieldmap key DuelService$1) is DuelService_RequestHandler
void DuelService::onDuelRequest(model::gameobjects::player::Player& requester, runtime::Ptr<model::gameobjects::player::Player> targetPlayer) {
	if (targetPlayer == nullptr || requester.equals(*targetPlayer)) {
		PacketSendUtility::sendPacket(requester, SM_SYSTEM_MESSAGE::STR_DUEL_NO_USER_TO_REQUEST());
		return;
	}
	if (requester.isInInstance() && !configs::main::InstanceConfig::INSTANCE_DUEL_ENABLE.load()) {
		PacketSendUtility::sendPacket(requester, SM_SYSTEM_MESSAGE::STR_MSG_DUEL_CANT_IN_THIS_ZONE());
		return;
	}
	if (isDueling(requester)) {
		PacketSendUtility::sendPacket(requester, SM_SYSTEM_MESSAGE::STR_DUEL_YOU_ARE_IN_DUEL_ALREADY());
		return;
	}
	if (isDueling(*targetPlayer)) {
		PacketSendUtility::sendPacket(requester, SM_SYSTEM_MESSAGE::STR_DUEL_PARTNER_IN_DUEL_ALREADY(targetPlayer->getName()));
		return;
	}
	if (targetPlayer->getPlayerSettings()->isInDeniedStatus(model::gameobjects::player::DeniedStatus::DUEL)) {
		PacketSendUtility::sendPacket(requester, SM_SYSTEM_MESSAGE::STR_MSG_REJECTED_DUEL(targetPlayer->getName()));
		return;
	}
	if (requester.isDead() || targetPlayer->isDead()) {
		PacketSendUtility::sendPacket(requester, SM_SYSTEM_MESSAGE::STR_DUEL_PARTNER_INVALID(targetPlayer->getName()));
		return;
	}
	for (const runtime::Ptr<world::zone::ZoneInstance>& zone : targetPlayer->findZones()) {
		if ((!zone->isOtherRaceDuelsAllowed() && targetPlayer->getRace() != requester.getRace())
			|| (!zone->isSameRaceDuelsAllowed() && targetPlayer->getRace() == requester.getRace())) {
			PacketSendUtility::sendPacket(requester, SM_SYSTEM_MESSAGE::STR_MSG_DUEL_CANT_IN_THIS_ZONE());
			return;
		}
	}

	runtime::Ref<DuelService_RequestHandler> rrh = DuelService_RequestHandler::create(*this, requester);
	if (targetPlayer->getResponseRequester().putRequest(SM_QUESTION_WINDOW::STR_DUEL_DO_YOU_ACCEPT_REQUEST, rrh)) {
		PacketSendUtility::sendPacket(*targetPlayer, SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_DUEL_DO_YOU_ACCEPT_REQUEST, 0, 0, requester.getName()));
		PacketSendUtility::sendPacket(*targetPlayer, SM_SYSTEM_MESSAGE::STR_DUEL_REQUESTED(requester.getName()));
		confirmDuelWith(requester, *targetPlayer);
	} else {
		PacketSendUtility::sendPacket(requester, SM_SYSTEM_MESSAGE::STR_DUEL_CANT_REQUEST_WHEN_HE_IS_ASKED_QUESTION(targetPlayer->getName()));
	}
}

// Java DuelService.java:114-130; the anonymous RequestResponseHandler at :119 (fieldmap key DuelService$2) is DuelService_WithdrawHandler
void DuelService::confirmDuelWith(model::gameobjects::player::Player& requester, model::gameobjects::player::Player& targetPlayer) {
	// Check if requester isn't already in a duel and responder is same race
	if (requester.isEnemy(targetPlayer))
		return;

	runtime::Ref<DuelService_WithdrawHandler> rrh = DuelService_WithdrawHandler::create(*this, targetPlayer);
	requester.getResponseRequester().putRequest(SM_QUESTION_WINDOW::STR_DUEL_DO_YOU_WITHDRAW_REQUEST, rrh);
	PacketSendUtility::sendPacket(requester, SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_DUEL_DO_YOU_WITHDRAW_REQUEST, 0, 0, targetPlayer.getName()));
	PacketSendUtility::sendPacket(requester, SM_SYSTEM_MESSAGE::STR_DUEL_REQUEST_TO_PARTNER(targetPlayer.getName()));
}

// Java DuelService.java:140-144
void DuelService::rejectDuelRequest(model::gameobjects::player::Player& requester, model::gameobjects::player::Player& responder) {
	PacketSendUtility::sendPacket(requester, SM_CLOSE_QUESTION_WINDOW::STR_DUEL_HE_REJECT_DUEL(responder.getName()));
	PacketSendUtility::sendPacket(responder, SM_SYSTEM_MESSAGE::STR_DUEL_REJECT_DUEL(requester.getName()));
	requester.getResponseRequester().remove(SM_QUESTION_WINDOW::STR_DUEL_DO_YOU_WITHDRAW_REQUEST);
}

// Java DuelService.java:146-150
void DuelService::cancelDuelRequest(model::gameobjects::player::Player& canceller, model::gameobjects::player::Player& target) {
	PacketSendUtility::sendPacket(target, SM_CLOSE_QUESTION_WINDOW::STR_DUEL_REQUESTER_WITHDRAW_REQUEST(canceller.getName()));
	PacketSendUtility::sendPacket(canceller, SM_SYSTEM_MESSAGE::STR_DUEL_WITHDRAW_REQUEST(target.getName()));
	target.getResponseRequester().remove(SM_QUESTION_WINDOW::STR_DUEL_DO_YOU_ACCEPT_REQUEST);
}

// Java DuelService.java:160-171
void DuelService::startDuel(model::gameobjects::player::Player& requester, model::gameobjects::player::Player& responder) {
	if (requester.getResponseRequester().remove(SM_QUESTION_WINDOW::STR_DUEL_DO_YOU_WITHDRAW_REQUEST))
		PacketSendUtility::sendPacket(requester, SM_CLOSE_QUESTION_WINDOW::CLOSE_QUESTION_WINDOW());
	PacketSendUtility::sendPacket(requester, SM_DUEL::SM_DUEL_STARTED(responder.getObjectId()));
	PacketSendUtility::sendPacket(responder, SM_DUEL::SM_DUEL_STARTED(requester.getObjectId()));
	registerDuel(requester.getObjectId(), responder.getObjectId());
	createTask(requester, responder);
	if (requester.isInAnyHide())
		requester.getController().onHide();
	if (responder.isInAnyHide())
		responder.getController().onHide();
}

void DuelService::fixTeamVisibility(model::gameobjects::player::Player& hiddenDuelist) {
	// Java: DuelService.getInstance().getOpponentId(hiddenDuelist) - the singleton itself
	std::optional<int32_t> opponentId = DuelService::getInstance().getOpponentId(hiddenDuelist);
	if (opponentId) {
		runtime::Ptr<model::gameobjects::player::Player> opponent = world::World::getInstance().getPlayer(*opponentId);
		if (opponent && opponent->getKnownList().knows(hiddenDuelist) && !opponent->getKnownList().sees(hiddenDuelist)
			&& hiddenDuelist.isInSameTeam(*opponent))
			utils::PacketSendUtility::sendPacket(*opponent, network::aion::serverpackets::SM_DELETE(hiddenDuelist));
	}
}

// Java DuelService.java:189-198
void DuelService::loseDuel(model::gameobjects::player::Player& loser) {
	std::optional<int32_t> opponentId = getOpponentId(loser);
	if (!opponentId) // not dueling
		return;
	onDuelEnd(model::DuelResult::DUEL_LOST, loser, *opponentId); // Chain of Suffering must be ended before calling removeDuel
	runtime::Ptr<Player> winner = world::World::getInstance().getPlayer(*opponentId);
	if (winner != nullptr)
		onDuelEnd(model::DuelResult::DUEL_WON, *winner, loser.getObjectId()); // Chain of Suffering must be ended before calling removeDuel
	removeDuel(loser);
}

// Java DuelService.java:200-205
void DuelService::endDebuffsByOpponent(model::gameobjects::player::Player& player, int32_t opponentId) {
	for (const runtime::Ptr<skillengine::model::Effect>& effect : player.getEffectController()->getAbnormalEffects()) {
		if (effect->getTargetSlot() == skillengine::model::SkillTargetSlot::DEBUFF && effect->getEffectorId() == opponentId)
			effect->endEffect();
	}
}

// Java DuelService.java:207-215
void DuelService::cancelSummonedObjectAttacks(model::gameobjects::player::Player& target, int32_t summonerId) {
	target.getKnownList().forEachNpc([&target, summonerId](model::gameobjects::Npc& npc) {
		runtime::Ptr<Creature> master = npc.getMaster();
		if (master == nullptr) // Java: npc.getMaster().getObjectId() on null
			throw runtime::NullPointerException("Npc.getMaster()");
		if (master->getObjectId() == summonerId) {
			runtime::Ptr<skillengine::model::Skill> castingSkill = npc.getCastingSkill();
			if (castingSkill != nullptr && castingSkill->getFirstTarget() != nullptr && target.equals(*castingSkill->getFirstTarget()))
				npc.getController().cancelCurrentSkill(nullptr);
		}
	});
}

// Java DuelService.java:217-229. The draw task pins both players and the immortal service for its 5 minutes; removeDuel cancels it (a won,
// lost or logged-out duel: loseDuel), and the map that holds its Future belongs to the immortal service, which no player reaches, so it closes
// no cycle (fieldmap finds no edge for it)
void DuelService::createTask(model::gameobjects::player::Player& requester, model::gameobjects::player::Player& responder) {
	// Schedule for draw
	runtime::FutureRef task = utils::ThreadPoolManager::getInstance().schedule({this, &requester, &responder}, [this, &requester, &responder] {
		if (isDueling(requester, responder)) {
			onDuelEnd(model::DuelResult::DUEL_DRAW, requester, responder.getObjectId());
			onDuelEnd(model::DuelResult::DUEL_DRAW, responder, requester.getObjectId());
			removeDuel(requester);
		}
	}, 5, runtime::TimeUnit::MINUTES); // 5 minutes battle retail like

	drawTasks.put(requester.getObjectId(), task);
	drawTasks.put(responder.getObjectId(), task);
}

// Java DuelService.java:231-241
void DuelService::onDuelEnd(model::DuelResult duelResult, model::gameobjects::player::Player& player, int32_t opponentId) {
	if (player.isTargeting(opponentId))
		player.getController().cancelCurrentSkill(nullptr);
	endDebuffsByOpponent(player, opponentId);
	cancelSummonedObjectAttacks(player, opponentId);
	for (const runtime::Ptr<controllers::attack::AggroInfo>& info : player.getAggroList().stream()) {
		runtime::Ptr<Creature> attacker = info->getAttacker();
		runtime::Ptr<Creature> master = attacker->getMaster();
		if (master == nullptr) // Java: attacker.getMaster().getObjectId() on null
			throw runtime::NullPointerException("Creature.getMaster()");
		if (master->getObjectId() == opponentId)
			player.getAggroList().remove(*attacker, false);
	}
	// Java: PlayerService.getPlayerName(opponentId), null for an unknown id, written as an empty string by writeS
	PacketSendUtility::sendPacket(player, SM_DUEL::SM_DUEL_RESULT(duelResult, player::PlayerService::getPlayerName(opponentId).value_or("")));
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
