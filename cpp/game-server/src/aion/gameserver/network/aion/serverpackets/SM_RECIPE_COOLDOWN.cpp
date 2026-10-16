#include "aion/gameserver/network/aion/serverpackets/SM_RECIPE_COOLDOWN.h"

#include "aion/gameserver/model/gameobjects/player/Cooldowns.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_RECIPE_COOLDOWN::SM_RECIPE_COOLDOWN(model::gameobjects::player::Player& player, int32_t modeValue)
	: AionServerPacket(opcodeOf<SM_RECIPE_COOLDOWN>), mode(modeValue) {
	runtime::Ptr<model::gameobjects::player::Cooldowns> craftCooldowns = player.getCraftCooldowns();
	if (!craftCooldowns->isEmpty()) {
		for (const auto& entry : craftCooldowns->snapshot()) // Java craftCooldowns.forEach((cooldownId, value) -> ...)
			cooldowns.insert_or_assign(entry.key, craftCooldowns->remainingSeconds(entry.key));
	}
}

void SM_RECIPE_COOLDOWN::writeImpl(AionConnection* con) {
	writeC(mode);
	writeH(static_cast<int32_t>(cooldowns.size()));
	for (const auto& [cooldownId, remainingSeconds] : detail::javaHashMapOrder(cooldowns)) { // Java HashMap.forEach
		writeD(cooldownId);
		writeD(remainingSeconds);
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
