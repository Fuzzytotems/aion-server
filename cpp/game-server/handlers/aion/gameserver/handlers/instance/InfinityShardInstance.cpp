#include "aion/gameserver/handlers/instance/InfinityShardInstance.h"

#include <limits>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(InfinityShardInstance, 300800000);

// Java InfinityShardInstance.java:32-34
InfinityShardInstance::InfinityShardInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<InfinityShardInstance> InfinityShardInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<InfinityShardInstance>(instance);
}

// Java InfinityShardInstance.java:36-73
void InfinityShardInstance::onDie(Npc& npc) {
	GeneralInstanceHandler::onDie(npc);
	switch (npc.getNpcId()) {
		case 231083: // Hyperion Defense Elite Assassin
			removeIdeAethericFieldEffect(231074); // Ide Forcefield Generator
			break;
		case 231087: // Hyperion Defense Elite Magus
			removeIdeAethericFieldEffect(231078); // Ide Forcefield Generator
			break;
		case 231079: // Hyperion Defense Elite Assassin
			removeIdeAethericFieldEffect(231082); // Ide Forcefield Generator
			break;
		case 231075: // Agent Kabalash
			removeIdeAethericFieldEffect(231086); // Ide Forcefield Generator
			break;
		case 231074:
		case 231078:
		case 231082:
		case 231086:
			npc.getController().delete_();
			checkGeneratorState();
			break;
		case 231092:
		case 231093:
		case 231094:
		case 231095:
		case 231102:
			// the lambda pins this and the dead npc, whose id and place it reads when it runs
			tasks.add(ThreadPoolManager::getInstance().schedule({this, &npc},
				[this, &npc] { spawn(npc.getNpcId(), npc.getX(), npc.getY(), npc.getZ(), int8_t{0}); }, commons::utils::Rnd::get(29000, 31000)));
			break;
		case 231073: // Hyperion
			cancelTasks();
			despawnAllAndSpawnExit();
			// rewardGP();
			break;
		default:
			break;
	}
}

// Java InfinityShardInstance.java:75-79
void InfinityShardInstance::removeIdeAethericFieldEffect(int32_t npcId) {
	runtime::Ptr<Npc> npc = instance->getNpc(npcId);
	if (npc != nullptr)
		npc->getEffectController()->removeEffect(21371);
}

// Java InfinityShardInstance.java:81-89
void InfinityShardInstance::onBackHome(Npc& npc) {
	if (npc.getNpcId() == 231073) { // hyperion
		if (isRunning.getAndSet(false)) {
			cancelTasks();
			despawnAllAndSpawnExit();
		}
	}
}

// Java InfinityShardInstance.java:91-104
void InfinityShardInstance::onStartEffect(runtime::Ptr<Effect> effect) {
	runtime::Ptr<Creature> effected = effect->getEffected();
	runtime::Ptr<Npc> effectedNpc = runtime::as<Npc>(effected);
	if (effectedNpc != nullptr && effectedNpc->getNpcId() == 231073)
		switch (effect->getSkillId()) {
			case 21258:
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDRUNEWP_CHARGER1_COMPLETED());
				break;
			case 21382:
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDRUNEWP_CHARGER2_COMPLETED());
				break;
			case 21384:
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDRUNEWP_CHARGER3_COMPLETED());
				break;
			case 21416:
				sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDRUNEWP_CHARGER4_COMPLETED());
				ThreadPoolManager::getInstance().schedule({this}, [this] { failInstance(true); }, 12000);
				break;
			default:
				break;
		}
}

// Java InfinityShardInstance.java:106-122
void InfinityShardInstance::checkGeneratorState() {
	for (int32_t id = 231074; id <= 231086; id += 4) {
		if (instance->getNpc(id) != nullptr) {
			sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDRUNEWP_BROKENPROTECTION());
			return;
		}
	}
	sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDRUNEWP_BROKENPROTECTIONALL());
	deleteAliveNpcs({730741}); // remove barrier in center
	instance->getNpc(231073)->getEffectController()->removeEffect(21254);
	startHatingNearestPlayer();
	SkillEngine::getInstance().getSkill(*instance->getNpc(231073), 21255, 56, runtime::Ptr<VisibleObject>(instance->getNpc(231073)))->useWithoutPropSkill();
	spawnResonators();
	spawnTurrets();
	startFailTimer();
	sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDRUNEWP_CHARGING());
}

// Java InfinityShardInstance.java:124-140
void InfinityShardInstance::startHatingNearestPlayer() {
	runtime::Ptr<Npc> hyperion = instance->getNpc(231073);
	runtime::Ptr<Player> nearest = nullptr;
	double dist = std::numeric_limits<double>::max();
	for (const runtime::Ptr<Player>& p : instance->getPlayersInside()) {
		if (!p->isDead()) {
			double locDist = PositionUtil::getDistance(*hyperion, *p, false);
			if (locDist < dist) {
				dist = locDist;
				nearest = p;
			}
		}
	}
	if (nearest != nullptr) {
		hyperion->getAi().onCreatureEvent(AIEventType::CREATURE_AGGRO, *nearest);
	}
}

// Java InfinityShardInstance.java:142-149
void InfinityShardInstance::startFailTimer() {
	tasks.add(ThreadPoolManager::getInstance().schedule({this}, [this] {
		runtime::Ptr<Npc> hyperion = instance->getNpc(231073);
		if (hyperion != nullptr && !hyperion->isDead()) {
			failInstance(false);
		}
	}, 20 * 60 * 1000)); // 20 min
}

// Java InfinityShardInstance.java:151-161
void InfinityShardInstance::failInstance(bool wipePlayers) {
	if (isRunning.getAndSet(false)) {
		cancelTasks();
		deleteAliveNpcs({231073});
		sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDRUNEWP_USER_KILL());
		if (wipePlayers)
			for (const runtime::Ptr<Npc>& npc : instance->getNpcs({231104}))
				npc->getController().useSkill(21199);
		ThreadPoolManager::getInstance().schedule(this, &InfinityShardInstance::despawnAllAndSpawnExit, 1500); // delay despawn for invisible NPCs to use the skill
	}
}

// Java InfinityShardInstance.java:163-168
void InfinityShardInstance::spawnResonators() {
	spawn(231092, 108.55013f, 138.96948f, 132.60164f, int8_t{0});
	ThreadPoolManager::getInstance().schedule({this}, [this] { spawn(231093, 126.5471f, 154.47961f, 131.47116f, int8_t{0}); }, 10000);
	ThreadPoolManager::getInstance().schedule({this}, [this] { spawn(231094, 146.72455f, 139.12267f, 132.68515f, int8_t{0}); }, 20000);
	ThreadPoolManager::getInstance().schedule({this}, [this] { spawn(231095, 129.41306f, 121.34766f, 131.47116f, int8_t{0}); }, 30000);
}

// Java InfinityShardInstance.java:170-175
void InfinityShardInstance::spawnTurrets() {
	ThreadPoolManager::getInstance().schedule({this}, [this] { spawn(231102, 107.53553f, 142.51953f, 127.03997f, int8_t{0}); },
		commons::utils::Rnd::get(30000, 45000));
	ThreadPoolManager::getInstance().schedule({this}, [this] { spawn(231102, 113.86417f, 154.06656f, 127.68255f, int8_t{110}); },
		commons::utils::Rnd::get(30000, 45000));
	ThreadPoolManager::getInstance().schedule({this}, [this] { spawn(231102, 144.52719f, 122.26577f, 127.44639f, int8_t{45}); },
		commons::utils::Rnd::get(30000, 45000));
	ThreadPoolManager::getInstance().schedule({this}, [this] { spawn(231102, 150.33377f, 132.67754f, 126.57981f, int8_t{50}); },
		commons::utils::Rnd::get(30000, 45000));
}

// Java InfinityShardInstance.java:177-180
void InfinityShardInstance::despawnAllAndSpawnExit() {
	instance->forEachNpc([](Npc& npc) { npc.getController().deleteIfAliveOrCancelRespawn(); });
	spawn(730842, 146.65f, 135.33f, 112.18f, int8_t{59}); // exit portal
}

// Java InfinityShardInstance.java:192-196
void InfinityShardInstance::cancelTasks() {
	for (const auto& task : tasks)
		if (task != nullptr && !task->isCancelled())
			task->cancel(true);
}

// Java InfinityShardInstance.java:198-204
void InfinityShardInstance::onPlayerLogout(Player& player) {
	GeneralInstanceHandler::onPlayerLogout(player);
	if (player.isDead()) {
		TeleportService::moveToBindLocation(player);
	}
}

// Java InfinityShardInstance.java:206-209
void InfinityShardInstance::onInstanceDestroy() {
	cancelTasks();
}

// Java InfinityShardInstance.java:211-214
bool InfinityShardInstance::isBoss(Npc& npc) {
	return npc.getNpcId() == 231073; // Hyperion
}

} // namespace aion::gameserver::handlers::instance
