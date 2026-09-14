#include "aion/gameserver/network/aion/serverpackets/SM_ABNORMAL_STATE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ABNORMAL_STATE::SM_ABNORMAL_STATE(const std::vector<runtime::Ptr<skillengine::model::Effect>>& effectsValue, int32_t abnormalsValue,
	int32_t slotValue)
	: AionServerPacket(opcodeOf<SM_ABNORMAL_STATE>), effects(effectsValue.begin(), effectsValue.end()), abnormals(abnormalsValue), slot(slotValue) {
}

SM_ABNORMAL_STATE::~SM_ABNORMAL_STATE() = default;

void SM_ABNORMAL_STATE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
