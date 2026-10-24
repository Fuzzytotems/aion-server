#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/BrigadeGeneralLaksyakaAI.h"

#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(BrigadeGeneralLaksyakaAI, "brigadegenerallaksyaka");

// Java BrigadeGeneralLaksyakaAI.java:35-46
void BrigadeGeneralLaksyakaAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (commons::utils::Rnd::chance() < 3)
		spawnSummon();
	if (isHome.compareAndSet(true, false))
		startSkillTask();
	if (!isFinalBuff.get() && getOwner().getLifeStats()->getHpPercentage() <= 25) {
		isFinalBuff = true;
		AIActions::useSkill(*this, 20731);
	}
}

// Java BrigadeGeneralLaksyakaAI.java:48-60 (the anonymous Runnable: a task pinning this)
void BrigadeGeneralLaksyakaAI::startSkillTask() {
	skeletonTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		if (isDead())
			cancelTask();
		else {
			startSkeletonEvent();
		}
	}, 5000, 40000));
}

// Java BrigadeGeneralLaksyakaAI.java:62-66
void BrigadeGeneralLaksyakaAI::cancelTask() {
	runtime::FutureRef task = skeletonTask.get();
	if (task != nullptr && !task->isCancelled()) {
		task->cancel(true);
	}
}

// Java BrigadeGeneralLaksyakaAI.java:68-80 (no Tiamat's Eye: Java's NullPointerException in isInRange)
void BrigadeGeneralLaksyakaAI::startSkeletonEvent() {
	runtime::Ptr<Npc> tiamatEye = getPosition()->getWorldMapInstance()->getNpc(283089); // 4.0
	std::vector<runtime::Ptr<Player>> players;
	getKnownList().forEachPlayer([&players, &tiamatEye](Player& player) {
		if (!player.isDead() && PositionUtil::isInRange(player, *tiamatEye, 40)) {
			players.emplace_back(player);
		}
	});
	if (!players.empty()) {
		runtime::Ptr<Player> player = *commons::utils::Rnd::get(players);
		SkillEngine::getInstance().applyEffectDirectly(20865, *tiamatEye, *player);
	}
}

// Java BrigadeGeneralLaksyakaAI.java:82-86
void BrigadeGeneralLaksyakaAI::spawnSummon() {
	if (getPosition()->getWorldMapInstance()->getNpc(283115) == nullptr) { // 4.0
		rndSpawn(283115, 4); // 4.0
	}
}

// Java BrigadeGeneralLaksyakaAI.java:88-92
void BrigadeGeneralLaksyakaAI::rndSpawn(int32_t npcId, int32_t count) {
	for (int32_t i = 0; i < count; i++) {
		rndSpawnInRange(npcId, 10);
	}
}

// Java BrigadeGeneralLaksyakaAI.java:94-98
void BrigadeGeneralLaksyakaAI::handleDied() {
	AggressiveNpcAI::handleDied();
	cancelTask();
}

// Java BrigadeGeneralLaksyakaAI.java:100-104
void BrigadeGeneralLaksyakaAI::handleBeforeSpawned() {
	AggressiveNpcAI::handleBeforeSpawned();
	getOwner().overrideNpcType(CreatureType::PEACE);
}

// Java BrigadeGeneralLaksyakaAI.java:106-110
void BrigadeGeneralLaksyakaAI::handleDespawned() {
	AggressiveNpcAI::handleDespawned();
	cancelTask();
}

// Java BrigadeGeneralLaksyakaAI.java:112-118
void BrigadeGeneralLaksyakaAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	cancelTask();
	isFinalBuff = false;
	isHome.set(true);
}

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
