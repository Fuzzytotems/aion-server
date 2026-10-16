#include "aion/gameserver/handlers/ai/instance/custom/eternalChallenge/CustomInstanceBulkyAI.h"

#include "aion/gameserver/custom/instance/RoahCustomInstanceHandler.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FORCED_MOVE.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge {

AION_AI(CustomInstanceBulkyAI, "custom_instance_bulky");

// Java CustomInstanceBulkyAI.java:25-39
AttackIntention CustomInstanceBulkyAI::chooseAttackIntention() {
	runtime::Ptr<world::WorldMapInstance> wmi = getPosition()->getWorldMapInstance();
	if (runtime::as<gameserver::custom::instance::RoahCustomInstanceHandler>(wmi->getInstanceHandler()) == nullptr)
		return AggressiveNoLootNpcAI::chooseAttackIntention();

	runtime::Ptr<VisibleObject> target = getTarget();
	if (!isDead() && target != nullptr) {
		if (!GeoService::getInstance().canSee(getOwner(), *target)) {
			world::World::getInstance().updatePosition(getOwner(), target->getX(), target->getY(), target->getZ(), int8_t{30});
			PacketSendUtility::broadcastPacketAndReceive(getOwner(), network::aion::serverpackets::SM_FORCED_MOVE(getOwner(), getOwner()));
		}
	}
	return AggressiveNoLootNpcAI::chooseAttackIntention();
}

} // namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge
