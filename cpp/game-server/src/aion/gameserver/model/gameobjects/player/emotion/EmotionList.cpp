#include "aion/gameserver/model/gameobjects/player/emotion/EmotionList.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/emotion/Emotion.h"

namespace aion::gameserver::model::gameobjects::player::emotion {

EmotionList::EmotionList(Player& ownerValue) : OwnedPart(ownerValue), owner(ownerValue) {
}

EmotionList::~EmotionList() = default;

void EmotionList::add(int32_t emotionId, int32_t dispearTime, bool isNew) {
	AION_UNPORTED();
}

void EmotionList::remove(int32_t emotionId) {
	AION_UNPORTED();
}

bool EmotionList::contains(int32_t emotionId) {
	AION_UNPORTED();
}

bool EmotionList::canUse(int32_t emotionId) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<Emotion>> EmotionList::getEmotions() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player::emotion
