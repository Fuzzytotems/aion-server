#include "aion/gameserver/handlers/ai/SummonerAI.h"

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/dataholders/AIData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/ai/AITemplate.h"
#include "aion/gameserver/model/templates/ai/Percentage.h"
#include "aion/gameserver/model/templates/ai/SummonGroup.h"
#include "aion/gameserver/model/templates/ai/Summons.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::handlers::ai {

using model::templates::ai::Percentage;
using model::templates::ai::SummonGroup;

AION_AI(SummonerAI, "summoner");

// Java SummonerAI.java:31-35
void SummonerAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	checkPercentage(getLifeStats()->getHpPercentage());
}

// Java SummonerAI.java:37-42
void SummonerAI::handleDespawned() {
	AggressiveNpcAI::handleDespawned();
	removeAndResetHelperSpawns();
	clearPercentage();
}

// Java SummonerAI.java:44-48
void SummonerAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	removeAndResetHelperSpawns();
}

// Java SummonerAI.java:50-55
void SummonerAI::handleNotAtHome() {
	AggressiveNpcAI::handleNotAtHome();
	if (getState() == AIState::WALKING)
		removeAndResetHelperSpawns();
}

// Java SummonerAI.java:57-61
void SummonerAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	const model::templates::ai::AITemplate* aiTemplate = DataManager::AI_DATA->getAiTemplate(getNpcId());
	if (aiTemplate == nullptr || aiTemplate->getSummons() == nullptr) // Java: the chain's NullPointerException, explicit
		throw runtime::NullPointerException("ai template " + std::to_string(getNpcId()) + " has no summons");
	runtime::Ref<runtime::RcArrayList<const Percentage*>> list =
		runtime::RcArrayList<const Percentage*>::create(AION_LOCK_CLASS(SummonerAI::percentage));
	for (const Percentage& percent : aiTemplate->getSummons()->getPercentage())
		list->add(&percent);
	percentage.set(list);
}

// Java SummonerAI.java:63-68
void SummonerAI::handleDied() {
	AggressiveNpcAI::handleDied();
	removeAndResetHelperSpawns();
	clearPercentage();
}

/** Java percentage.clear(): Collections.emptyList() throws UnsupportedOperationException */
void SummonerAI::clearPercentage() {
	runtime::Ref<runtime::RcArrayList<const Percentage*>> list = percentage.get();
	if (list == nullptr)
		throw commons::utils::UnsupportedOperationException("Collections.emptyList().clear()");
	list->clear();
}

// Java SummonerAI.java:70-81
void SummonerAI::removeAndResetHelperSpawns() {
	SYNCHRONIZED(spawnedNpc) {
		for (int32_t object : spawnedNpc.snapshot()) {
			runtime::Ptr<VisibleObject> npc = world::World::getInstance().findVisibleObject(object);
			if (npc != nullptr && npc->isSpawned()) {
				npc->getController().delete_();
			}
		}
		spawnedNpc.clear();
	}
	spawnedPercent.set(0);
}

// Java SummonerAI.java:83-87
void SummonerAI::addHelpersSpawn(int32_t objId) {
	SYNCHRONIZED(spawnedNpc) {
		spawnedNpc.add(objId);
	}
}

// Java SummonerAI.java:89-114. The summon tasks are pinned on this AI (a part of its npc); their summon group is static data.
void SummonerAI::checkPercentage(int32_t hpPercentage) {
	runtime::Ref<runtime::RcArrayList<const Percentage*>> list = percentage.get();
	if (list == nullptr)
		return; // Java: Collections.emptyList(), nothing to iterate
	for (const Percentage* percent : list->snapshot()) {
		if (spawnedPercent.get() != 0 && spawnedPercent.get() <= percent->getPercent()) {
			continue;
		}
		if (hpPercentage <= percent->getPercent()) {
			spawnedPercent.set(percent->getPercent());
			int32_t skill = percent->getSkillId();
			if (skill != 0)
				AIActions::useSkill(*this, skill);
			if (percent->isIndividual()) {
				handleIndividualSpawnedSummons(*percent);
			} else if (!percent->getSummons().empty()) { // Java: percent.getSummons() != null (an absent list binds as none)
				handleBeforeSpawn(*percent);
				for (const SummonGroup& summonGroup : percent->getSummons()) {
					const SummonGroup* sg = &summonGroup;
					// lint: L5 sg points into the AI_DATA static template (fieldmap ai.SummonerAI@L110:48: `const SummonGroup*`, IsTemplatePtr)
					ThreadPoolManager::getInstance().schedule({this}, [this, sg] { spawnHelpers(*sg); }, summonGroup.getSchedule());
				}
			}
		}
	}
}

// Java SummonerAI.java:116-131
void SummonerAI::spawnHelpers(const SummonGroup& summonGroup) {
	if (!isDead() && checkBeforeSpawn()) {
		int32_t count = commons::utils::Rnd::get(summonGroup.getMinCount(), summonGroup.getMaxCount());
		for (int32_t i = 0; i < count; i++) {
			runtime::Ptr<VisibleObject> npc;
			if (summonGroup.getDistance() != 0)
				npc = rndSpawnInRange(summonGroup.getNpcId(), summonGroup.getDistance());
			else
				npc = spawn(summonGroup.getNpcId(), summonGroup.getX(), summonGroup.getY(), summonGroup.getZ(), summonGroup.getH());
			if (npc == nullptr) // Java: npc.getObjectId() of a failed spawn
				throw runtime::NullPointerException("the spawn of npc " + std::to_string(summonGroup.getNpcId()) + " is null");
			addHelpersSpawn(npc->getObjectId());
		}
		handleSpawnFinished(summonGroup);
	}
}

// Java SummonerAI.java:133-135
bool SummonerAI::checkBeforeSpawn() {
	return true;
}

// Java SummonerAI.java:137-138
void SummonerAI::handleBeforeSpawn(const Percentage& /*percent*/) {
}

// Java SummonerAI.java:140-141
void SummonerAI::handleSpawnFinished(const SummonGroup& /*summonGroup*/) {
}

// Java SummonerAI.java:143-144
void SummonerAI::handleIndividualSpawnedSummons(const Percentage& /*percent*/) {
}

} // namespace aion::gameserver::handlers::ai
