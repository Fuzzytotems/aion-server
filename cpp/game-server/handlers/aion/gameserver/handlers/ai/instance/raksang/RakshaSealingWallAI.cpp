#include "aion/gameserver/handlers/ai/instance/raksang/RakshaSealingWallAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::raksang {

AION_AI(RakshaSealingWallAI, "raksha_sealing_wall");

// Java RakshaSealingWallAI.java:27-30
bool RakshaSealingWallAI::canThink() {
	return false;
}

// Java RakshaSealingWallAI.java:32-53
void RakshaSealingWallAI::handleCreatureMoved(Creature& creature) {
	if (runtime::Ptr<Player> player = runtime::as<Player>(runtime::Ptr<Creature>(creature))) {
		if (PositionUtil::getDistance(getOwner(), *player) <= 35) {
			if (startedEvent.compareAndSet(false, true)) {
				runtime::Ptr<WorldMapInstance> instance = getPosition()->getWorldMapInstance();
				runtime::Ptr<Npc> sharik = instance->getNpc(217425);
				runtime::Ptr<Npc> flamelord = instance->getNpc(217451);
				runtime::Ptr<Npc> sealguard = instance->getNpc(217456);
				int32_t bossId;
				if ((sharik == nullptr || sharik->isDead()) && (flamelord == nullptr || flamelord->isDead()) && (sealguard == nullptr || sealguard->isDead())) {
					bossId = 217475;
				} else {
					bossId = 217647;
				}
				spawn(bossId, 1063.08f, 903.13f, 138.744f, int8_t{29});
				AIActions::deleteOwner(*this);
			}
		}
	}
}

} // namespace aion::gameserver::handlers::ai::instance::raksang
