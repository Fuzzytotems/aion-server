#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOGInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPEInfo.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ATTACK_STATUS::SM_ATTACK_STATUS(model::gameobjects::Creature& creatureValue, SM_ATTACK_STATUS::TYPE typeValue, int32_t skillIdValue,
	int32_t valueValue, SM_ATTACK_STATUS::LOG log, bool criticalHitValue)
	: SM_ATTACK_STATUS(creatureValue, typeValue, skillIdValue, valueValue, log) {
	this->criticalHit = criticalHitValue;
}

SM_ATTACK_STATUS::SM_ATTACK_STATUS(model::gameobjects::Creature& creatureValue, SM_ATTACK_STATUS::TYPE typeValue, int32_t skillIdValue,
	int32_t valueValue, SM_ATTACK_STATUS::LOG log)
	: AionServerPacket(opcodeOf<SM_ATTACK_STATUS>), creature(creatureValue), type(typeValue), skillId(skillIdValue), value(valueValue),
	  logId(getValue(log)) {
}

SM_ATTACK_STATUS::SM_ATTACK_STATUS(model::gameobjects::Creature& creatureValue, SM_ATTACK_STATUS::TYPE typeValue, int32_t skillIdValue,
	int32_t valueValue)
	: SM_ATTACK_STATUS(creatureValue, typeValue, skillIdValue, valueValue, SM_ATTACK_STATUS::LOG::REGULAR) {
}

SM_ATTACK_STATUS::SM_ATTACK_STATUS(model::gameobjects::Creature& creatureValue, int32_t valueValue)
	: SM_ATTACK_STATUS(creatureValue, SM_ATTACK_STATUS::TYPE::REGULAR, 0, valueValue, SM_ATTACK_STATUS::LOG::REGULAR) {
}

SM_ATTACK_STATUS::~SM_ATTACK_STATUS() = default;

void SM_ATTACK_STATUS::writeImpl(AionConnection* con) {
	int32_t hpOrMp;
	writeD(creature->getObjectId());
	switch (type) {
		case TYPE::DAMAGE:
		case TYPE::DELAYDAMAGE:
		case TYPE::FALL_DAMAGE:
		case TYPE::FP_DAMAGE:
		case TYPE::MAGICCOUNTERATK:
		case TYPE::DISPELBUFFCOUNTERATK:
		case TYPE::USED_HP:
		case TYPE::DROWNING:
			writeD(-value);
			hpOrMp = creature->getLifeStats()->getHpPercentage();
			break;
		case TYPE::USED_MP:
		case TYPE::DAMAGE_MP:
			writeD(-value);
			hpOrMp = creature->getLifeStats()->getMpPercentage();
			break;
		case TYPE::MP:
		case TYPE::NATURAL_MP:
		case TYPE::HEAL_MP:
		case TYPE::ABSORBED_MP:
			writeD(value);
			hpOrMp = creature->getLifeStats()->getMpPercentage();
			break;
		default:
			writeD(value);
			hpOrMp = creature->getLifeStats()->getHpPercentage();
	}
	writeC(getValue(type));
	writeC(hpOrMp);
	writeH(skillId);
	writeC(logId);
	writeC(criticalHit ? CRITICAL_DISPLAY_CODE : 0);
}

} // namespace aion::gameserver::network::aion::serverpackets
