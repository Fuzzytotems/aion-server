#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_CANCEL.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SKILL_CANCEL::SM_SKILL_CANCEL(model::gameobjects::Creature& creatureValue, int32_t skillIdValue)
	: AionServerPacket(opcodeOf<SM_SKILL_CANCEL>), creature(creatureValue), skillId(skillIdValue) {
}

SM_SKILL_CANCEL::~SM_SKILL_CANCEL() = default;

void SM_SKILL_CANCEL::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
