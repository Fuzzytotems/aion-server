#include "aion/gameserver/network/aion/serverpackets/SM_ABNORMAL_STATE.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlot.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ABNORMAL_STATE::SM_ABNORMAL_STATE(const std::vector<runtime::Ptr<skillengine::model::Effect>>& effectsValue, int32_t abnormalsValue,
	int32_t slotValue)
	: AionServerPacket(opcodeOf<SM_ABNORMAL_STATE>), effects(effectsValue.begin(), effectsValue.end()), abnormals(abnormalsValue), slot(slotValue) {
}

SM_ABNORMAL_STATE::~SM_ABNORMAL_STATE() = default;

void SM_ABNORMAL_STATE::writeImpl(AionConnection* con) {
	writeD(abnormals);
	writeD(0);
	writeD(0); // 4.5
	writeC(slot);
	writeH(static_cast<int32_t>(effects.size()));
	for (const runtime::Ref<skillengine::model::Effect>& effect : effects) {
		writeD(effect->getEffectorId());
		writeH(effect->getSkillId());
		writeC(effect->getSkillLevel());
		writeC(static_cast<int32_t>(detail::requireTargetSlot(effect->getTargetSlot())));
		writeD(effect->getRemainingTimeToDisplay());
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
