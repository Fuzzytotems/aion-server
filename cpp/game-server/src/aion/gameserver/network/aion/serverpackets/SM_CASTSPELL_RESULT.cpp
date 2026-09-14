#include "aion/gameserver/network/aion/serverpackets/SM_CASTSPELL_RESULT.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CASTSPELL_RESULT::SM_CASTSPELL_RESULT(skillengine::model::Skill& skillValue,
	const std::vector<runtime::Ptr<skillengine::model::Effect>>& effectsValue, int32_t hitTimeValue, bool chainSuccessValue, int32_t dashStatusValue)
	: AionServerPacket(opcodeOf<SM_CASTSPELL_RESULT>) {
	AION_UNPORTED();
}

SM_CASTSPELL_RESULT::SM_CASTSPELL_RESULT(skillengine::model::Skill& skillValue,
	const std::vector<runtime::Ptr<skillengine::model::Effect>>& effectsValue, int32_t hitTimeValue, bool chainSuccessValue, int32_t dashStatusValue,
	int32_t targetTypeValue)
	: SM_CASTSPELL_RESULT(skillValue, effectsValue, hitTimeValue, chainSuccessValue, dashStatusValue) {
	this->targetType = targetTypeValue;
}

SM_CASTSPELL_RESULT::~SM_CASTSPELL_RESULT() = default;

void SM_CASTSPELL_RESULT::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
