#include "aion/gameserver/network/aion/serverpackets/SM_CASTSPELL.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/gameobjects/Creature.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CASTSPELL::SM_CASTSPELL(model::gameobjects::Creature& effectorValue, int32_t spellIdValue, int32_t levelValue, int32_t targetTypeValue,
	int32_t targetObjectIdValue, int32_t castDurationValue, float castSpeedValue, bool allowAnimationBoostByCastSpeedValue)
	: AionServerPacket(opcodeOf<SM_CASTSPELL>), effector(effectorValue), spellId(spellIdValue), level(levelValue), targetType(targetTypeValue),
	  targetObjectId(targetObjectIdValue), castDuration(castDurationValue), castSpeed(castSpeedValue),
	  allowAnimationBoostByCastSpeed(allowAnimationBoostByCastSpeedValue) {
}

SM_CASTSPELL::SM_CASTSPELL(model::gameobjects::Creature& effectorValue, int32_t spellIdValue, int32_t levelValue, int32_t targetTypeValue,
	float xValue, float yValue, float zValue, int32_t castDurationValue, float castSpeedValue, bool allowAnimationBoostByCastSpeedValue)
	: SM_CASTSPELL(effectorValue, spellIdValue, levelValue, targetTypeValue, 0, castDurationValue, castSpeedValue,
		allowAnimationBoostByCastSpeedValue) {
	this->x = xValue;
	this->y = yValue;
	this->z = zValue;
}

SM_CASTSPELL::~SM_CASTSPELL() = default;

void SM_CASTSPELL::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
