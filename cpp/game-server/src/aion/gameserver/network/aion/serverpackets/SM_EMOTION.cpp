#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_EMOTION::SM_EMOTION(model::gameobjects::Creature& creature, model::EmotionType emotionTypeValue) : SM_EMOTION(creature, emotionTypeValue, 0, 0) {
}

SM_EMOTION::SM_EMOTION(model::gameobjects::Creature& creature, model::EmotionType emotionTypeValue, int32_t emotionValue, int32_t targetObjectIdValue)
	: AionServerPacket(opcodeOf<SM_EMOTION>) {
	AION_UNPORTED();
}

SM_EMOTION::SM_EMOTION(int32_t Objid, model::EmotionType emotionTypeValue, int32_t stateValue)
	: AionServerPacket(opcodeOf<SM_EMOTION>), senderObjectId(Objid), emotionType(emotionTypeValue), state(stateValue) {
}

SM_EMOTION::SM_EMOTION(model::gameobjects::player::Player& player, model::EmotionType emotionTypeValue, int32_t emotionValue, float xValue,
	float yValue, float zValue, int8_t headingValue, int32_t targetObjectIdValue)
	: AionServerPacket(opcodeOf<SM_EMOTION>) {
	AION_UNPORTED();
}

void SM_EMOTION::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
