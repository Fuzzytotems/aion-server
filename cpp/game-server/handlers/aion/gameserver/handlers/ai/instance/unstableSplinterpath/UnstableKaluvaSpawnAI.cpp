#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/UnstableKaluvaSpawnAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

AION_AI(UnstableKaluvaSpawnAI, "unstablekaluvaspawn");

// Java UnstableKaluvaSpawnAI.java:26-32
void UnstableKaluvaSpawnAI::handleDied() {
	NpcAI::handleDied();
	runtime::FutureRef current = task.get();
	if (current != nullptr && !current->isDone())
		current->cancel(true);
	checkKaluva();
}

// Java UnstableKaluvaSpawnAI.java:34-37
void UnstableKaluvaSpawnAI::handleCreatureSee(Creature& creature) {
	onMoved(creature);
}

// Java UnstableKaluvaSpawnAI.java:39-42
void UnstableKaluvaSpawnAI::handleCreatureMoved(Creature& creature) {
	onMoved(creature);
}

// Java UnstableKaluvaSpawnAI.java:44-51
void UnstableKaluvaSpawnAI::onMoved(Creature& creature) {
	runtime::Ptr<Npc> npc = runtime::as<Npc>(runtime::Ptr<Creature>(creature));
	if (task.get() == nullptr && npc != nullptr && npc->getNpcId() == 219553) {
		if (PositionUtil::isInRange(getOwner(), creature, 7)) {
			creature.getEffectController()->removeEffect(19152);
			scheduleHatch();
		}
	}
}

// Java UnstableKaluvaSpawnAI.java:53-57
void UnstableKaluvaSpawnAI::handleSpawned() {
	NpcAI::handleSpawned();
	SkillEngine::getInstance().getSkill(getOwner(), 19222, 55, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
}

// Java UnstableKaluvaSpawnAI.java:59-65
void UnstableKaluvaSpawnAI::checkKaluva() {
	runtime::Ptr<Npc> kaluva = getPosition()->getWorldMapInstance()->getNpc(219553);
	if (kaluva != nullptr && !kaluva->isDead()) {
		kaluva->getEffectController()->removeEffect(19152);
	}
	AIActions::deleteOwner(*this);
}

// Java UnstableKaluvaSpawnAI.java:67-74
void UnstableKaluvaSpawnAI::scheduleHatch() {
	task.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead()) {
			hatchAdds();
			checkKaluva();
		}
	}, 28000)); // schedule hatch when debuff ends(20s)
}

// Java UnstableKaluvaSpawnAI.java:76-98
void UnstableKaluvaSpawnAI::hatchAdds() { // 4 different spawn-formations; See Powerwiki for more information
	runtime::Ptr<WorldPosition> p = getPosition();
	switch (commons::utils::Rnd::get(1, 4)) {
		case 1:
			spawn(219572, p->getX(), p->getY(), p->getZ(), p->getHeading());
			spawn(219572, p->getX(), p->getY(), p->getZ(), p->getHeading());
			break;
		case 2:
			for (int32_t i = 0; i < 12; i++) {
				spawn(219573, p->getX(), p->getY(), p->getZ(), p->getHeading());
			}
			break;
		case 3:
			spawn(219584, p->getX(), p->getY(), p->getZ(), p->getHeading());
			break;
		case 4:
			spawn(219572, p->getX(), p->getY(), p->getZ(), p->getHeading());
			spawn(219573, p->getX(), p->getY(), p->getZ(), p->getHeading());
			spawn(219573, p->getX(), p->getY(), p->getZ(), p->getHeading());
			spawn(219573, p->getX(), p->getY(), p->getZ(), p->getHeading());
			break;
		default:
			break;
	}
}

// Java UnstableKaluvaSpawnAI.java:100-106
bool UnstableKaluvaSpawnAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return NpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath
