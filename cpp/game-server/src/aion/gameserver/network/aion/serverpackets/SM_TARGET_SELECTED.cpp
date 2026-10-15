#include "aion/gameserver/network/aion/serverpackets/SM_TARGET_SELECTED.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_TARGET_SELECTED::SM_TARGET_SELECTED(runtime::Ptr<model::gameobjects::VisibleObject> target)
	: AionServerPacket(opcodeOf<SM_TARGET_SELECTED>) {
	if (target != nullptr) {
		targetObjId = target->getObjectId();
		if (runtime::Ptr<model::gameobjects::Creature> creature = runtime::as<model::gameobjects::Creature>(target)) {
			level = creature->getLevel();
			maxHp = creature->getLifeStats()->getMaxHp();
			currentHp = creature->getLifeStats()->getCurrentHp();
			maxMp = creature->getLifeStats()->getMaxMp();
			currentMp = creature->getLifeStats()->getCurrentMp();
		}
	}
}

void SM_TARGET_SELECTED::writeImpl(AionConnection* con) {
	writeD(targetObjId);
	writeH(level);
	writeD(maxHp);
	writeD(currentHp);
	writeD(maxMp); // new 4.0
	writeD(currentMp); // new 4.0
}

} // namespace aion::gameserver::network::aion::serverpackets
