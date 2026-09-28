#include "aion/gameserver/network/aion/serverpackets/SM_TUNE_RESULT.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/items/PendingTuneResult.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/iteminfo/EnchantInfoBlobEntry.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_TUNE_RESULT::SM_TUNE_RESULT(model::gameobjects::Item& targetItemValue, int32_t tuningScrollItemIdValue,
	model::items::PendingTuneResult& resultValue)
	: AionServerPacket(opcodeOf<SM_TUNE_RESULT>), targetItem(targetItemValue), tuningScrollItemId(tuningScrollItemIdValue), result(resultValue) {
	tuneCancelPossible = showManastoneSlots = !resultValue.isAttributeOnly();
}

SM_TUNE_RESULT::~SM_TUNE_RESULT() = default;

void SM_TUNE_RESULT::writeImpl(AionConnection* con) {
	writeD(targetItem->getObjectId());
	writeD(tuningScrollItemId);
	writeC(result->getStatBonusId());
	iteminfo::EnchantInfoBlobEntry::writeInfo(getBuf(), *targetItem, result->getOptionalSockets(), result->getEnchantBonus());
	writeC(showManastoneSlots ? 0 : 1);
	writeC(tuneCancelPossible ? 0 : 1);
}

} // namespace aion::gameserver::network::aion::serverpackets
