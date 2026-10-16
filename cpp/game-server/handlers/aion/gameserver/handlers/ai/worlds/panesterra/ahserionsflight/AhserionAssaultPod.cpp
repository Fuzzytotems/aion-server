#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionAssaultPod.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

AION_AI(AhserionAssaultPod, "ahserion_assault_pod");

// Java AhserionAssaultPod.java:21-25
void AhserionAssaultPod::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] { activate(); }, 400);
}

// Java AhserionAssaultPod.java:27-32 (the position is the owner's live WorldPosition, as Java's)
void AhserionAssaultPod::activate() {
	runtime::Ref<world::WorldPosition> p(getPosition());
	runtime::Ref<skillengine::model::Skill> skill = SkillEngine::getInstance().getSkill(getOwner(), 20776, 1, runtime::Ptr<VisibleObject>(getOwner()));
	if (skill == nullptr) // Java: getSkill(...).useWithoutPropSkill() on null
		throw runtime::NullPointerException("AhserionAssaultPod: skill 20776");
	skill->useWithoutPropSkill(); // Trooper Shock
	ThreadPoolManager::getInstance().schedule({this}, [this, p] { spawnDefenders(*p); }, 4000);
	ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::deleteOwner(*this); }, 6000);
}

// Java AhserionAssaultPod.java:34-38
void AhserionAssaultPod::spawnDefenders(world::WorldPosition& p) {
	spawn(297191, p.getX() + 3, p.getY() - 3, p.getZ() + 3, p.getHeading()); // Ahserion Troopers Assassin
	spawn(297191, p.getX(), p.getY(), p.getZ() + 3, p.getHeading()); // Ahserion Troopers Sorcerer
	spawn(297191, p.getX() + 3, p.getY() + 3, p.getZ() + 3, p.getHeading()); // Ahserion Troopers Assassin
}

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
