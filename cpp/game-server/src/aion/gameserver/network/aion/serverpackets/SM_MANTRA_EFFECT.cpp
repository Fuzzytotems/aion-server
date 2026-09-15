#include "aion/gameserver/network/aion/serverpackets/SM_MANTRA_EFFECT.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_MANTRA_EFFECT::SM_MANTRA_EFFECT(model::gameobjects::Creature& effectorValue, int32_t subEffectIdValue)
	: AionServerPacket(opcodeOf<SM_MANTRA_EFFECT>), effector(effectorValue), subEffectId(subEffectIdValue) {
}

SM_MANTRA_EFFECT::~SM_MANTRA_EFFECT() = default;

void SM_MANTRA_EFFECT::writeImpl(AionConnection* con) {
	writeD(0x00); // unk
	writeD(effector->getObjectId());
	writeH(subEffectId);
}

} // namespace aion::gameserver::network::aion::serverpackets
