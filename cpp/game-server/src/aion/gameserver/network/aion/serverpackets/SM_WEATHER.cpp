#include "aion/gameserver/network/aion/serverpackets/SM_WEATHER.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/templates/world/WeatherEntry.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_WEATHER::SM_WEATHER(std::span<const model::templates::world::WeatherEntry* const> weatherEntriesValue)
	: AionServerPacket(opcodeOf<SM_WEATHER>), weatherEntries(weatherEntriesValue.begin(), weatherEntriesValue.end()) {
}

void SM_WEATHER::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
