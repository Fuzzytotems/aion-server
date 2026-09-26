#include "aion/gameserver/network/aion/serverpackets/SM_SHOW_BRAND.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SHOW_BRAND::SM_SHOW_BRAND(int32_t iconId, int32_t targetObjectId)
	: AionServerPacket(opcodeOf<SM_SHOW_BRAND>) {
	targetIdsByIconId.insert_or_assign(iconId, targetObjectId);
}

SM_SHOW_BRAND::SM_SHOW_BRAND(const std::unordered_map<int32_t, int32_t>& targetIdsByIconIdValue)
	: AionServerPacket(opcodeOf<SM_SHOW_BRAND>) {
	if (targetIdsByIconIdValue.empty()) {
		for (int32_t brandId = 0; brandId < 16; brandId++) // Java IntStream.range(0, 16): reset all brands
			targetIdsByIconId.insert_or_assign(brandId, 0);
	} else {
		targetIdsByIconId.insert(targetIdsByIconIdValue.begin(), targetIdsByIconIdValue.end());
	}
}

void SM_SHOW_BRAND::writeImpl(AionConnection* con) {
	writeH(static_cast<int32_t>(targetIdsByIconId.size()));
	for (const auto& [iconId, targetObjectId] : targetIdsByIconId) {
		writeD(1); // 0 = solo?, 1 = group/alliance?, 2 = league? - doesn't seem to make any difference
		writeD(iconId);
		writeD(targetObjectId); // 0 = remove icon
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
