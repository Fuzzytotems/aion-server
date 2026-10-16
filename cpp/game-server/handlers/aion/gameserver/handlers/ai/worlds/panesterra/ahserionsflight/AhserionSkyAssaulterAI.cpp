#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionSkyAssaulterAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

AION_AI(AhserionSkyAssaulterAI, "ahserion_sky_assaulter");

// Java AhserionSkyAssaulterAI.java:22-26
void AhserionSkyAssaulterAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] { activate(); }, 400);
}

// Java AhserionSkyAssaulterAI.java:28-35 (Java's cast (Npc) of the pod; a failed spawn is its NullPointerException)
void AhserionSkyAssaulterAI::activate() {
	runtime::Ptr<world::WorldPosition> p = getPosition();
	runtime::Ptr<Npc> spawned = runtime::cast<Npc>(spawn(297188, p->getX(), p->getY(), p->getZ(), p->getHeading())); // Assault Pod
	if (spawned == nullptr)
		throw runtime::NullPointerException("AhserionSkyAssaulterAI: the assault pod 297188");
	runtime::Ref<Npc> assaultPod(spawned);
	assaultPod->getController().addTask(model::TaskId::DESPAWN, ThreadPoolManager::getInstance().schedule({&*assaultPod}, [assaultPod] {
		assaultPod->getController().deleteIfAliveOrCancelRespawn();
	}, 6000));
	ThreadPoolManager::getInstance().schedule({this}, [this] { useSkill(); }, 1000);
	ThreadPoolManager::getInstance().schedule({this}, [this] { spawnDefenders(); }, 4000);
}

// Java AhserionSkyAssaulterAI.java:37-39
void AhserionSkyAssaulterAI::useSkill() {
	runtime::Ref<skillengine::model::Skill> skill = SkillEngine::getInstance().getSkill(getOwner(), 20776, 1, runtime::Ptr<VisibleObject>(getOwner()));
	if (skill == nullptr) // Java: getSkill(...).useWithoutPropSkill() on null
		throw runtime::NullPointerException("AhserionSkyAssaulterAI: skill 20776");
	skill->useWithoutPropSkill(); // Trooper Shock
}

// Java AhserionSkyAssaulterAI.java:41-56
void AhserionSkyAssaulterAI::spawnDefenders() {
	runtime::Ptr<world::WorldPosition> p = getPosition();
	switch (getNpcId()) {
		case 297352:
			spawn(297191, p->getX() + 3, p->getY() - 3, p->getZ(), p->getHeading()); // Ahserion Troopers Assassin
			spawn(297192, p->getX(), p->getY(), p->getZ() + 0.1f, p->getHeading()); // Ahserion Troopers Sorcerer
			spawn(297191, p->getX() + 3, p->getY() + 3, p->getZ(), p->getHeading()); // Ahserion Troopers Assassin
			break;
		case 297353:
			spawn(297190, p->getX() - 2, p->getY() + 2, p->getZ(), p->getHeading()); // Ahserion Troopers Defender Captain
			spawn(297191, p->getX() + 2, p->getY() - 2, p->getZ(), p->getHeading()); // Ahserion Troopers Assassin
			spawn(297191, p->getX() - 2, p->getY() - 2, p->getZ() + 2, p->getHeading()); // Ahserion Troopers Assassin
			break;
		default:
			break;
	}
	AIActions::deleteOwner(*this);
}

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
