#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"

namespace aion::gameserver::model::gameobjects::player {

ResponseRequester::ResponseRequester(Player& playerValue) : OwnedPart(playerValue), player(playerValue) {
}

ResponseRequester::~ResponseRequester() = default;

bool ResponseRequester::putRequest(int32_t messageId, runtime::Ptr<RequestResponseHandler> handler) {
	if (!handler)
		return false;
	return !activeRequests.putIfAbsent(messageId, runtime::Ref<RequestResponseHandler>(handler));
}

bool ResponseRequester::respond(int32_t messageId, int32_t responseCode) {
	runtime::Ptr<RequestResponseHandler> handler = activeRequests.remove(messageId);
	if (handler) {
		handler->handle(player, responseCode);
		return true;
	}
	return false;
}

void ResponseRequester::denyAll() {
	for (const runtime::Ptr<RequestResponseHandler>& handler : activeRequests.values())
		handler->handle(player, 0);
	// java-race: a request put while the handlers run is cleared without being handled
	activeRequests.clear();
}

bool ResponseRequester::remove(int32_t messageId) {
	return static_cast<bool>(activeRequests.remove(messageId));
}

} // namespace aion::gameserver::model::gameobjects::player
