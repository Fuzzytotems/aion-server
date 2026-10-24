#include "aion/gameserver/handlers/ai/instance/theShugoEmperorsVault/IDSweep_HealTower.h"

#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault {

AION_AI(IDSweep_HealTower, "idsweep_healtower");

// Java IDSweep_HealTower.java:24-28
void IDSweep_HealTower::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	schedule.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { tryHeal(); }, 2000, 3000));
}

// Java IDSweep_HealTower.java:30-34
void IDSweep_HealTower::handleDespawned() {
	GeneralNpcAI::handleDespawned();
	cancelTask();
}

// Java IDSweep_HealTower.java:36-40
void IDSweep_HealTower::handleDied() {
	GeneralNpcAI::handleDied();
	cancelTask();
}

// Java IDSweep_HealTower.java:42-46
void IDSweep_HealTower::cancelTask() {
	runtime::FutureRef task = schedule.get();
	if (task != nullptr && !task->isCancelled()) {
		task->cancel(true);
	}
}

// Java IDSweep_HealTower.java:48-55
void IDSweep_HealTower::tryHeal() {
	if (!getOwner().isSpawned() || getOwner().isDead())
		return;
	bool anyToHeal = false; // Java: streamPlayers().noneMatch(p -> !p.isDead() && !p.getLifeStats().isFullyRestoredHp() && isInRange(p, 3))
	for (const runtime::Ptr<Player>& p : getKnownList().streamPlayers()) {
		if (!p->isDead() && !p->getLifeStats()->isFullyRestoredHp() && isInRange(*p, 3)) {
			anyToHeal = true;
			break;
		}
	}
	if (!anyToHeal)
		return;
	getOwner().setTarget(runtime::Ptr<VisibleObject>(getOwner()));
	SkillEngine::getInstance().getSkill(getOwner(), 21837, 1, runtime::Ptr<VisibleObject>(getOwner()))->useSkill();
}

} // namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault
