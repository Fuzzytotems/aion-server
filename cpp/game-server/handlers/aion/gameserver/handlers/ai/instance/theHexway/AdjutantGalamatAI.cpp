#include "aion/gameserver/handlers/ai/instance/theHexway/AdjutantGalamatAI.h"

#include <algorithm>
#include <vector>

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/templates/ai/Percentage.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::theHexway {

AION_AI(AdjutantGalamatAI, "adjutant_galamat");

// Java AdjutantGalamatAI.java:40-48
void AdjutantGalamatAI::handleIndividualSpawnedSummons(const model::templates::ai::Percentage& percent) {
	switch (percent.getPercent()) {
		case 60:
		case 20:
			getOwner().clearQueuedSkills();
			getOwner().queueSkill(21799, 65, 25000);
			break;
		default:
			break;
	}
}

// Java AdjutantGalamatAI.java:51-81
void AdjutantGalamatAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate->getSkillId() == 21799) {
		shieldPhase.set(true);
		addSpawnTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { rndSpawnInRange(219616, 12); }, 0, 4000));
		shieldPhaseEvaluationTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
			shieldPhase.set(false);
			runtime::FutureRef spawnTask = addSpawnTask.get();
			if (spawnTask != nullptr && !spawnTask->isDone())
				spawnTask->cancel(true);
			std::vector<runtime::Ptr<Player>> playersInRange;
			for (const runtime::Ptr<Player>& p : getKnownList().streamPlayers()) {
				if (isInRange(*p, 30))
					playersInRange.push_back(p);
			}
			if (playersInRange.empty())
				return;
			int32_t dmgPerPlayer = damageInShieldPhase.getAndSet(0) / static_cast<int32_t>(playersInRange.size());
			float multiplicatorToAdd = 0.0f;
			for (const runtime::Ptr<Player>& player : playersInRange) {
				if (player->isDead()) {
					multiplicatorToAdd += 0.2f;
					continue;
				}
				if (dmgPerPlayer > 0 && player->getLifeStats()->getCurrentHp() <= dmgPerPlayer)
					multiplicatorToAdd += 0.35f;
			}
			SYNCHRONIZED(monitor) {
				if (damageMultiplicator.get() == 1.0f)
					damageMultiplicator = 1.2f;
				damageMultiplicator = damageMultiplicator.get() + multiplicatorToAdd;
				damageMultiplicator = std::min(damageMultiplicator.get(), 2.7f);
			}
		}, 25000));
	}
	SummonerAI::onEndUseSkill(skillTemplate, skillLevel);
}

// Java AdjutantGalamatAI.java:84-88
void AdjutantGalamatAI::handleBackHome() {
	resetVariablesAndCancelTasks();
	SummonerAI::handleBackHome();
}

// Java AdjutantGalamatAI.java:90-94
void AdjutantGalamatAI::handleDespawned() {
	resetVariablesAndCancelTasks();
	SummonerAI::handleDespawned();
}

// Java AdjutantGalamatAI.java:96-99
void AdjutantGalamatAI::handleDied() {
	resetVariablesAndCancelTasks();
	SummonerAI::handleDied();
}

// Java AdjutantGalamatAI.java:101-112
void AdjutantGalamatAI::resetVariablesAndCancelTasks() {
	shieldPhase.set(false);
	damageInShieldPhase.set(0);
	runtime::FutureRef evaluation = shieldPhaseEvaluationTask.get();
	if (evaluation != nullptr && !evaluation->isDone()) {
		evaluation->cancel(true);
		shieldPhaseEvaluationTask.set(nullptr);
	}
	runtime::FutureRef spawnTask = addSpawnTask.get();
	if (spawnTask != nullptr && !spawnTask->isDone()) {
		spawnTask->cancel(true);
		addSpawnTask.set(nullptr);
	}
}

// Java AdjutantGalamatAI.java:115-117
float AdjutantGalamatAI::modifyOwnerDamage(float damage, Creature& /*effected*/, runtime::Ptr<Effect> /*effect*/) {
	return damage * damageMultiplicator.get();
}

// Java AdjutantGalamatAI.java:120-130
float AdjutantGalamatAI::modifyDamage(Creature& /*attacker*/, float damage, runtime::Ptr<Effect> /*effect*/) {
	if (shieldPhase.get()) {
		if (getEffectController()->findBySkillId(21799) == nullptr) {
			getOwner().clearQueuedSkills();
			resetVariablesAndCancelTasks();
		} else {
			damageInShieldPhase.addAndGet(static_cast<int32_t>(damage));
		}
	}
	return damage;
}

} // namespace aion::gameserver::handlers::ai::instance::theHexway
