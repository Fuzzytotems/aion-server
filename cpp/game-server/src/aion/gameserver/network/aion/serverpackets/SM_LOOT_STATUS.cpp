#include "aion/gameserver/network/aion/serverpackets/SM_LOOT_STATUS.h"

#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"

namespace aion::gameserver::network::aion::serverpackets {

namespace {

/** Java: SM_LOOT_STATUS.Status.getId() - LOOT_ENABLE(0), LOOT_DISABLE(1), OPEN_DROP_LIST(2), CLOSE_DROP_LIST(3): equal to the ordinal */
int32_t statusId(SM_LOOT_STATUS::Status status) {
	return static_cast<int32_t>(status);
}

} // namespace

SM_LOOT_STATUS::SM_LOOT_STATUS(int32_t targetObjectIdValue, SM_LOOT_STATUS::Status statusValue)
	: AionServerPacket(opcodeOf<SM_LOOT_STATUS>), targetObjectId(targetObjectIdValue), status(statusValue) {
	lootEffectId = statusValue == Status::LOOT_ENABLE ? getLootEffect(targetObjectIdValue) : 0;
}

void SM_LOOT_STATUS::writeImpl(AionConnection* con) {
	writeD(targetObjectId);
	writeC(statusId(status));
	writeD(lootEffectId);
}

int32_t SM_LOOT_STATUS::getLootEffect(int32_t value) {
	// Java: items.stream().mapToInt(DropItem::getLootEffectId).filter(i -> i != 0).findAny().orElse(0) (a sequential stream: the first match)
	for (const runtime::Ptr<model::drop::DropItem>& item : detail::getCurrentDropItems(value)) {
		int32_t lootEffectIdValue = item->getLootEffectId();
		if (lootEffectIdValue != 0)
			return lootEffectIdValue;
	}
	return 0;
}

} // namespace aion::gameserver::network::aion::serverpackets
