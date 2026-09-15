#include "aion/gameserver/model/Expirable.h"

#include "aion/commons/utils/TimeUtils.h"

namespace aion::gameserver::model {

int32_t Expirable::secondsUntilExpiration() {
	// Java: getExpireTime() - (int) (System.currentTimeMillis() / 1000) with int wrap-around
	return getExpireTime() == 0 ? 0
								: static_cast<int32_t>(static_cast<uint32_t>(getExpireTime()) -
									  static_cast<uint32_t>(static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000)));
}

bool Expirable::isExpired() {
	return secondsUntilExpiration() < 0;
}

void Expirable::onBeforeExpire(gameobjects::player::Player&, int32_t) {
}

bool Expirable::canExpireNow() {
	return true;
}

} // namespace aion::gameserver::model
