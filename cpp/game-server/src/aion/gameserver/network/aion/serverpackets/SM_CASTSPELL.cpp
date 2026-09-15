#include "aion/gameserver/network/aion/serverpackets/SM_CASTSPELL.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

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
	writeD(effector->getObjectId());
	writeH(spellId);
	writeC(level);
	writeC(targetType);
	switch (targetType) {
		case 0:
		case 3:
		case 4:
			writeD(targetObjectId);
			break;
		case 1:
			writeF(x);
			writeF(y);
			writeF(z);
			break;
		case 2:
			writeF(x);
			writeF(y);
			writeF(z);
			writeD(0); // unk1
			writeD(0); // unk2
			writeD(0); // unk3
			writeD(0); // unk4
			writeD(0); // unk5
			writeD(0); // unk6
			writeD(0); // unk7
			writeD(0); // unk8
	}
	writeH(castDuration);
	writeC(0x00); // unk
	writeF(castSpeed);
	writeC(allowAnimationBoostByCastSpeed ? 1 : 0); // affects animation time of the next skill based on castSpeed (valid range: 0.5f - 1f)
}

} // namespace aion::gameserver::network::aion::serverpackets
