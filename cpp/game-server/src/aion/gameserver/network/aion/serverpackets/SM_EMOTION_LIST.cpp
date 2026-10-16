#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION_LIST.h"

#include "aion/gameserver/model/gameobjects/player/emotion/Emotion.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_EMOTION_LIST::SM_EMOTION_LIST(int8_t actionValue, const std::vector<runtime::Ptr<model::gameobjects::player::emotion::Emotion>>& emotionsValue)
	: AionServerPacket(opcodeOf<SM_EMOTION_LIST>), action(actionValue), emotions(emotionsValue.begin(), emotionsValue.end()) {
}

SM_EMOTION_LIST::~SM_EMOTION_LIST() = default;

void SM_EMOTION_LIST::writeImpl(AionConnection* con) {
	writeC(action);
	writeH(static_cast<int32_t>(emotions.size()));
	for (const runtime::Ref<model::gameobjects::player::emotion::Emotion>& emotion : emotions) {
		writeD(emotion->getId());
		writeH(emotion->secondsUntilExpiration());
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
