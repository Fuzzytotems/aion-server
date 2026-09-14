#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION_LIST.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/gameobjects/player/emotion/Emotion.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_EMOTION_LIST::SM_EMOTION_LIST(int8_t actionValue, const std::vector<runtime::Ptr<model::gameobjects::player::emotion::Emotion>>& emotionsValue)
	: AionServerPacket(opcodeOf<SM_EMOTION_LIST>), action(actionValue), emotions(emotionsValue.begin(), emotionsValue.end()) {
}

SM_EMOTION_LIST::~SM_EMOTION_LIST() = default;

void SM_EMOTION_LIST::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
