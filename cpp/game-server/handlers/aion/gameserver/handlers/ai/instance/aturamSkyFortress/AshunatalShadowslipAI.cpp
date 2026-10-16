#include "aion/gameserver/handlers/ai/instance/aturamSkyFortress/AshunatalShadowslipAI.h"

#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress {

AION_AI(AshunatalShadowslipAI, "ashunatal_shadowslip");

// Java AshunatalShadowslipAI.java:31-34
bool AshunatalShadowslipAI::canThink() {
	return canThink_.get();
}

// Java AshunatalShadowslipAI.java:36-42
void AshunatalShadowslipAI::handleCreatureAggro(Creature& creature) {
	AggressiveNpcAI::handleCreatureAggro(creature);
	if (isHome.compareAndSet(true, false)) {
		getPosition()->getWorldMapInstance()->setDoorState(2, true); // this actually closes it on client side (wtf)
	}
}

// Java AshunatalShadowslipAI.java:44-49
void AshunatalShadowslipAI::handleBackHome() {
	isHome.set(true);
	AggressiveNpcAI::handleBackHome();
	getPosition()->getWorldMapInstance()->setDoorState(2, false); // this actually opens it on client side (wtf)
}

// Java AshunatalShadowslipAI.java:51-57
void AshunatalShadowslipAI::handleDied() {
	AggressiveNpcAI::handleDied();
	// ashunatal got killed early, therefore she could not spawn her shadow which usually would open the door
	getPosition()->getWorldMapInstance()->setDoorState(17, false); // this actually opens it on client side (wtf)
	getPosition()->getWorldMapInstance()->setDoorState(2, false); // this actually opens it on client side (wtf)
}

// Java AshunatalShadowslipAI.java:59-72
void AshunatalShadowslipAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	AggressiveNpcAI::onEndUseSkill(skillTemplate, skillLevel);
	if (skillTemplate->getSkillId() == 19417) {
		canThink_ = false;
		ThreadPoolManager::getInstance().schedule({this}, [this] {
			getSpawnTemplate()->setWalkerId(std::string_view("3002400001"));
			getOwner().setState(CreatureState::ACTIVE, true);
			gameserver::ai::manager::WalkManager::startWalking(*this);
			PacketSendUtility::broadcastPacket(getOwner(),
				network::aion::serverpackets::SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getObjectId()));
			ThreadPoolManager::getInstance().schedule({this}, [this] { getOwner().getController().delete_(); }, 4000);
		}, 3000);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress
