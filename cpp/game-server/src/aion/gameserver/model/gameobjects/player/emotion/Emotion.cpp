#include "aion/gameserver/model/gameobjects/player/emotion/Emotion.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/emotion/EmotionList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::gameobjects::player::emotion {

Emotion::Emotion(int32_t idValue, int32_t expireTimeValue) : id(idValue), expireTime(expireTimeValue) {
}

Emotion::~Emotion() = default;

runtime::Ref<Emotion> Emotion::create(int32_t idValue, int32_t expireTimeValue) {
	return runtime::makeRef<Emotion>(idValue, expireTimeValue);
}

void Emotion::onExpire(Player& player) {
	player.getEmotions()->remove(id);
	// TODO emotion templates -> parse nameIds for system message, like 600228 for STR_EMOTION_CASH_DISCODANCE (Aion Boogie) etc.
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_DELETE_CASH_SOCIALACTION_BY_TIMEOUT(/* nameId */));
}

} // namespace aion::gameserver::model::gameobjects::player::emotion
