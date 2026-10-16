#include "aion/gameserver/handlers/ai/instance/drakenspire/Lv1HumanBeritraAI.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numbers>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/DamageInfo.h"
#include "aion/gameserver/controllers/attack/DamageList.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/skill/NpcSkillList.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

AION_AI(Lv1HumanBeritraAI, "drakenspire_lv1_human_beritra");

const commons::logging::Logger& Lv1HumanBeritraAI::log() {
	static const auto logger = commons::logging::LoggerFactory::getLogger("INSTANCE_LOG");
	return logger;
}

// Java Lv1HumanBeritraAI.java:112-117
void Lv1HumanBeritraAI::handleFightStarted() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (isActivated.get()) // just in case he resets immediately
			spawnFactionHelpers(getAttackingPlayerRace() == Race::ELYOS ? std::vector<int32_t>{209736, 209737, 209737}
			                                                             : std::vector<int32_t>{209801, 209802, 209802});
	}, 10 * 1000); // 10, TimeUnit.SECONDS
}

// Java Lv1HumanBeritraAI.java:119-128
void Lv1HumanBeritraAI::handleCreatureAggro(Creature& creature) {
	AggressiveNoLootNpcAI::handleCreatureAggro(creature);
	if (isActivated.compareAndSet(false, true)) {
		fightStartTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		applyBuffs();
		scheduleNewSealGuardianSpawn();
		handleFightStarted(); // Only relevant for Lv1 Beritra
	}
}

// Java Lv1HumanBeritraAI.java:130-134
void Lv1HumanBeritraAI::applyBuffs() {
	SkillEngine::getInstance().applyEffectDirectly(21612, getOwner(), getOwner());
	SkillEngine::getInstance().applyEffectDirectly(21611, getOwner(), getOwner());
	SkillEngine::getInstance().applyEffectDirectly(21610, getOwner(), getOwner());
}

// Java Lv1HumanBeritraAI.java:136-139
void Lv1HumanBeritraAI::scheduleNewSealGuardianSpawn() {
	if (isActivated.get())
		spawnTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] { spawnSealGuardian(); }, 60 * 1000)); // 60, TimeUnit.SECONDS
}

// Java Lv1HumanBeritraAI.java:144-154
void Lv1HumanBeritraAI::spawnSealGuardian() {
	if (commons::utils::Rnd::chance() < 25) {
		spawn(855460, 128.621f, 461.719f, 1754.576f, int8_t{15});
	} else if (commons::utils::Rnd::chance() < 33) {
		spawn(855461, 207.780f, 496.081f, 1754.524f, int8_t{40});
	} else if (commons::utils::Rnd::chance() < 50) {
		spawn(855462, 208.671f, 542.410f, 1754.609f, int8_t{67});
	} else {
		spawn(855463, 127.028f, 574.691f, 1754.681f, int8_t{103});
	}
}

// Java Lv1HumanBeritraAI.java:156-162
void Lv1HumanBeritraAI::handleCustomEvent(int32_t eventId, std::span<const std::any> args) {
	switch (eventId) {
		case 1:
			scheduleNewSealGuardianSpawn();
			break;
		case 2:
			spawnSealGuardian();
			break;
	}
}

// Java Lv1HumanBeritraAI.java:164-172
void Lv1HumanBeritraAI::onEffectApplied(Effect& effect) {
	switch (effect.getSkillId()) {
		case 20834:
		case 20835:
			PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1501272); // You insects think you have a chance against me?
			handleBuffsRemovedByNpc();
			break;
	}
}

// Java Lv1HumanBeritraAI.java:177-185
void Lv1HumanBeritraAI::handleBuffsRemovedByNpc() {
	int32_t chainId = getOwner().getGameStats()->getLastSkill()->getNextChainId();
	runtime::Ptr<model::skill::NpcSkillEntry> entry;
	for (const runtime::Ptr<model::skill::NpcSkillEntry>& nse : getOwner().getSkillList()->getNpcSkills()->snapshot()) {
		if (nse->getChainId() == chainId) { // findAny
			entry = nse;
			break;
		}
	}

	getOwner().queueSkill(21604, 56, 0);
	getOwner().queueSkill(21603, 56, entry == nullptr ? -1 : 0);
	if (entry != nullptr)
		getOwner().queueSkill(*entry);
}

// Java Lv1HumanBeritraAI.java:194-226
void Lv1HumanBeritraAI::handlePulseWave() {
	// 855541
	runtime::Ref<Npc> skill1(*runtime::cast<Npc>(spawn(855742, 151.9f, 518.6f, 1749.6f, int8_t{0})));
	runtime::Ref<Npc> skill2(*runtime::cast<Npc>(spawn(855742, 151.9f, 518.6f, 1749.6f, int8_t{0})));
	runtime::Ref<Npc> skill3(*runtime::cast<Npc>(spawn(855742, 151.9f, 518.6f, 1749.6f, int8_t{0})));

	runtime::Ptr<Npc> slave1, slave2, slave3;
	if (commons::utils::Rnd::chance() < 25) {
		slave1 = runtime::cast<Npc>(spawn(855745, 136.6f, 496.7f, 1749.9f, int8_t{0})); // Pos 1
		slave2 = runtime::cast<Npc>(spawn(855747, 137.5f, 534.4f, 1749.9f, int8_t{0})); // Pos 3
		slave3 = runtime::cast<Npc>(spawn(855750, 180.9f, 515.4f, 1749.9f, int8_t{0})); // Pos 6
	} else if (commons::utils::Rnd::chance() < 33) {
		slave1 = runtime::cast<Npc>(spawn(855746, 129.6f, 516.2f, 1749.9f, int8_t{0})); // Pos 2
		slave2 = runtime::cast<Npc>(spawn(855749, 173.7f, 534.0f, 1749.9f, int8_t{0})); // Pos 5
		slave3 = runtime::cast<Npc>(spawn(855751, 174.3f, 498.2f, 1749.9f, int8_t{0})); // Pos 7
	} else if (commons::utils::Rnd::chance() < 50) {
		slave1 = runtime::cast<Npc>(spawn(855747, 137.5f, 534.4f, 1749.9f, int8_t{0})); // Pos 3
		slave2 = runtime::cast<Npc>(spawn(855749, 173.7f, 534.0f, 1749.9f, int8_t{0})); // Pos 5
		slave3 = runtime::cast<Npc>(spawn(855752, 156.4f, 490.2f, 1749.9f, int8_t{0})); // Pos 8
	} else {
		slave1 = runtime::cast<Npc>(spawn(855745, 136.6f, 496.7f, 1749.9f, int8_t{0})); // Pos 1
		slave2 = runtime::cast<Npc>(spawn(855748, 154.6f, 540.7f, 1749.9f, int8_t{0})); // Pos 4
		slave3 = runtime::cast<Npc>(spawn(855751, 174.3f, 498.2f, 1749.9f, int8_t{0})); // Pos 7
	}
	runtime::Ref<Npc> s1(*slave1), s2(*slave2), s3(*slave3);

	ThreadPoolManager::getInstance().schedule({this}, [this, skill1, skill2, skill3, s1, s2, s3] {
		int32_t skillId = getPulseWaveSkillId();
		SkillEngine::getInstance().getSkill(*skill1, skillId, 1, runtime::Ptr<VisibleObject>(s1))->useNoAnimationSkill();
		SkillEngine::getInstance().getSkill(*skill2, skillId, 1, runtime::Ptr<VisibleObject>(s2))->useNoAnimationSkill();
		SkillEngine::getInstance().getSkill(*skill3, skillId, 1, runtime::Ptr<VisibleObject>(s3))->useNoAnimationSkill();
	}, 500);
	ThreadPoolManager::getInstance().schedule({this}, [skill1, skill2, skill3, s1, s2, s3] {
		despawnNpcs({skill1, skill2, skill3, s1, s2, s3});
	}, 8000);
}

// Java Lv1HumanBeritraAI.java:228-230
int32_t Lv1HumanBeritraAI::getPulseWaveSkillId() {
	return getNpcId() == 236247 ? 21623 : 21828;
}

// Java Lv1HumanBeritraAI.java:235-241
void Lv1HumanBeritraAI::handleSoulExtinctionFields() {
	std::vector<runtime::Ptr<Creature>> playersInRange;
	for (const runtime::Ptr<Creature>& c : getAggroList().streamValidTargets(50))
		if (runtime::as<Player>(c) != nullptr)
			playersInRange.push_back(c);
	std::shuffle(playersInRange.begin(), playersInRange.end(), commons::utils::Rnd::generator()); // Java: Collections.shuffle
	size_t limit = 0;
	for (const runtime::Ptr<Creature>& p : playersInRange) {
		if (limit++ >= 3) // limit(3)
			break;
		spawn(855450, p->getX(), p->getY(), p->getZ(), int8_t{0});
	}
}

// Java Lv1HumanBeritraAI.java:249-263
void Lv1HumanBeritraAI::handleDimensionalWave() {
	// equals area_id="10"
	ThreadPoolManager::getInstance().schedule({this}, [this] { spawn(855435, 151.991f, 518.583f, 1749.5945f, int8_t{90}); }, 3000);
	// equals area_id="20"
	ThreadPoolManager::getInstance().schedule({this}, [this] { spawn(855435, 152.002f, 518.548f, 1749.5945f, int8_t{30}); }, 7500);
	// equals area_id="30"
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		spawn(856300, 126.977f, 574.736f, 1754.6809f, int8_t{0});
		spawn(856300, 208.552f, 542.472f, 1754.6082f, int8_t{0});
		spawn(856300, 177.031f, 458.650f, 1759.8838f, int8_t{0});
		spawn(856300, 176.057f, 579.624f, 1760.0452f, int8_t{0});
		spawn(856300, 207.733f, 496.071f, 1754.5236f, int8_t{0});
		spawn(856300, 128.722f, 461.584f, 1754.5775f, int8_t{0});
	}, 12000);
}

// Java Lv1HumanBeritraAI.java:265-274
void Lv1HumanBeritraAI::onStartUseSkill(const SkillTemplate* st, int32_t level) {
	switch (st->getSkillId()) {
		case 21601: // Pulse Wave
			handlePulseWave();
			addHateToRandomTarget();
			break;
		case 21602: // Dimensional Wave
			handleDimensionalWave();
			break;
	}
}

// Java Lv1HumanBeritraAI.java:276-278
void Lv1HumanBeritraAI::addHateToRandomTarget() {
	// Java: addHate(null, ...) is a no-op (AggroList.isAware(null) is false)
	if (runtime::Ptr<Creature> target = getAggroList().getTarget(AggroTarget::RANDOM); target != nullptr)
		getAggroList().addHate(*target, 100000);
}

// Java Lv1HumanBeritraAI.java:280-291
void Lv1HumanBeritraAI::onEndUseSkill(const SkillTemplate* st, int32_t level) {
	switch (st->getSkillId()) {
		case 21609: // Soul Extinction Field
			switch (level) {
				case 57:
					PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1501269); // You're not too bad for an insect! | Could also be 1501274
					break;
				case 58:
					PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1501270); // I'm not playing anymore! | Could also be 1501275
					break;
			}
			handleSoulExtinctionFields();
			break;
	}
}

// Java Lv1HumanBeritraAI.java:298-312
void Lv1HumanBeritraAI::spawnFactionHelpers(const std::vector<int32_t>& npcIds) {
	runtime::Ptr<SpawnTemplate> st = getSpawnTemplate();
	for (int32_t npcId : npcIds) {
		float distance = commons::utils::Rnd::nextFloat() * 5;
		double angleRadians = commons::utils::Rnd::nextFloat(360.0f) * std::numbers::pi / 180.0; // Math.toRadians
		float x = st->getX() + static_cast<float>(std::cos(angleRadians) * distance);
		float y = st->getY() + static_cast<float>(std::sin(angleRadians) * distance);
		runtime::Ref<Npc> helper(*runtime::cast<Npc>(spawn(npcId, x, y, st->getZ(), int8_t{0})));
		ThreadPoolManager::getInstance().schedule({this}, [this, helper] {
			helper->setTarget(runtime::Ptr<VisibleObject>(getOwner()));
			helper->getAggroList().addHate(getOwner(), 100000);
			helper->getMoveController()->moveToTargetObject();
		}, 500);
	}
}

// Java Lv1HumanBeritraAI.java:314-318
void Lv1HumanBeritraAI::despawnNpcs(std::initializer_list<runtime::Ptr<Npc>> npcs) {
	for (const runtime::Ptr<Npc>& npc : npcs)
		if (npc != nullptr)
			npc->getController().deleteIfAliveOrCancelRespawn();
}

// Java Lv1HumanBeritraAI.java:320-322
void Lv1HumanBeritraAI::despawnNpcsById(std::initializer_list<int32_t> npcIds) {
	for (const runtime::Ptr<Npc>& npc : getPosition()->getWorldMapInstance()->getNpcs(npcIds))
		npc->getController().deleteIfAliveOrCancelRespawn();
}

// Java Lv1HumanBeritraAI.java:324-327
bool Lv1HumanBeritraAI::isTargetInsideArena() {
	runtime::Ptr<SpawnTemplate> st = getSpawnTemplate();
	return PositionUtil::isInRange(*getTarget(), st->getX(), st->getY(), st->getZ(), 28);
}

// Java Lv1HumanBeritraAI.java:329-341
void Lv1HumanBeritraAI::handleTargetTooFar() {
	if (!isTargetInsideArena()) {
		getAggroList().stopHating(*getTarget());
		runtime::Ptr<Creature> mostHated = getAggroList().getTarget(AggroTarget::MOST_HATED);
		if (mostHated != nullptr)
			onCreatureEvent(gameserver::ai::event::AIEventType::TARGET_CHANGED, *mostHated);
		else
			onGeneralEvent(gameserver::ai::event::AIEventType::TARGET_GIVEUP);
		return;
	}
	AggressiveNoLootNpcAI::handleTargetTooFar();
}

// Java Lv1HumanBeritraAI.java:343-350
void Lv1HumanBeritraAI::handleBackHome() {
	AggressiveNoLootNpcAI::handleBackHome();
	despawnNpcsById({209734, 209735, 209736, 209737, 209799, 209800, 209801, 209802, 855446, 855460, 855461, 855462, 855463});
	cancelTasks();
	fightStartTime = 0;
	isActivated.set(false);
}

// Java Lv1HumanBeritraAI.java:352-357
void Lv1HumanBeritraAI::handleDied() {
	cancelTasks();
	logMetrics();
	AggressiveNoLootNpcAI::handleDied();
}

// Java Lv1HumanBeritraAI.java:359-365
void Lv1HumanBeritraAI::handleDespawned() {
	cancelTasks();
	isActivated.set(false);
	despawnNpcsById({209734, 209735, 209736, 209737, 209799, 209800, 209801, 209802, 855446, 855460, 855461, 855462, 855463});
	AggressiveNoLootNpcAI::handleDespawned();
}

// Java Lv1HumanBeritraAI.java:367-369
Race Lv1HumanBeritraAI::getAttackingPlayerRace() {
	for (const runtime::Ptr<Player>& p : getKnownList().streamPlayers())
		if (!p->isStaff()) // findAny
			return p->getRace();
	return Race::ELYOS;
}

// Java Lv1HumanBeritraAI.java:371-374
void Lv1HumanBeritraAI::cancelTasks() {
	if (runtime::FutureRef task = spawnTask.get(); task != nullptr && !task->isDone())
		task->cancel(true);
}

// Java Lv1HumanBeritraAI.java:376-385
void Lv1HumanBeritraAI::logMetrics() {
	const int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	int64_t fullFightTime = (now - fightStartTime.get()) / 1000;
	std::vector<controllers::attack::DamageInfo> damages = getAggroList().getFinalDamageList().getCreatureDamages();
	std::ranges::stable_sort(damages, [](const controllers::attack::DamageInfo& a, const controllers::attack::DamageInfo& b) {
		return a.getDamage() > b.getDamage(); // Comparator.comparingInt(DamageInfo::getDamage).reversed()
	});
	std::string damageDealt;
	for (const controllers::attack::DamageInfo& ai : damages) {
		if (!damageDealt.empty())
			damageDealt += ", ";
		damageDealt += ai.getAttacker()->getName() + " (ID: " + std::to_string(ai.getAttacker()->getObjectId()) + ", Dmg: " +
			std::to_string(ai.getDamage()) + ")";
	}

	log().info("[{}] {} (ID:{}) was killed in {}s. Damage List: {}", getPosition()->getWorldMapInstance()->getTemplate()->getName(),
		getOwner().getName(), getNpcId(), fullFightTime, damageDealt);
}

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
