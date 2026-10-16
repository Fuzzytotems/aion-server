#include "aion/gameserver/handlers/ai/instance/infinityShard/HyperionAI.h"

#include <cmath>
#include <numbers>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/model/geometry/Point3D.h"
#include "aion/gameserver/model/templates/BoundRadius.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::infinityShard {

AION_AI(HyperionAI, "hyperion");

// Java HyperionAI.java:36-46 (the field initializers of the two positions read the owner's map id)
HyperionAI::HyperionAI(Npc& owner)
	: AggressiveNpcAI(owner), northernSpawnPos(WorldPosition::create(getPosition()->getMapId(), 112.006f, 122.894f, 123.303f, int8_t{0})),
	  southernSpawnPos(WorldPosition::create(getPosition()->getMapId(), 148.127f, 150.346f, 123.729f, int8_t{0})) {
}

// Java HyperionAI.java:48-55
AttackTypeAnimation HyperionAI::getAttackTypeAnimation(Creature& target) {
	dist = PositionUtil::getDistance(getOwner(), target) - getObjectTemplate()->getBoundRadius()->getMaxOfFrontAndSide() -
		target.getObjectTemplate()->getBoundRadius()->getMaxOfFrontAndSide();
	if (dist.get() > 4) {
		return AttackTypeAnimation::RANGED;
	}
	return AttackTypeAnimation::MELEE;
}

// Java HyperionAI.java:57-61
void HyperionAI::handleAttack(runtime::Ptr<Creature> creature) {
	hpPhases->tryEnterNextPhase(*this);
	AggressiveNpcAI::handleAttack(creature);
}

// Java HyperionAI.java:64-74
float HyperionAI::modifyOwnerDamage(float damage, Creature& /*effected*/, runtime::Ptr<Effect> effect) {
	if (effect == nullptr) {
		if (dist.get() > 10) {
			return damage * 4.5f;
		} else if (dist.get() > 4) {
			return damage * 1.5f;
		}
	}
	return damage;
}

// Java HyperionAI.java:76-82
ItemAttackType HyperionAI::modifyAttackType(ItemAttackType /*type*/) {
	if (dist.get() > 4) {
		return ItemAttackType::MAGICAL_EARTH;
	}
	return ItemAttackType::PHYSICAL;
}

// Java HyperionAI.java:84-87 (Rnd.get(AttackHandAnimation.values()): one of the constants)
AttackHandAnimation HyperionAI::modifyAttackHandAnimation(AttackHandAnimation /*attackHandAnimation*/) {
	return static_cast<AttackHandAnimation>(commons::utils::Rnd::nextInt(static_cast<int32_t>(xml::EnumTraits<AttackHandAnimation>::names.size())));
}

// Java HyperionAI.java:89-108
void HyperionAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 100:
			queuePowerfulEnergyBlast();
			break;
		case 75:
			stage = static_cast<int8_t>(stage.get() + 1);
			spawnSummons(stage.get());
			spawnAncientTyrhund(2);
			break;
		case 80:
		case 67:
			spawnAncientTyrhund(2);
			break;
		case 55:
			spawnAncientTyrhund(3);
			break;
		case 45:
		case 30:
		case 17:
			spawnAncientTyrhund(4);
			break;
		case 65:
		case 50:
		case 25:
		case 20:
			stage = static_cast<int8_t>(stage.get() + 1);
			getOwner().queueSkill(21253, 56, 0);
			getOwner().queueSkill(21244, 56, 5000);
			break;
		case 40:
			stage = static_cast<int8_t>(stage.get() + 1);
			spawnSummons(stage.get());
			break;
		case 10:
			getOwner().queueSkill(21246, 56);
			break;
		default:
			break;
	}
}

// Java HyperionAI.java:110-135
void HyperionAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t /*skillLevel*/) {
	switch (skillTemplate->getSkillId()) {
		case 21253:
			switch (stage.get()) {
				case 1:
				case 2:
					spawnSummons(1);
					break;
				case 3:
				case 4:
					spawnSummons(2);
					break;
				case 5:
					spawnSummons(3);
					break;
				case 6:
					spawnSummons(4);
					break;
				default:
					break;
			}
			break;
		case 21246:
			scheduleLastPhase();
			break;
		default:
			break;
	}
}

// Java HyperionAI.java:137-144
void HyperionAI::scheduleLastPhase() {
	spawnTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		if (!isDead()) {
			spawnSummons(5);
			spawnAncientTyrhund(4);
		}
	}, 35000, 60000));
}

// Java HyperionAI.java:146-150
void HyperionAI::spawnAncientTyrhund(int32_t count) {
	for (int32_t i = 0; i < count; i++) {
		spawnWithWalker(231103, *getRndPos(*getPosition(), 12), std::nullopt);
	}
}

// Java HyperionAI.java:152-193
void HyperionAI::spawnSummons(int32_t spawnCase) {
	switch (spawnCase) {
		case 1:
			spawnWithWalker(0, *getRndPos(*northernSpawnPos, 3), "hyperionGuards1");
			spawnWithWalker(0, *getRndPos(*southernSpawnPos, 3), "hyperionGuards2");
			break;
		case 2:
			spawnWithWalker(0, *getRndPos(*northernSpawnPos, 3), "hyperionGuards1");
			spawnWithWalker(0, *getRndPos(*northernSpawnPos, 3), "hyperionGuards1");
			spawnWithWalker(0, *getRndPos(*southernSpawnPos, 3), "hyperionGuards2");
			spawnWithWalker(0, *getRndPos(*southernSpawnPos, 3), "hyperionGuards2");
			break;
		case 3:
			spawnWithWalker(0, *getRndPos(*northernSpawnPos, 3), "hyperionGuards1");
			spawnWithWalker(0, *getRndPos(*southernSpawnPos, 3), "hyperionGuards2");
			spawnWithWalker(0, *getRndPos(*southernSpawnPos, 3), "hyperionGuards2");
			break;
		case 4:
			spawnWithWalker(0, *getRndPos(*northernSpawnPos, 3), "hyperionGuards1");
			spawnWithWalker(0, *getRndPos(*northernSpawnPos, 3), "hyperionGuards1");
			spawnWithWalker(0, *getRndPos(*southernSpawnPos, 3), "hyperionGuards2");
			break;
		case 5:
			spawnWithWalker(0, *getRndPos(*northernSpawnPos, 3), "hyperionGuards1");
			spawnWithWalker(0, *getRndPos(*northernSpawnPos, 3), "hyperionGuards1");
			spawnWithWalker(0, *getRndPos(*northernSpawnPos, 3), "hyperionGuards1");
			spawnWithWalker(0, *getRndPos(*northernSpawnPos, 3), "hyperionGuards1");
			spawnWithWalker(0, *getRndPos(*southernSpawnPos, 3), "hyperionGuards2");
			spawnWithWalker(0, *getRndPos(*southernSpawnPos, 3), "hyperionGuards2");
			spawnWithWalker(0, *getRndPos(*southernSpawnPos, 3), "hyperionGuards2");
			spawnWithWalker(0, *getRndPos(*southernSpawnPos, 3), "hyperionGuards2");
			break;
		default:
			break;
	}
}

// Java HyperionAI.java:195-207 (the lambda pins this and the spawned npc)
void HyperionAI::spawnWithWalker(int32_t npcId, Point3D& p, std::optional<std::string> walkerId) {
	runtime::Ptr<Npc> npc = runtime::cast<Npc>(
		spawn(npcId == 0 ? *commons::utils::Rnd::getOptional(possibleSummons) : npcId, p.getX(), p.getY(), p.getZ(), int8_t{0}));
	if (walkerId.has_value()) {
		Npc& walker = *npc;
		ThreadPoolManager::getInstance().schedule({this, &walker}, [this, &walker, walkerId] {
			if (walker.isDead())
				return;
			walker.getSpawn()->setWalkerId(*walkerId);
			WalkManager::startWalking(dynamic_cast<NpcAI&>(walker.getAi())); // Java: (NpcAI) npc.getAi()
			walker.setState(CreatureState::ACTIVE, true);
			PacketSendUtility::broadcastToMap(getOwner(), SM_EMOTION(getOwner(), EmotionType::RUN));
		}, 2500);
	}
}

// Java HyperionAI.java:209-215
runtime::Ref<Point3D> HyperionAI::getRndPos(WorldPosition& p, float distanceMod) {
	double angleRadians = static_cast<double>(commons::utils::Rnd::nextFloat(360.0f)) / 180.0 * std::numbers::pi; // Java: Math.toRadians
	float distance = commons::utils::Rnd::nextFloat(distanceMod);
	float x1 = static_cast<float>(std::cos(angleRadians) * distance);
	float y1 = static_cast<float>(std::sin(angleRadians) * distance);
	return Point3D::create(p.getX() + x1, p.getY() + y1, p.getZ());
}

// Java HyperionAI.java:217-221
void HyperionAI::queuePowerfulEnergyBlast() {
	getOwner().queueSkill(21241, 56, 0);
	getOwner().queueSkill(21241, 56, 0);
	getOwner().queueSkill(21241, 56, 8000);
}

// Java HyperionAI.java:223-226
void HyperionAI::cancelSpawnTask() {
	runtime::FutureRef task = spawnTask.get();
	if (task != nullptr && !task->isCancelled())
		task->cancel(true);
}

// Java HyperionAI.java:228-231
int32_t HyperionAI::modifyInitialSkillDelay(int32_t /*delay*/) {
	return 0;
}

// Java HyperionAI.java:233-237
void HyperionAI::handleBackHome() {
	cancelSpawnTask();
	AggressiveNpcAI::handleBackHome();
}

// Java HyperionAI.java:239-243
void HyperionAI::handleDespawned() {
	cancelSpawnTask();
	AggressiveNpcAI::handleDespawned();
}

// Java HyperionAI.java:245-249
void HyperionAI::handleDied() {
	cancelSpawnTask();
	AggressiveNpcAI::handleDied();
}

} // namespace aion::gameserver::handlers::ai::instance::infinityShard
