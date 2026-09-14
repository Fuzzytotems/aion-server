#include "aion/gameserver/model/Expirable.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model {

int32_t Expirable::secondsUntilExpiration() {
	AION_UNPORTED();
}

bool Expirable::isExpired() {
	AION_UNPORTED();
}

void Expirable::onBeforeExpire(gameobjects::player::Player&, int32_t) {
}

bool Expirable::canExpireNow() {
	return true;
}

} // namespace aion::gameserver::model
