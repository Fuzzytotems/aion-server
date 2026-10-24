#include "aion/gameserver/handlers/ai/instance/darkPoeta/WoundedScarAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/walker/RouteStep.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::darkPoeta {

AION_AI(WoundedScarAI, "wounded_scar");

// Java WoundedScarAI.java:33-36
bool WoundedScarAI::canThink() {
	return false;
}

// Java WoundedScarAI.java:38-44
void WoundedScarAI::handleDialogStart(Player& player) {
	Npc& ownerNpc = getOwner(); // Java: owner (a member's name here)
	ownerNpc.getSpawn()->setWalkerId("9C671D72B623B88FDFAFF9E2AF491976B84AE720");
	ownerNpc.overrideNpcType(CreatureType::INVULNERABLE);
	WalkManager::startWalking(dynamic_cast<NpcAI&>(ownerNpc.getAi()));
}

// Java WoundedScarAI.java:46-79
void WoundedScarAI::handleMoveArrived() {
	GeneralNpcAI::handleMoveArrived();
	if (getOwner().getMoveController()->isStop() && isDestinationReached.compareAndSet(false, true)) {
		runtime::Ptr<world::WorldPosition> p = getPosition();
		spawn(281116, p->getX(), p->getY(), p->getZ(), p->getHeading());
		AIActions::deleteOwner(*this);
		return;
	}
	const model::templates::walker::RouteStep* step = getMoveController()->getCurrentStep();
	if (step == nullptr) // Java: getCurrentStep().getStepIndex() of no step
		throw runtime::NullPointerException("NpcMoveController.getCurrentStep()");
	switch (step->getStepIndex()) {
		case 8:
			skillengine::SkillEngine::getInstance().applyEffectDirectly(18532, getOwner(), getOwner());
			spawn(281148, 648.8706f, 1167.4102f, 143.6956f, int8_t{2});
			spawn(281148, 649.7064f, 1164.4081f, 143.4077f, int8_t{6});
			spawn(281148, 651.4119f, 1161.6421f, 143.6618f, int8_t{11});
			getOwner().setState(CreatureState::ACTIVE, true);
			PacketSendUtility::broadcastToMap(getOwner(), network::aion::serverpackets::SM_EMOTION(getOwner(), EmotionType::RUN));
			break;
		case 15:
			skillengine::SkillEngine::getInstance().applyEffectDirectly(18532, getOwner(), getOwner());
			spawn(281149, 530.7089f, 1179.5717f, 138.0460f, int8_t{7});
			break;
		case 25:
			skillengine::SkillEngine::getInstance().applyEffectDirectly(18532, getOwner(), getOwner());
			spawn(281150, 508.9217f, 1050.1077f, 123.8376f, int8_t{87});
			break;
		case 26:
			skillengine::SkillEngine::getInstance().applyEffectDirectly(18532, getOwner(), getOwner());
			spawn(281151, 504.4358f, 1064.9199f, 126.04446f, int8_t{53});
			spawn(281151, 506.9358f, 1066.3199f, 126.04446f, int8_t{53});
			break;
	}
}

} // namespace aion::gameserver::handlers::ai::instance::darkPoeta
