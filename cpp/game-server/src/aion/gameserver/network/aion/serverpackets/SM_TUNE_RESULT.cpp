#include "aion/gameserver/network/aion/serverpackets/SM_TUNE_RESULT.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/items/PendingTuneResult.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_TUNE_RESULT::SM_TUNE_RESULT(model::gameobjects::Item& targetItemValue, int32_t tuningScrollItemIdValue,
	model::items::PendingTuneResult& resultValue)
	: AionServerPacket(opcodeOf<SM_TUNE_RESULT>), targetItem(targetItemValue), tuningScrollItemId(tuningScrollItemIdValue), result(resultValue) {
	AION_UNPORTED();
}

SM_TUNE_RESULT::~SM_TUNE_RESULT() = default;

void SM_TUNE_RESULT::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
