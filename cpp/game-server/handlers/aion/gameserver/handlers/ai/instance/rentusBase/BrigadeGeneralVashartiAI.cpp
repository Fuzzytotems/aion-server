#include "aion/gameserver/handlers/ai/instance/rentusBase/BrigadeGeneralVashartiAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/manager/EmoteManager.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

AION_AI(BrigadeGeneralVashartiAI, "brigade_general_vasharti");

// Java BrigadeGeneralVashartiAI.java:45-55 (10 TimeUnit.MINUTES: 600000 ms)
void BrigadeGeneralVashartiAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (isHome.compareAndSet(true, false)) {
		getPosition()->getWorldMapInstance()->setDoorState(70, false);
		enrageSchedule.set(ThreadPoolManager::getInstance().schedule({this}, [this] { handleEnrageEvent(); }, 600000));
		scheduleFlameShieldBuffEvent(5000);
	}
	if (!isInFlameShowerEvent.get())
		hpPhases->tryEnterNextPhase(*this);
}

// Java BrigadeGeneralVashartiAI.java:57-62
void BrigadeGeneralVashartiAI::handleHpPhase(int32_t phaseHpPercent) {
	cancelTasks({flameShieldBuffSchedule.get()});
	getOwner().clearQueuedSkills();
	getOwner().queueSkill(20532, 1, 10000); // off (skill name)
}

// Java BrigadeGeneralVashartiAI.java:64-66
void BrigadeGeneralVashartiAI::scheduleFlameShieldBuffEvent(int32_t delay) {
	flameShieldBuffSchedule.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
		getOwner().queueSkill(20530 + commons::utils::Rnd::get(0, 1), 60);
	}, delay));
}

// Java BrigadeGeneralVashartiAI.java:68-72
void BrigadeGeneralVashartiAI::handleEnrageEvent() {
	getOwner().clearQueuedSkills();
	getOwner().queueSkill(19962, 1, 15000); // Purple Flame Weapon
	getOwner().queueSkill(19907, 1, 0); // Chastise
}

// Java BrigadeGeneralVashartiAI.java:74-87 (Java's (byte) 244 heading is -12)
void BrigadeGeneralVashartiAI::handleSeaOfFireEvent() {
	int32_t percent = getLifeStats()->getHpPercentage();
	int32_t npcId = percent <= 70 ? percent <= 40 ? 283012 : 283011 : 283010;

	spawn(npcId, 188.33f, 414.61f, 260.61f, static_cast<int8_t>(244)); // FX
	spawn(283007, 188.33f, 414.61f, 260.61f, int8_t{0}); // de-buff

	seaOfFireSpawnTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this, npcId] {
		int32_t smashCount = (npcId - 283007) * 5 + 1; // 15, 20, 25
		for (int32_t i = 2; i < smashCount; i++) {
			rndSpawnInRange(i % 2 == 0 ? 283008 : 283009, 0, 29);
		}
	}, 750, 7100));
}

// Java BrigadeGeneralVashartiAI.java:89-96
void BrigadeGeneralVashartiAI::onStartUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	switch (skillTemplate->getSkillId()) {
		case 20534:
			handleSeaOfFireEvent();
			break;
		default:
			break;
	}
}

// Java BrigadeGeneralVashartiAI.java:98-130
void BrigadeGeneralVashartiAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	switch (skillTemplate->getSkillId()) {
		case 19907: // repeat until reset
			getOwner().queueSkill(19907, 1, 0); // Chastise
			break;
		case 20530:
		case 20531: {
			runtime::Ptr<WorldMapInstance> instance = getPosition()->getWorldMapInstance();
			if (instance != nullptr) {
				if (instance->getNpc(283000) == nullptr)
					spawn(283000, 171.330f, 417.57f, 261.0f, int8_t{116});
				if (instance->getNpc(283001) == nullptr)
					spawn(283001, 205.280f, 410.53f, 261.0f, int8_t{56});
			}
			scheduleFlameShieldBuffEvent(33000);
			break;
		}
		case 20532:
			EmoteManager::emoteStopAttacking(getOwner());
			getOwner().clearQueuedSkills();
			ThreadPoolManager::getInstance().schedule({this}, [this] {
				WalkManager::startForcedWalking(*this, 188.17f, 414.06f, 260.75488f);
				getOwner().setState(CreatureState::ACTIVE, true);
				PacketSendUtility::broadcastPacket(getOwner(), SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getObjectId()));
			}, 800);
			break;
		case 20533:
			setStateIfNot(AIState::FIGHT);
			SkillEngine::getInstance().getSkill(getOwner(), 20534, 1, runtime::Ptr<VisibleObject>(getOwner()))->useSkill(); // Sea of Fire
			break;
		default:
			break;
	}

}

// Java BrigadeGeneralVashartiAI.java:132-148 (Java's switch reads the owner's npc id: ported as is; AggroList.addHate(null) does nothing)
void BrigadeGeneralVashartiAI::onEffectEnd(runtime::Ptr<Effect> effect) {
	if (effect != nullptr && effect->getSkillId() == 20534 && isInFlameShowerEvent.compareAndSet(true, false)) {
		cancelTasks({seaOfFireSpawnTask.get()});
		getKnownList().forEachNpc([this](Npc& n) {
			switch (getNpcId()) {
				case 283010:
				case 283011:
				case 283012:
					n.getController().delete_();
					break;
				default:
					break;
			}
		});
		scheduleFlameShieldBuffEvent(10000);
		runtime::Ptr<Creature> target = runtime::cast<Creature>(getTarget());
		if (target != nullptr)
			getOwner().getAggroList().addHate(*target, 1000);
	}
}

// Java BrigadeGeneralVashartiAI.java:150-157
bool BrigadeGeneralVashartiAI::isDestinationReached() {
	if (getState() == AIState::FORCED_WALKING && PositionUtil::getDistance(getOwner().getX(), getOwner().getY(), 188.17f, 414.06f) <= 1.0f
		&& isInFlameShowerEvent.compareAndSet(false, true)) {
		SkillEngine::getInstance().getSkill(getOwner(), 20533, 1, runtime::Ptr<VisibleObject>(getOwner()))->useSkill(); // off (skill name)
	}
	return AggressiveNpcAI::isDestinationReached();
}

// Java BrigadeGeneralVashartiAI.java:159-174
void BrigadeGeneralVashartiAI::clearSpawns() {
	runtime::Ptr<WorldMapInstance> instance = getPosition()->getWorldMapInstance();
	if (instance != nullptr) {
		deleteNpcs(instance->getNpcs({283002}));
		deleteNpcs(instance->getNpcs({283003}));
		deleteNpcs(instance->getNpcs({283004}));
		deleteNpcs(instance->getNpcs({283005}));
		deleteNpcs(instance->getNpcs({283006}));
		deleteNpcs(instance->getNpcs({283007}));
		deleteNpcs(instance->getNpcs({283010}));
		deleteNpcs(instance->getNpcs({283011}));
		deleteNpcs(instance->getNpcs({283012}));
		deleteNpcs(instance->getNpcs({283000}));
		deleteNpcs(instance->getNpcs({283001}));
	}
}

// Java BrigadeGeneralVashartiAI.java:176-178
void BrigadeGeneralVashartiAI::deleteNpcs(const std::vector<runtime::Ptr<Npc>>& npcs) {
	for (const runtime::Ptr<Npc>& npc : npcs) {
		if (npc != nullptr)
			npc->getController().delete_();
	}
}

// Java BrigadeGeneralVashartiAI.java:180-184
void BrigadeGeneralVashartiAI::cancelTasks(std::initializer_list<runtime::FutureRef> tasks) {
	for (const runtime::FutureRef& task : tasks)
		if (task != nullptr && !task->isCancelled())
			task->cancel(true);
}

// Java BrigadeGeneralVashartiAI.java:186-191
void BrigadeGeneralVashartiAI::handleDespawned() {
	cancelTasks({enrageSchedule.get(), flameShieldBuffSchedule.get(), seaOfFireSpawnTask.get()});
	clearSpawns();
	AggressiveNpcAI::handleDespawned();
}

// Java BrigadeGeneralVashartiAI.java:193-201
void BrigadeGeneralVashartiAI::handleBackHome() {
	isHome.set(true);
	getPosition()->getWorldMapInstance()->setDoorState(70, true);
	cancelTasks({enrageSchedule.get(), flameShieldBuffSchedule.get(), seaOfFireSpawnTask.get()});
	clearSpawns();
	AggressiveNpcAI::handleBackHome();
	hpPhases->reset();
}

// Java BrigadeGeneralVashartiAI.java:203-210
void BrigadeGeneralVashartiAI::handleDied() {
	getPosition()->getWorldMapInstance()->setDoorState(70, true);
	cancelTasks({enrageSchedule.get(), flameShieldBuffSchedule.get(), seaOfFireSpawnTask.get()});
	PacketSendUtility::broadcastMessage(getOwner(), 1500410);
	clearSpawns();
	AggressiveNpcAI::handleDied();
}

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
