#include "aion/gameserver/ai/AIActions.h"

#include <utility>

#include "aion/gameserver/ai/AIRequest.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/observer/DialogObserver.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/RespawnService.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::ai {

using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using model::gameobjects::player::RequestResponseHandler;
using network::aion::serverpackets::SM_QUESTION_WINDOW;
using utils::PacketSendUtility;

/**
 * Java: the anonymous RequestResponseHandler<Creature> of addRequest (AIActions$1, captures the request and the requestId). Its acceptRequest
 * adds the request id Java's AIRequest.acceptRequest takes as its third argument.
 */
class AIActions_RequestResponseHandler final : public RequestResponseHandler {
	AION_MAKE_REF_FRIEND
public:
	const runtime::Ref<AIRequest> request;
	const int32_t requestId;

protected:
	AIActions_RequestResponseHandler(Creature& requester, AIRequest& requestValue, int32_t requestIdValue)
		: RequestResponseHandler(requester), request(requestValue), requestId(requestIdValue) {}
	~AIActions_RequestResponseHandler() override = default;

public:
	static runtime::Ref<AIActions_RequestResponseHandler> create(Creature& requester, AIRequest& request, int32_t requestId) {
		return runtime::makeRef<AIActions_RequestResponseHandler>(requester, request, requestId);
	}

	// the parameters are renamed (MSVC C4458: they would hide the data members of RequestResponseHandler/DialogObserver; CONVENTIONS.md)
	void denyRequest(runtime::Ptr<Creature> value, Player& responder) override { request->denyRequest(value, responder); }

	void acceptRequest(runtime::Ptr<Creature> value, Player& responder) override { request->acceptRequest(value, responder, requestId); }
};

/** Java: the anonymous DialogObserver of addRequest (AIActions$2, captures the request); tooFar() denies it. */
class AIActions_DialogObserver final : public controllers::observer::DialogObserver {
	AION_MAKE_REF_FRIEND
public:
	const runtime::Ref<AIRequest> request;

protected:
	AIActions_DialogObserver(Creature& requester, Player& responder, int32_t maxDistance, AIRequest& requestValue)
		: DialogObserver(requester, responder, maxDistance), request(requestValue) {}
	~AIActions_DialogObserver() override = default;

public:
	static runtime::Ref<AIActions_DialogObserver> create(Creature& requester, Player& responder, int32_t maxDistance, AIRequest& request) {
		return runtime::makeRef<AIActions_DialogObserver>(requester, responder, maxDistance, request);
	}

	void tooFar() override { request->denyRequest(requester, *responder); }
};

void AIActions::deleteOwner(AbstractAI& ai) {
	ai.getOwner().getController().delete_();
}

void AIActions::die(AbstractAI& ai) {
	ai.getOwner().getController().die(ai.getOwner());
}

void AIActions::die(AbstractAI& ai, Creature& attacker) {
	ai.getOwner().getController().die(attacker);
}

void AIActions::useSkill(AbstractAI& ai, int32_t skillId, int32_t level) {
	ai.getOwner().getController().useSkill(skillId, level);
}

void AIActions::useSkill(AbstractAI& ai, int32_t skillId) {
	ai.getOwner().getController().useSkill(skillId);
}

void AIActions::targetSelf(AbstractAI& ai) {
	ai.getOwner().setTarget(runtime::Ptr<Creature>(ai.getOwner()));
}

void AIActions::targetCreature(AbstractAI& ai, Creature& target) {
	ai.getOwner().setTarget(runtime::Ptr<Creature>(target));
}

void AIActions::handleUseItemFinish(AbstractAI& ai, Player& player) {
	ai.getPosition()->getWorldMapInstance()->getInstanceHandler()->handleUseItemFinish(
		runtime::Ptr<Player>(player), *runtime::cast<Npc>(ai.getOwner()));
}

void AIActions::registerDrop(AbstractAI& ai, Player& player, const std::vector<runtime::Ptr<Player>>& registeredPlayers) {
	services::drop::DropRegistrationService::getInstance().registerDrop(*runtime::cast<Npc>(ai.getOwner()), player, registeredPlayers);
}

void AIActions::scheduleRespawn(AbstractAI& ai) {
	services::RespawnService::scheduleRespawn(ai.getOwner());
}

void AIActions::addRequest(AbstractAI& ai, Player& player, int32_t requestId, AIRequest& request, std::vector<std::string> requestParams) {
	addRequest(ai, player, requestId, 5, request, std::move(requestParams));
}

void AIActions::addRequest(AbstractAI& ai, Player& player, int32_t requestId, int32_t rangeOrCooldownSeconds, AIRequest& request,
	std::vector<std::string> requestParams) {
	bool requested = player.getResponseRequester().putRequest(requestId, AIActions_RequestResponseHandler::create(ai.getOwner(), request, requestId));

	if (requested) {
		if (rangeOrCooldownSeconds > 0) {
			player.getObserveController()->addObserver(
				*AIActions_DialogObserver::create(ai.getOwner(), player, rangeOrCooldownSeconds, request));
		}
		PacketSendUtility::sendPacket(player, SM_QUESTION_WINDOW(requestId, ai.getObjectId(), rangeOrCooldownSeconds, std::move(requestParams)));
	}
}

} // namespace aion::gameserver::ai
