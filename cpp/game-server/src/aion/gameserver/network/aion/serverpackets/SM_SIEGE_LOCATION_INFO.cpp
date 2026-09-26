#include "aion/gameserver/network/aion/serverpackets/SM_SIEGE_LOCATION_INFO.h"

#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionEmblem.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SIEGE_LOCATION_INFO::SM_SIEGE_LOCATION_INFO()
	: AionServerPacket(opcodeOf<SM_SIEGE_LOCATION_INFO>), infoType(0) {
	// Java stores the live map of SiegeService.getInstance().getSiegeLocations() and iterates it in writeImpl (a LinkedHashMap in XML order). The
	// C++ member is an unordered map, so writeImpl reads the service's entries itself for this constructor and `locations` stays empty.
}

SM_SIEGE_LOCATION_INFO::SM_SIEGE_LOCATION_INFO(model::siege::SiegeLocation& loc)
	: AionServerPacket(opcodeOf<SM_SIEGE_LOCATION_INFO>), infoType(1) {
	locations.insert_or_assign(loc.getLocationId(), runtime::Ref<model::siege::SiegeLocation>(loc));
}

SM_SIEGE_LOCATION_INFO::~SM_SIEGE_LOCATION_INFO() = default;

void SM_SIEGE_LOCATION_INFO::writeImpl(AionConnection* con) {
	runtime::Ptr<model::gameobjects::player::Player> player = detail::requireConnection(con, "SM_SIEGE_LOCATION_INFO").getActivePlayer();
	if (!configs::main::SiegeConfig::SIEGE_ENABLED.load()) {
		writeC(0);
		writeH(0);
		return;
	}
	// infoType 0: the live map of SiegeService (iteration order of the service); 1: the new HashMap with one location
	std::vector<std::pair<int32_t, runtime::Ptr<model::siege::SiegeLocation>>> entries;
	if (infoType == 0) {
		entries = detail::getSiegeLocations();
	} else {
		for (const auto& [id, location] : locations)
			entries.emplace_back(id, location);
	}
	writeC(infoType);
	writeH(static_cast<int32_t>(entries.size()));
	for (const auto& [id, loc] : entries) {
		runtime::Ref<model::team::legion::LegionEmblem> emblem = model::team::legion::LegionEmblem::create();
		int32_t legionId = loc->getLegionId();
		int32_t locId = loc->getLocationId();
		writeD(locId);
		writeD(legionId);
		if (legionId != 0 && detail::getLegion(legionId) != nullptr) // can be null if legion got deleted
			emblem = runtime::Ref<model::team::legion::LegionEmblem>(detail::getLegion(legionId)->getLegionEmblem());
		writeC(emblem->getEmblemId());
		writeC(detail::legionEmblemTypeValue(emblem->getEmblemType()));
		writeH(0);
		writeC(emblem->getColor_a());
		writeC(emblem->getColor_r());
		writeC(emblem->getColor_g());
		writeC(emblem->getColor_b());
		writeC(detail::siegeRaceId(loc->getRace()));
		writeC(loc->isVulnerable() ? 2 : 0); // is vulnerable (0 - no, 2 - yes)
		writeC(loc->isCanTeleport(player) ? 1 : 0);
		writeC(loc->getNextState()); // Next State (0 - invulnerable, 1 - vulnerable)
		writeH(0); // unk
		writeH(0);
		writeD(locId == 2111 || locId == 3111 ? detail::getRemainingSiegeTimeInSeconds(locId) : 0); // veille/masta timer
		writeD(configs::network::NetworkConfig::GAMESERVER_ID.load()); // server ID of the fortress owner (TODO relevant for panesterra)
		writeD(0); // unk 4.7 (some timestamp, maybe Capture Date?)
		writeD(loc->getOccupiedCount());
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
