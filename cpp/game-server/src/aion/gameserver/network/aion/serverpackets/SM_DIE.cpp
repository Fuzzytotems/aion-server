#include "aion/gameserver/network/aion/serverpackets/SM_DIE.h"

#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Kisk.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapType.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_DIE::SM_DIE(model::gameobjects::player::Player& player) : AionServerPacket(opcodeOf<SM_DIE>) {
	runtime::Ptr<instance::handlers::InstanceHandler> instanceHandler = player.getWorldMapInstance()->getInstanceHandler();
	allowReviveBySkill = instanceHandler->allowSelfReviveBySkill() && player.canUseRebirthRevive();
	allowReviveByItem = instanceHandler->allowSelfReviveByItem() && player.haveSelfRezItem();
	remainingKiskTimeSeconds = instanceHandler->allowKiskRevive() && player.getKisk() ? player.getKisk()->getRemainingLifetime() : 0;
	allowInstanceRevive = instanceHandler->allowInstanceRevive();
	invasion = player.getWorldId() == world::getId(getInvasionWorld(player));
}

void SM_DIE::writeImpl(AionConnection* con) {
	writeC(allowReviveBySkill ? 1 : 0);
	writeC(allowReviveByItem ? 1 : 0);
	writeD(remainingKiskTimeSeconds);
	// select between obelisk and instance revive (0 = ReviveType.BIND_REVIVE, else ReviveType.INSTANCE_REVIVE)
	writeC(allowInstanceRevive ? 1 : 0);
	writeC(invasion ? 0x80 : 0x00);
}

world::WorldMapType SM_DIE::getInvasionWorld(model::gameobjects::player::Player& player) {
	return player.getRace() == model::Race::ASMODIANS ? world::WorldMapType::THEOBOMOS : world::WorldMapType::BRUSTHONIN;
}

} // namespace aion::gameserver::network::aion::serverpackets
