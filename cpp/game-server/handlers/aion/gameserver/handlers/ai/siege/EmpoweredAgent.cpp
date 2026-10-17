#include "aion/gameserver/handlers/ai/siege/EmpoweredAgent.h"

#include <cmath>
#include <numbers>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/manager/EmoteManager.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroInfo.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/geometry/Point3D.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"
#include "aion/gameserver/model/templates/walker/RouteStep.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/siege/AgentSiege.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::siege {

AION_AI(EmpoweredAgent, "empowered_agent");

// Java EmpoweredAgent.java:58-61
bool EmpoweredAgent::canThink() {
	return canThink_.get();
}

// Java EmpoweredAgent.java:63-79
void EmpoweredAgent::handleSpawned() {
	AbstractSiegeProtectorAI::handleSpawned();
	runtime::Ref<skillengine::model::Skill> shield = SkillEngine::getInstance().getSkill(getOwner(), 21779, 1, runtime::Ptr<VisibleObject>(getOwner()));
	if (shield == nullptr) // Java: getSkill(...).useNoAnimationSkill() on null
		throw runtime::NullPointerException("EmpoweredAgent: skill 21779");
	shield->useNoAnimationSkill();
	canThink_ = false;
	EmoteManager::emoteStopAttacking(getOwner());
	switch (getOwner().getNpcId()) {
		case 235064:
			for (int32_t id : {235334, 235335, 235336, 235337, 235338, 235339})
				guardIds.add(id); // Java: Collections.addAll
			startWalking("600100000_npcpathgod_l");
			break;
		case 235065:
			for (int32_t id : {235340, 235341, 235342, 235343, 235344, 235345})
				guardIds.add(id);
			startWalking("600100000_npcpathgod_d");
			break;
		default:
			break;
	}
}

// Java EmpoweredAgent.java:81-86
void EmpoweredAgent::startWalking(std::string_view walkerId) {
	getOwner().getSpawn()->setWalkerId(walkerId);
	WalkManager::startWalking(*this);
	getOwner().setState(CreatureState::WALK_MODE);
	PacketSendUtility::broadcastPacket(getOwner(), network::aion::serverpackets::SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getObjectId()));
}

// Java EmpoweredAgent.java:88-92
void EmpoweredAgent::handleAttack(runtime::Ptr<Creature> creature) {
	AbstractSiegeProtectorAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java EmpoweredAgent.java:94-100
void EmpoweredAgent::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 80:
		case 70:
		case 60:
		case 50:
		case 40:
		case 30:
		case 20:
			onGuardSpawnEvent();
			break;
		case 25:
		case 5:
			getOwner().queueSkill(21778, 1, 3000, NpcSkillTargetAttribute::ME);
			break;
		default:
			break;
	}
}

// Java EmpoweredAgent.java:102-107 (getCurrentStep() on null is Java's NullPointerException)
void EmpoweredAgent::handleMoveArrived() {
	AbstractSiegeProtectorAI::handleMoveArrived();
	const model::templates::walker::RouteStep* step = getOwner().getMoveController()->getCurrentStep();
	if (step == nullptr)
		throw runtime::NullPointerException("EmpoweredAgent: no current route step");
	if (step->isLastStep() && isDestinationReached() && isWalkingCompleted.compareAndSet(false, true))
		onDestinationArrived();
}

// Java EmpoweredAgent.java:109-114
void EmpoweredAgent::onDestinationArrived() {
	getSpawnTemplate()->setWalkerId(std::nullopt);
	WalkManager::stopWalking(*this);
	spawnFlag();
	tryActivating();
}

// Java EmpoweredAgent.java:116-126
void EmpoweredAgent::tryActivating() {
	runtime::Ptr<Npc> otherAgent = getOtherAgent();
	if (otherAgent != nullptr && PositionUtil::getDistance(getOwner(), *otherAgent, false) <= 5) {
		getOwner().getEffectController()->removeEffect(21779);
		getOwner().getLifeStats()->setCurrentHpPercent(100);
		getAggroList().addHate(*otherAgent, 100'000'000);
		startFight(*otherAgent);
	} else {
		activationTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] { tryActivating(); }, 5000));
	}
}

// Java EmpoweredAgent.java:128-134
runtime::Ptr<Npc> EmpoweredAgent::getOtherAgent() {
	switch (getOwner().getNpcId()) {
		case 235064:
			return getOwner().getPosition()->getWorldMapInstance()->getNpc(235065); // Veille
		case 235065:
			return getOwner().getPosition()->getWorldMapInstance()->getNpc(235064); // Mastarius
		default:
			return nullptr;
	}
}

// Java EmpoweredAgent.java:136-146
void EmpoweredAgent::startFight(Npc& otherAgent) {
	canThink_ = true;
	getOwner().getGameStats()->resetFightStats();
	getOwner().setTarget(runtime::Ptr<VisibleObject>(otherAgent));
	setStateIfNot(AIState::FIGHT);
	think();
	getOwner().setState(CreatureState::ACTIVE, true);
	EmoteManager::emoteStartAttacking(getOwner(), otherAgent);
	PacketSendUtility::broadcastPacket(getOwner(),
		network::aion::serverpackets::SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getOwner().getObjectId()));
	aggroResetTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { resetPlayerHate(); }, 10000, 15000));
}

// Java EmpoweredAgent.java:148-150
void EmpoweredAgent::resetPlayerHate() {
	for (const runtime::Ptr<AggroInfo>& info : getAggroList().stream()) {
		runtime::Ptr<Creature> attacker = info->getAttacker();
		if (attacker != nullptr && runtime::as<Player>(attacker->getMaster()) != nullptr)
			info->setHate(0);
	}
}

// Java EmpoweredAgent.java:152-166
void EmpoweredAgent::spawnFlag() {
	if (flagNpc.get() != nullptr) {
		commons::logging::LoggerFactory::getLogger("EmpoweredAgent")
			.warn("Tried to spawn flag for empowered agent " + std::to_string(getNpcId()) + " twice!", runtime::Exception("stack"));
		return;
	}
	int32_t flagNpcId = 0;
	switch (getNpcId()) {
		case 235064:
			flagNpcId = 832830; // Veille
			break;
		case 235065:
			flagNpcId = 832831; // Mastarius
			break;
		default:
			flagNpcId = 0;
			break;
	}
	if (flagNpcId != 0) {
		runtime::Ptr<world::WorldPosition> pos = getPosition();
		flagNpc.set(runtime::Ref<Npc>(runtime::cast<Npc>(spawn(flagNpcId, pos->getX(), pos->getY(), pos->getZ(), pos->getHeading()))));
	}
}

// Java EmpoweredAgent.java:168-173
void EmpoweredAgent::despawnFlag() {
	runtime::Ref<Npc> flag = flagNpc.get();
	if (flag != nullptr) {
		flag->getController().delete_();
		flagNpc.set(nullptr);
	}
}

// Java EmpoweredAgent.java:175-186
void EmpoweredAgent::onGuardSpawnEvent() {
	int32_t worldId = getOwner().getWorldId();
	int64_t hating = 0; // Java: getAggroList().stream().filter(aggroInfo -> aggroInfo.getHate() > 0).count()
	for (const runtime::Ptr<AggroInfo>& aggroInfo : getAggroList().stream())
		if (aggroInfo->getHate() > 0)
			hating++;
	float guardAmount = static_cast<float>(hating) / 2.5f;
	if (guardAmount < 6)
		guardAmount = 6;
	for (int32_t i = 0; i < guardAmount; i++) {
		runtime::Ref<Point3D> pos = getRndPos();
		runtime::Ref<SiegeSpawnTemplate> template_ = SpawnEngine::newSiegeSpawn(worldId, commons::utils::Rnd::get(guardIds.snapshot()),
			getSpawnTemplate()->getSiegeId(), SiegeRace::BALAUR, SiegeModType::SIEGE, pos->getX(), pos->getY(), pos->getZ(), int8_t{0});
		SpawnEngine::spawnObject(*template_, 1);
	}
}

// Java EmpoweredAgent.java:188-195
runtime::Ref<Point3D> EmpoweredAgent::getRndPos() {
	double angleRadians = commons::utils::Rnd::nextFloat(360.0f) * std::numbers::pi / 180.0; // Java: Math.toRadians
	float distance = commons::utils::Rnd::nextFloat(10.0f);
	float x1 = static_cast<float>(std::cos(angleRadians) * distance);
	float y1 = static_cast<float>(std::sin(angleRadians) * distance);
	runtime::Ptr<world::WorldPosition> p = getPosition();
	return Point3D::create(p->getX() + x1, p->getY() + y1, p->getZ());
}

// Java EmpoweredAgent.java:197-202
void EmpoweredAgent::cancelActivationTask() {
	runtime::FutureRef activation = activationTask.get();
	if (activation != nullptr && !activation->isDone())
		activation->cancel(true);
	runtime::FutureRef aggroReset = aggroResetTask.get();
	if (aggroReset != nullptr && !aggroReset->isDone())
		aggroReset->cancel(true);
}

// Java EmpoweredAgent.java:204-210 (Java's cast (AgentSiege) getSiege(); a null siege is its NullPointerException)
void EmpoweredAgent::handleDied() {
	despawnFlag();
	cancelActivationTask();
	runtime::Ptr<AgentSiege> siege = runtime::cast<AgentSiege>(getSiege());
	if (siege == nullptr)
		throw runtime::NullPointerException("EmpoweredAgent: no AgentSiege");
	siege->setWinnerRace(getObjectTemplate()->getRace() == Race::GHENCHMAN_LIGHT ? SiegeRace::ASMODIANS : SiegeRace::ELYOS);
	AbstractSiegeProtectorAI::handleDied();
}

// Java EmpoweredAgent.java:212-217
void EmpoweredAgent::handleDespawned() {
	despawnFlag();
	cancelActivationTask();
	AbstractSiegeProtectorAI::handleDespawned();
}

// Java EmpoweredAgent.java:219-225
bool EmpoweredAgent::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REMOVE_EFFECTS_ON_MAP_REGION_DEACTIVATE:
			return false;
		default:
			return AbstractSiegeProtectorAI::ask(question);
	}
}

// Java EmpoweredAgent.java:227-231
void EmpoweredAgent::modifyOwnerStat(Stat2& stat) {
	if (stat.getStat() == StatEnum::MAXHP)
		stat.setBaseRate(SiegeConfig::FORTRESS_PROTECTOR_HEALTH_MULTIPLIER.load());
}

} // namespace aion::gameserver::handlers::ai::siege
