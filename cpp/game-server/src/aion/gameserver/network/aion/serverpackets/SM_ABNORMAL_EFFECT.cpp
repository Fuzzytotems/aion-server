#include "aion/gameserver/network/aion/serverpackets/SM_ABNORMAL_EFFECT.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ABNORMAL_EFFECT::SM_ABNORMAL_EFFECT(model::gameobjects::Creature& effectedValue) : AionServerPacket(opcodeOf<SM_ABNORMAL_EFFECT>) {
	AION_UNPORTED();
}

SM_ABNORMAL_EFFECT::SM_ABNORMAL_EFFECT(model::gameobjects::Creature& effectedValue, int32_t abnormalsValue,
	const std::vector<runtime::Ptr<skillengine::model::Effect>>& effects, int32_t slotsValue)
	: AionServerPacket(opcodeOf<SM_ABNORMAL_EFFECT>) {
	AION_UNPORTED();
}

SM_ABNORMAL_EFFECT::~SM_ABNORMAL_EFFECT() = default;

void SM_ABNORMAL_EFFECT::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
