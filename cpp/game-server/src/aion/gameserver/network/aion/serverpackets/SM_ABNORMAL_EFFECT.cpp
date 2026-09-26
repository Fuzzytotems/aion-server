#include "aion/gameserver/network/aion/serverpackets/SM_ABNORMAL_EFFECT.h"

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlot.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ABNORMAL_EFFECT::SM_ABNORMAL_EFFECT(model::gameobjects::Creature& effectedValue)
	: SM_ABNORMAL_EFFECT(effectedValue, effectedValue.getEffectController()->getAbnormals(), effectedValue.getEffectController()->getAbnormalEffects(),
		detail::SKILL_TARGET_SLOT_FULLSLOTS) {
}

SM_ABNORMAL_EFFECT::SM_ABNORMAL_EFFECT(model::gameobjects::Creature& effectedValue, int32_t abnormalsValue,
	const std::vector<runtime::Ptr<skillengine::model::Effect>>& effects, int32_t slotsValue)
	: AionServerPacket(opcodeOf<SM_ABNORMAL_EFFECT>), effected(effectedValue), abnormals(abnormalsValue), slots(slotsValue) {
	if (slotsValue == detail::SKILL_TARGET_SLOT_FULLSLOTS) {
		filtered.assign(effects.begin(), effects.end());
	} else {
		for (runtime::Ptr<skillengine::model::Effect> e : effects) {
			if ((slotsValue & detail::skillTargetSlotId(detail::requireTargetSlot(e->getTargetSlot()))) != 0)
				filtered.emplace_back(e);
		}
	}
	this->effectType = runtime::as<model::gameobjects::player::Player>(effectedValue) ? 2 : 1;
}

SM_ABNORMAL_EFFECT::~SM_ABNORMAL_EFFECT() = default;

void SM_ABNORMAL_EFFECT::writeImpl(AionConnection* con) {
	writeD(effected->getObjectId());
	writeC(effectType); // unk
	writeD(0); // TODO time
	writeD(abnormals); // unk
	writeD(0); // unk
	writeC(slots); // 4.5
	writeH(static_cast<int32_t>(filtered.size())); // effects size
	for (const runtime::Ref<skillengine::model::Effect>& effect : filtered) {
		switch (effectType) {
			case 2:
				writeD(effect->getEffectorId()); // fall-through on purpose
				[[fallthrough]];
			case 1:
				writeH(effect->getSkillId());
				writeC(effect->getSkillLevel());
				writeC(static_cast<int32_t>(detail::requireTargetSlot(effect->getTargetSlot())));
				writeD(effect->getRemainingTimeToDisplay());
				break;
			default:
				writeH(effect->getSkillId());
				writeC(effect->getSkillLevel());
		}
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
