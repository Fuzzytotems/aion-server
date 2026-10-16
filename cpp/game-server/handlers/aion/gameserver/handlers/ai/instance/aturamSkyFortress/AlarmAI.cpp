#include "aion/gameserver/handlers/ai/instance/aturamSkyFortress/AlarmAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress {

AION_AI(AlarmAI, "alarm");

// Java AlarmAI.java:34-37
bool AlarmAI::canThink() {
	return canThink_.get();
}

// Java AlarmAI.java:39-63
void AlarmAI::handleCreatureMoved(Creature& creature) {
	if (runtime::Ptr<Player> player = runtime::as<Player>(runtime::Ptr<Creature>(creature))) {
		if (PositionUtil::getDistance(getOwner(), *player) <= 23) {
			if (startedEvent.compareAndSet(false, true)) {
				canThink_ = false;
				PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1500380);
				PacketSendUtility::broadcastPacket(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_Station_DoorCtrl_Evileye());
				getSpawnTemplate()->setWalkerId(std::string_view("3002400002"));
				gameserver::ai::manager::WalkManager::startWalking(*this);
				getOwner().setState(CreatureState::ACTIVE, true);
				PacketSendUtility::broadcastPacket(getOwner(),
					network::aion::serverpackets::SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getObjectId()));
				// both doors are inverted, they visually close by opening.
				// they also have no collision but there are invisible walls anyway, as you are never supposed to enter the rooms behind them
				getPosition()->getWorldMapInstance()->setDoorState(128, true);
				getPosition()->getWorldMapInstance()->setDoorState(138, true);
				ThreadPoolManager::getInstance().schedule({this}, [this] {
					if (!isDead()) {
						despawn();
					}
				}, 3000);
			}
		}
	}
}

// Java AlarmAI.java:65-67
void AlarmAI::despawn() {
	AIActions::deleteOwner(*this);
}

} // namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress
