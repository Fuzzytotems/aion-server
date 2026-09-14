#include "aion/gameserver/model/gameobjects/player/emotion/Emotion.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::gameobjects::player::emotion {

Emotion::Emotion(int32_t idValue, int32_t expireTimeValue) : id(idValue), expireTime(expireTimeValue) {
}

Emotion::~Emotion() = default;

runtime::Ref<Emotion> Emotion::create(int32_t idValue, int32_t expireTimeValue) {
	return runtime::makeRef<Emotion>(idValue, expireTimeValue);
}

void Emotion::onExpire(Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player::emotion
