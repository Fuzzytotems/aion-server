#include "aion/gameserver/handlers/ai/instance/custom/eternalChallenge/CustomInstanceReianTeleportShouter.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/custom/instance/CustomInstanceService.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge {

/** Java: private static final Map<Integer, Long> lastShoutToPlayer = new ConcurrentHashMap<>() */
runtime::ConcurrentHashMap<int32_t, int64_t>& CustomInstanceReianTeleportShouter::lastShoutToPlayer() {
	static auto* const map = new runtime::ConcurrentHashMap<int32_t, int64_t>(AION_LOCK_CLASS(CustomInstanceReianTeleportShouter::lastShoutToPlayer#stripe));
	return *map;
}

AION_AI(CustomInstanceReianTeleportShouter, "custom_instance_reian_teleport_shouter");


// Java CustomInstanceReianTeleportShouter.java:27-43
void CustomInstanceReianTeleportShouter::handleCreatureMoved(Creature& creature) {
	runtime::Ptr<Player> player = runtime::as<Player>(creature);
	if (player == nullptr || creature.isDead() || !getOwner().canSee(runtime::Ptr<VisibleObject>(creature)) || !getOwner().getPosition()->isMapRegionActive())
		return;
	if (lastShoutToPlayer().get(creature.getObjectId())
		&& commons::utils::currentTimeMillis() - *lastShoutToPlayer().get(creature.getObjectId()) <= 1800000) // only allow 1 shout half an hour
		return;

	if (PositionUtil::isInRange(getOwner(), creature, 15) && GeoService::getInstance().canSee(getOwner(), creature)) {
		lastShoutToPlayer().put(creature.getObjectId(), commons::utils::currentTimeMillis());
		if (gameserver::custom::instance::CustomInstanceService::getInstance().canEnter(creature.getObjectId()))
			PacketSendUtility::sendPacket(*player,
				SM_MESSAGE(getOwner(), "Hey you know, use this device to teleport into the 'Eternal Challenge'.", ChatType::NPC));
		else
			PacketSendUtility::sendPacket(*player, SM_MESSAGE(getOwner(),
				"I see... you already have shown your potential today. Come again tomorrow if you want to improve.", ChatType::NPC));
	}
}

} // namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge
