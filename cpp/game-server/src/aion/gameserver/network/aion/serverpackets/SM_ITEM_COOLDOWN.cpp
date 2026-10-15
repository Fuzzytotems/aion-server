#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_COOLDOWN.h"

#include <algorithm>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/items/ItemCooldown.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ITEM_COOLDOWN::SM_ITEM_COOLDOWN(const std::unordered_map<int32_t, runtime::Ptr<model::items::ItemCooldown>>& cooldownsValue)
	: AionServerPacket(opcodeOf<SM_ITEM_COOLDOWN>), cooldowns(cooldownsValue.begin(), cooldownsValue.end()) {
}

SM_ITEM_COOLDOWN::~SM_ITEM_COOLDOWN() = default;

void SM_ITEM_COOLDOWN::writeImpl(AionConnection* con) {
	writeH(static_cast<int32_t>(cooldowns.size()));
	int64_t currentTime = commons::utils::currentTimeMillis();
	// C++: the member is an unordered_map (frozen header); entries are written in ascending item cooldown group id for a deterministic order. Java
	// writes the player's ConcurrentHashMap in its bucket order, which differs for most real group ids; the client matches the entries by id
	// (docs/deviations/P4-16.md)
	std::vector<int32_t> keys;
	keys.reserve(cooldowns.size());
	for (const auto& entry : cooldowns)
		keys.push_back(entry.first);
	std::ranges::sort(keys);
	for (int32_t key : keys) {
		const runtime::Ref<model::items::ItemCooldown>& cooldown = cooldowns.at(key);
		writeH(key);
		int32_t left = static_cast<int32_t>((cooldown->getReuseTime() - currentTime) / 1000);
		writeD(left > 0 ? left : 0);
		writeD(cooldown->getUseDelay());
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
