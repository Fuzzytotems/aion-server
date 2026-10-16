#include "aion/gameserver/handlers/ai/instance/raksang/RakshaAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::raksang {

AION_AI(RakshaAI, "raksha");

// Java RakshaAI.java:34-38
void RakshaAI::handleDespawned() {
	cancelPhaseTask();
	AggressiveNpcAI::handleDespawned();
}

// Java RakshaAI.java:40-45
void RakshaAI::handleDied() {
	cancelPhaseTask();
	spawn(730445, 1062.281f, 889.900f, 138.744f, int8_t{29});
	AggressiveNpcAI::handleDied();
}

// Java RakshaAI.java:47-54
void RakshaAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (isAggred.compareAndSet(false, true)) {
		PacketSendUtility::broadcastMessage(getOwner(), 1401152);
	}
	hpPhases->tryEnterNextPhase(*this);
}

// Java RakshaAI.java:56-60
void RakshaAI::handleHpPhase(int32_t phaseHpPercent) {
	PacketSendUtility::broadcastMessage(getOwner(), 1401154);
	startPhaseTask();
}

// Java RakshaAI.java:62-89 (the anonymous Runnable: a lambda pinned to the AI)
void RakshaAI::startPhaseTask() {
	phaseTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		if (isDead()) {
			cancelPhaseTask();
		} else {
			SkillEngine::getInstance().getSkill(getOwner(), 19938, 46, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
			std::vector<runtime::Ptr<Player>> players = getLifedPlayers();
			if (!players.empty()) {
				int32_t size = static_cast<int32_t>(players.size());
				if (players.size() < 4) {
					for (const runtime::Ptr<Player>& p : players) {
						spawnRubble(*p);
					}
				} else {
					int32_t count = commons::utils::Rnd::get(3, size);
					for (int32_t i = 0; i < count; i++) {
						spawnRubble(**commons::utils::Rnd::get(players));
					}
				}
			}
		}
	}, 3000, 15000));
}

// Java RakshaAI.java:91-108
void RakshaAI::spawnRubble(Player& player) {
	const float x = player.getX();
	const float y = player.getY();
	const float z = player.getZ();
	if (x > 0 && y > 0 && z > 0) {
		ThreadPoolManager::getInstance().schedule({this}, [this, x, y, z] {
			if (!isDead()) {
				spawn(282325, x, y, z, int8_t{0});
			}
		}, 3000);

	}
}

// Java RakshaAI.java:110-118
std::vector<runtime::Ptr<Player>> RakshaAI::getLifedPlayers() {
	std::vector<runtime::Ptr<Player>> players;
	getKnownList().forEachPlayer([&players](Player& player) {
		if (!player.isDead()) {
			players.push_back(runtime::Ptr<Player>(player));
		}
	});
	return players;
}

// Java RakshaAI.java:120-124
void RakshaAI::cancelPhaseTask() {
	runtime::FutureRef task = phaseTask.get();
	if (task != nullptr && !task->isDone()) {
		task->cancel(true);
	}
}

// Java RakshaAI.java:126-132
void RakshaAI::handleBackHome() {
	cancelPhaseTask();
	isAggred.set(false);
	AggressiveNpcAI::handleBackHome();
	hpPhases->reset();
}

} // namespace aion::gameserver::handlers::ai::instance::raksang
