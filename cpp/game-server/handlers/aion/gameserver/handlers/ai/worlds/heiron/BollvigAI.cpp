#include "aion/gameserver/handlers/ai/worlds/heiron/BollvigAI.h"

#include <cmath>
#include <numbers>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::worlds::heiron {

AION_AI(BollvigAI, "bollvig");

// Java BollvigAI.java:34-40
void BollvigAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	runtime::Ptr<Npc> npc = getPosition()->getWorldMapInstance()->getNpc(204655);
	if (npc != nullptr)
		npc->getController().delete_();
}

// Java BollvigAI.java:42-46
void BollvigAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java BollvigAI.java:48-61
void BollvigAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 75:
		case 50:
			cancelTask();
			useFirstSkillTree();
			break;
		case 25:
			cancelTask();
			firstSkill();
			break;
		default:
			break;
	}
}

// Java BollvigAI.java:63-70
void BollvigAI::useFirstSkillTree() {
	useSkill(17861); // Sleep of Death
	rndSpawnInRange(280802);
	rndSpawnInRange(280802);
	rndSpawnInRange(280803);
	rndSpawnInRange(280803);
	firstSkill();
}

// Java BollvigAI.java:72-83
void BollvigAI::firstSkill() {
	int32_t hpPercent = getLifeStats()->getHpPercentage();
	if (50 >= hpPercent && hpPercent > 25) {
		firstTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
			useSkill(18034); // Nerve Absorption
			rndSpawnInRange(280804);
		}, 10000));
	} else if (hpPercent <= 25) {
		useSkill(18037); // Blood Cell Destruction
	}
	secondTask.set(ThreadPoolManager::getInstance().schedule(this, &BollvigAI::skillThree, 31000));
}

// Java BollvigAI.java:85-100
void BollvigAI::skillThree() {
	useSkill(17899); // Charming Attraction
	thirdTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
		int32_t hpPercent = getLifeStats()->getHpPercentage();
		if (hpPercent <= 75 && hpPercent > 50) {
			useSkill(18025); // Curse of Soul
			firstSkill();
		} else if (hpPercent <= 50 && hpPercent > 25) {
			useSkill(18025); // Curse of Soul
			firstSkill();
		} else if (hpPercent <= 25) {
			useSkill(18027); // Mortal Cutting
			lastTask.set(ThreadPoolManager::getInstance().schedule(this, &BollvigAI::skillThree, 11000));
		}
	}, 5000));
}

// Java BollvigAI.java:102-111. Correction of the Java code (owner's decision 2026-10-19, both branches; was J-A1C-2): every task that is not
// done is cancelled; Java's else-if chain cancelled only the first, so a round's Charming Attraction outlived a new phase or a return home
void BollvigAI::cancelTask() {
	runtime::FutureRef first = firstTask.get();
	runtime::FutureRef second = secondTask.get();
	runtime::FutureRef third = thirdTask.get();
	runtime::FutureRef last = lastTask.get();
	if (first != nullptr && !first->isDone())
		first->cancel(true);
	if (second != nullptr && !second->isDone())
		second->cancel(true);
	if (third != nullptr && !third->isDone())
		third->cancel(true);
	if (last != nullptr && !last->isDone())
		last->cancel(true);
}

// Java BollvigAI.java:113-118
void BollvigAI::rndSpawnInRange(int32_t npcId) {
	double angleRadians = static_cast<double>(commons::utils::Rnd::nextFloat(360.0f)) * std::numbers::pi / 180.0; // Java: Math.toRadians
	float x = static_cast<float>(std::cos(angleRadians) * 10);
	float y = static_cast<float>(std::sin(angleRadians) * 10);
	spawn(npcId, 1001 + x, 2828 + y, 235.66f, int8_t{0});
}

// Java BollvigAI.java:120-122
void BollvigAI::useSkill(int32_t skillId) {
	SkillEngine::getInstance().getSkill(getOwner(), skillId, 50, getTarget())->useSkill();
}

// Java BollvigAI.java:124-129
void BollvigAI::handleBackHome() {
	cancelTask();
	AggressiveNpcAI::handleBackHome();
	hpPhases->reset();
}

// Java BollvigAI.java:131-140
void BollvigAI::handleDespawned() {
	cancelTask();
	deleteSummons(280802);
	deleteSummons(280803);
	deleteSummons(280804);
	AggressiveNpcAI::handleDespawned();
	if (checkNpc())
		spawn(204655, 1001.0f, 2828.0f, 235.66f, int8_t{0});
}

// Java BollvigAI.java:142-151
void BollvigAI::handleDied() {
	cancelTask();
	deleteSummons(280802);
	deleteSummons(280803);
	deleteSummons(280804);
	AggressiveNpcAI::handleDied();
	if (checkNpc())
		spawn(204655, 1001.0f, 2828.0f, 235.66f, int8_t{0});
}

// Java BollvigAI.java:153-160 (getNpcs never returns null: the check is always true)
void BollvigAI::deleteSummons(int32_t npcId) {
	std::vector<runtime::Ptr<Npc>> npcs = getPosition()->getWorldMapInstance()->getNpcs({npcId});
	for (const runtime::Ptr<Npc>& npc : npcs) {
		npc->getController().delete_();
	}
}

// Java BollvigAI.java:162-165
bool BollvigAI::checkNpc() {
	runtime::Ptr<WorldMapInstance> map = getPosition()->getWorldMapInstance();
	return map->getNpc(204655) == nullptr && (map->getNpc(212314) == nullptr || map->getNpc(212314)->isDead());
}

} // namespace aion::gameserver::handlers::ai::worlds::heiron
