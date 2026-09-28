#include "aion/gameserver/model/gameobjects/player/emotion/EmotionList.h"

#include <vector>

#include "aion/gameserver/dao/PlayerEmotionListDAO.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/emotion/Emotion.h"
#include "aion/gameserver/model/templates/item/actions/EmotionLearnAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION_LIST.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::gameobjects::player::emotion {

namespace {

/** Java ExpireTimerTask.getInstance().registerExpirable(expirable, player); the task manager (P5-14) has no C++ header yet */
void registerExpirable(Emotion& expirable, Player& player) {
	static_cast<void>(expirable);
	static_cast<void>(player);
	AION_UNPORTED();
}

} // namespace

EmotionList::EmotionList(Player& ownerValue) : OwnedPart(ownerValue), owner(ownerValue) {
}

EmotionList::~EmotionList() = default;

void EmotionList::add(int32_t emotionId, int32_t dispearTime, bool isNew) {
	if (!emotions)
		emotions.set(runtime::RcLinkedHashMap<int32_t, runtime::Ref<Emotion>>::create(AION_LOCK_CLASS(EmotionList::emotions)));

	runtime::Ref<Emotion> emotion = Emotion::create(emotionId, dispearTime);
	emotions->put(emotionId, emotion);

	if (isNew) {
		registerExpirable(*emotion, owner);
		dao::PlayerEmotionListDAO::insertEmotion(owner, *emotion);
		utils::PacketSendUtility::sendPacket(owner,
			network::aion::serverpackets::SM_EMOTION_LIST(static_cast<int8_t>(1), std::vector<runtime::Ptr<Emotion>>{emotion}));
	}
}

void EmotionList::remove(int32_t emotionId) {
	emotions->remove(emotionId);
	dao::PlayerEmotionListDAO::deleteEmotion(owner.getObjectId(), emotionId);
	utils::PacketSendUtility::sendPacket(owner, network::aion::serverpackets::SM_EMOTION_LIST(static_cast<int8_t>(0), getEmotions()));
}

bool EmotionList::contains(int32_t emotionId) {
	runtime::Ptr<runtime::RcLinkedHashMap<int32_t, runtime::Ref<Emotion>>> map = emotions.get();
	return map && map->containsKey(emotionId);
}

bool EmotionList::canUse(int32_t emotionId) {
	return !templates::item::actions::EmotionLearnAction::isLearnable(emotionId) || contains(emotionId);
}

std::vector<runtime::Ptr<Emotion>> EmotionList::getEmotions() {
	runtime::Ptr<runtime::RcLinkedHashMap<int32_t, runtime::Ref<Emotion>>> map = emotions.get();
	if (!map)
		return {};
	return map->values();
}

} // namespace aion::gameserver::model::gameobjects::player::emotion
