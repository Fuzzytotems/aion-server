#include "aion/gameserver/handlers/ai/instance/rentusBase/VashartiAssassinAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

AION_AI(VashartiAssassinAI, "vasharti_assassin");

// Java VashartiAssassinAI.java:26-34
void VashartiAssassinAI::handleCreatureAggro(Creature& creature) {
	if (isHome.compareAndSet(true, false)) {
		runtime::Ptr<WorldPosition> p = getPosition();
		runtime::Ptr<Npc> smoke = runtime::cast<Npc>(spawn(282465, p->getX(), p->getY(), p->getZ(), p->getHeading()));
		smoke->getController().delete_();
	}
	AggressiveNpcAI::handleCreatureAggro(creature);
}

// Java VashartiAssassinAI.java:36-43
void VashartiAssassinAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead())
			SkillEngine::getInstance().getSkill(getOwner(), 19915, 60, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
	}, 2000);
}

// Java VashartiAssassinAI.java:45-52
void VashartiAssassinAI::handleBackHome() {
	isHome.set(true);
	AggressiveNpcAI::handleBackHome();
	getEffectController()->removeEffect(19915);
	getEffectController()->removeEffect(19916);
	SkillEngine::getInstance().getSkill(getOwner(), 19915, 60, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
}

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
