#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"

#include "aion/gameserver/model/gameobjects/Creature.h"

namespace aion::gameserver::model::gameobjects::player {

RequestResponseHandler::RequestResponseHandler(runtime::Ptr<Creature> requesterValue) : requester(requesterValue) {
}

RequestResponseHandler::~RequestResponseHandler() = default;

void RequestResponseHandler::handle(Player& responder, int32_t response) {
	if (response == 0)
		denyRequest(runtime::Ptr<Creature>(requester), responder);
	else
		acceptRequest(runtime::Ptr<Creature>(requester), responder);
}

void RequestResponseHandler::denyRequest(runtime::Ptr<Creature> value, Player& responder) {
	// Java: empty default
}

} // namespace aion::gameserver::model::gameobjects::player
