#include "aion/gameserver/handlers/ai/instance/theobomosLab/TriroansSummonAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/walker/RouteStep.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::theobomosLab {

AION_AI(TriroansSummonAI, "triroan_summon");

// Java TriroansSummonAI.java:29-32
bool TriroansSummonAI::canThink() {
	return false;
}

// Java TriroansSummonAI.java:34-55
void TriroansSummonAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	switch (getNpcId()) {
		case 280975:
			walkPosition = 3;
			helperSkill = 18493;
			break;
		case 280976:
			walkPosition = 4;
			helperSkill = 18492;
			break;
		case 280977:
			walkPosition = 2;
			helperSkill = 18485;
			break;
		case 280978:
			walkPosition = 5;
			helperSkill = 18491;
			break;
		default:
			break;
	}
}

// Java TriroansSummonAI.java:57-69
void TriroansSummonAI::handleMoveArrived() {
	AggressiveNpcAI::handleMoveArrived();
	int32_t point = getOwner().getMoveController()->getCurrentStep()->getStepIndex();
	if (walkPosition.get() == point) {
		if (isDestroyed.compareAndSet(false, true)) {
			getSpawnTemplate()->setWalkerId(std::nullopt);
			WalkManager::stopWalking(*this);
			useSkill();
			startDespawnTask();
		}
	}
}

// Java TriroansSummonAI.java:71-77 (synchronized)
void TriroansSummonAI::useSkill() {
	SYNCHRONIZED(monitor) {
		runtime::Ptr<Npc> boss = getPosition()->getWorldMapInstance()->getNpc(214669);
		if (boss != nullptr && checkLocation(getOwner()) && !boss->isDead()) {
			SkillEngine::getInstance().getSkill(*boss, helperSkill.get(), 50, runtime::Ptr<VisibleObject>(boss))->useSkill();
		} else
			checkSkillUse(boss);
	}
}

// Java TriroansSummonAI.java:79-94 (the anonymous Runnable: a task pinning this and the boss)
void TriroansSummonAI::checkSkillUse(runtime::Ptr<Npc> boss) {
	if (boss != nullptr && checkDistance() == 0 && !boss->isDead()) {
		if (!boss->isCasting())
			SkillEngine::getInstance().getSkill(*boss, helperSkill.get(), 50, runtime::Ptr<VisibleObject>(boss))->useSkill();
		else {
			Npc& pinnedBoss = *boss;
			ThreadPoolManager::getInstance().schedule({this, &pinnedBoss}, [this, &pinnedBoss] {
				if (checkDistance() == 0 && !pinnedBoss.isDead())
					checkSkillUse(runtime::Ptr<Npc>(pinnedBoss));
			}, 5000);
		}
	}
}

// Java TriroansSummonAI.java:96-107
bool TriroansSummonAI::checkLocation(Npc& npc) {
	if (checkDistance() == 1 && npc.getNpcId() == 280975)
		return true;
	else if (checkDistance() == 2 && npc.getNpcId() == 280976)
		return true;
	else if (checkDistance() == 3 && npc.getNpcId() == 280977)
		return true;
	else if (checkDistance() == 4 && npc.getNpcId() == 280978)
		return true;
	else
		return false;
}

// Java TriroansSummonAI.java:109-121 (the boss is not checked for null: Java's NullPointerException when Triroan is gone)
int32_t TriroansSummonAI::checkDistance() {
	runtime::Ptr<Npc> boss = getPosition()->getWorldMapInstance()->getNpc(214669);
	if (PositionUtil::getDistance(*boss, 624.002f, 474.241f, 196.160f) <= 5)
		return 1;
	else if (PositionUtil::getDistance(*boss, 623.23f, 502.715f, 196.087f) <= 5)
		return 2;
	else if (PositionUtil::getDistance(*boss, 579.943f, 500.999f, 196.604f) <= 5)
		return 3;
	else if (PositionUtil::getDistance(*boss, 578.323f, 475.784f, 196.463f) <= 5)
		return 4;
	else
		return 0;
}

// Java TriroansSummonAI.java:123-131 (the anonymous Runnable: a task pinning this)
void TriroansSummonAI::startDespawnTask() {
	ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::deleteOwner(*this); }, 3000);
}

} // namespace aion::gameserver::handlers::ai::instance::theobomosLab
