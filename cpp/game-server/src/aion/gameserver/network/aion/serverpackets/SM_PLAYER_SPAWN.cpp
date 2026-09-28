#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_SPAWN.h"

#include <optional>
#include <string>

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PLAYER_SPAWN::SM_PLAYER_SPAWN(model::gameobjects::player::Player& playerValue)
	: AionServerPacket(opcodeOf<SM_PLAYER_SPAWN>), player(playerValue) {
}

SM_PLAYER_SPAWN::~SM_PLAYER_SPAWN() = default;

void SM_PLAYER_SPAWN::writeImpl(AionConnection* con) {
	std::optional<world::WorldMapType> worldMapType = world::getWorldMapType(player->getWorldId());
	if (!worldMapType) // Java: WorldMapType.getWorld(worldId).isPersonal() on null
		throw runtime::NullPointerException("SM_PLAYER_SPAWN: no WorldMapType for world " + std::to_string(player->getWorldId()));
	bool isPersonal = world::isPersonal(*worldMapType);
	int32_t worldChannel = player->getWorldId() + player->getInstanceId() - 1;
	writeD(isPersonal ? -worldChannel : worldChannel); // world + chnl
	writeD(player->getWorldId());
	writeD(0x00); // unk
	writeC(isPersonal ? 1 : 0);
	writeF(player->getX()); // x
	writeF(player->getY()); // y
	writeF(player->getZ()); // z
	writeC(player->getHeading()); // heading
	writeD(0); // new 2.5
	writeD(0); // new 2.5
	// 1 - Azphels Curse, too much low level player killed (no longer implemented in client, since removal of serial killer system in 4.8)
	// 2 - Victorys Pledge, boost attack 15, magic boost by 95, max hp by 440, healing boost by 30
	// 3 - Victorys Pledge, boost attack 20, magic boost by 105, max hp by 520, healing boost by 30
	// 4 - Victorys Pledge, boost attack 20, magic boost by 115, max hp by 600, healing boost by 30
	// 5 - Victorys Pledge, boost attack 25, magic boost by 125, max hp by 680, healing boost by 50
	// 6 - Victorys Pledge, boost attack 25, magic boost by 125, max hp by 680, healing boost by 50
	// 7 - Boost Moral, [Arena] boost pvp physical and magical defense by 90%, movement speed by 50%, magical resist by 9999, all altered state
	//     resist by 1000
	// 8 - Boost Moral, [Arena] same stats like 7
	// 9 - I never lose, boost pvp physical/magical attack/defense by 10%
	// 10 - Boost Moral, [Kamar Battlefield] same stats like 7, except 100% pvp defense
	// 11 - Boost Moral, [Engulfed Ophidan Bridge] same stats like 7, except 100% pvp defense
	// 12 - Boost Moral, [Iron Wall War Front] same stats like 7, except 100% pvp defense
	writeD(0);
	if (world::World::getInstance().getWorldMap(player->getWorldId())->getTemplate()->getBeginnerTwinCount() > 0)
		writeC(1);
	else
		writeC(0);
	writeD(0); // 4.0
	writeC(0); // 4.7
}

} // namespace aion::gameserver::network::aion::serverpackets
