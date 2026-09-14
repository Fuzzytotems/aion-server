#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_COOLDOWN.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/items/ItemCooldown.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ITEM_COOLDOWN::SM_ITEM_COOLDOWN(const std::unordered_map<int32_t, runtime::Ptr<model::items::ItemCooldown>>& cooldownsValue)
	: AionServerPacket(opcodeOf<SM_ITEM_COOLDOWN>), cooldowns(cooldownsValue.begin(), cooldownsValue.end()) {
}

SM_ITEM_COOLDOWN::~SM_ITEM_COOLDOWN() = default;

void SM_ITEM_COOLDOWN::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
