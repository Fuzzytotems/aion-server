#include "aion/gameserver/handlers/ai/siege/DredgionCommanderAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/services/siege/BalaurAssaultService.h"
#include "aion/gameserver/services/siege/FortressAssault.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/KnownObject.h"

namespace aion::gameserver::handlers::ai::siege {

AION_AI(DredgionCommanderAI, "dredgion_commander");

// Java DredgionCommanderAI.java:31-35
void DredgionCommanderAI::handleSpawned() {
	SiegeNpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] { findFortressBoss(); }, 3000);
}

// Java DredgionCommanderAI.java:37-45 (Java's findAny: the first known npc of the stream)
void DredgionCommanderAI::findFortressBoss() {
	runtime::Ref<Npc> found;
	for (const runtime::Ptr<world::knownlist::KnownObject>& knownObject : getKnownList().stream()) {
		runtime::Ptr<Npc> npc = runtime::as<Npc>(knownObject->get());
		if (npc != nullptr && (npc->getRace() == Race::GCHIEF_LIGHT || npc->getRace() == Race::GCHIEF_DARK)) {
			found = runtime::Ref<Npc>(npc);
			break;
		}
	}
	fortressBoss.set(found);
	if (found != nullptr)
		getAggroList().addHate(*found, 500000);
}

// Java DredgionCommanderAI.java:47-55
void DredgionCommanderAI::handleMoveArrived() {
	SiegeNpcAI::handleMoveArrived();
	runtime::Ref<Npc> boss = fortressBoss.get();
	if (getOwner().getDistanceToSpawnLocation() >= 25.0 && boss != nullptr) {
		getAggroList().addHate(*boss, 1000000);
		AIActions::targetCreature(*this, *boss);
		getOwner().getMoveController()->moveToPoint(boss->getX(), boss->getY(), boss->getZ());
	}
}

// Java DredgionCommanderAI.java:57-62
float DredgionCommanderAI::modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) {
	static_cast<void>(effect);
	if (&effected == fortressBoss.get().get())
		damage *= SiegeConfig::FORTRESS_PROTECTOR_HEALTH_MULTIPLIER.load();
	return damage;
}

// Java DredgionCommanderAI.java:64-68
void DredgionCommanderAI::modifyOwnerStat(Stat2& stat) {
	if (stat.getStat() == StatEnum::MAXHP)
		stat.setBaseRate(SiegeConfig::FORTRESS_PROTECTOR_HEALTH_MULTIPLIER.load());
}

// Java DredgionCommanderAI.java:70-74
void DredgionCommanderAI::handleDespawned() {
	fortressBoss.set(nullptr);
	SiegeNpcAI::handleDespawned();
}

// Java DredgionCommanderAI.java:76-83 (Java's cast (SiegeNpc) getOwner())
void DredgionCommanderAI::handleDied() {
	runtime::Ptr<services::siege::FortressAssault> assault =
		services::siege::BalaurAssaultService::getInstance().getFortressAssaultBySiegeId(runtime::cast<SiegeNpc>(getOwner())->getSiegeId());
	if (assault != nullptr)
		assault->onDredgionCommanderKilled();
	fortressBoss.set(nullptr);
	SiegeNpcAI::handleDied();
}

} // namespace aion::gameserver::handlers::ai::siege
