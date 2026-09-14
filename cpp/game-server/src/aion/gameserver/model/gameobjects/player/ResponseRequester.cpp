#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"

namespace aion::gameserver::model::gameobjects::player {

ResponseRequester::ResponseRequester(Player& playerValue) : OwnedPart(playerValue), player(playerValue) {
}

ResponseRequester::~ResponseRequester() = default;

bool ResponseRequester::putRequest(int32_t messageId, runtime::Ptr<RequestResponseHandler> handler) {
	AION_UNPORTED();
}

bool ResponseRequester::respond(int32_t messageId, int32_t responseCode) {
	AION_UNPORTED();
}

void ResponseRequester::denyAll() {
	AION_UNPORTED();
}

bool ResponseRequester::remove(int32_t messageId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player
