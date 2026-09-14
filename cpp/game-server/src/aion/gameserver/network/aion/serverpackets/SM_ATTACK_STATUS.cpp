#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/gameobjects/Creature.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ATTACK_STATUS::SM_ATTACK_STATUS(model::gameobjects::Creature& creatureValue, SM_ATTACK_STATUS::TYPE typeValue, int32_t skillIdValue,
	int32_t valueValue, SM_ATTACK_STATUS::LOG log, bool criticalHitValue)
	: SM_ATTACK_STATUS(creatureValue, typeValue, skillIdValue, valueValue, log) {
	this->criticalHit = criticalHitValue;
}

SM_ATTACK_STATUS::SM_ATTACK_STATUS(model::gameobjects::Creature& creatureValue, SM_ATTACK_STATUS::TYPE typeValue, int32_t skillIdValue,
	int32_t valueValue, SM_ATTACK_STATUS::LOG log)
	: AionServerPacket(opcodeOf<SM_ATTACK_STATUS>) {
	AION_UNPORTED();
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
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
