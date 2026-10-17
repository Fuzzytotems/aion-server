#include "aion/gameserver/handlers/ai/worlds/inggison/SematariuxAI.h"

#include <algorithm>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/controllers/observer/DeathObserver.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::worlds::inggison {

AION_AI(SematariuxAI, "sematariux");

// Java SematariuxAI.java:43-48
void SematariuxAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] { SkillEngine::getInstance().applyEffectDirectly(19186, getOwner(), getOwner()); }, 3000);
	spawnShieldNpcs();
}

// Java SematariuxAI.java:50-56
void SematariuxAI::modifyOwnerStat(Stat2& stat) {
	switch (stat.getStat()) { // Tweak for 12p (600 s | 3000 dps)
		case StatEnum::MAXHP:
			stat.setBase(20'880'000);
			break;
		case StatEnum::PHYSICAL_ATTACK:
			stat.setBase(2200);
			break;
		default:
			break;
	}
}

// Java SematariuxAI.java:58-69
void SematariuxAI::spawnShieldNpcs() {
	spawnAndObserveNpc(281931, *WorldPosition::create(210050000, 101.31f, 2136.59f, 441.456f, int8_t{0}));
	spawnAndObserveNpc(281931, *WorldPosition::create(210050000, 117.88f, 2097.47f, 440.581f, int8_t{0}));
	spawnAndObserveNpc(281931, *WorldPosition::create(210050000, 126.64f, 2175.42f, 441.662f, int8_t{0}));
	spawnAndObserveNpc(281931, *WorldPosition::create(210050000, 160.65f, 2099.39f, 439.625f, int8_t{0}));
	spawnAndObserveNpc(281931, *WorldPosition::create(210050000, 169.32f, 2175.65f, 441.708f, int8_t{0}));
	spawnAndObserveNpc(281931, *WorldPosition::create(210050000, 187.13f, 2137.05f, 441.185f, int8_t{0}));

	spawn(282123, 121.00f, 2119.01f, 441.626f, int8_t{0}, 1463);
	spawn(282123, 142.35f, 2110.25f, 441.213f, int8_t{0}, 1464);
	spawn(282123, 161.46f, 2119.23f, 441.356f, int8_t{0}, 1757);
}

// Java SematariuxAI.java:71-75
void SematariuxAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java SematariuxAI.java:77-83
void SematariuxAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 90:
		case 70:
		case 50:
		case 30:
		case 20:
		case 10:
			spawnTornado();
			break;
		case 5:
			getOwner().queueSkill(18730, 1, 3000); // Berserk State
			break;
		default:
			break;
	}
}

// Java SematariuxAI.java:85-94
bool SematariuxAI::isDestinationReached() {
	if (getState() == AIState::FORCED_WALKING) {
		if (PositionUtil::isInRange(getOwner(), 141.77f, 2142.31f, 440.0f, 4.0f) && isEggEventActive.compareAndSet(false, true)) {
			runtime::Ptr<Npc> egg = runtime::cast<Npc>(spawn(281451, 141.77f, 2142.31f, 440.0f, int8_t{0}));
			Npc& pinnedEgg = *egg; // Java: the lambda's capture of egg
			ThreadPoolManager::getInstance().schedule({this, &pinnedEgg}, [this, &pinnedEgg] {
				SkillEngine::getInstance().getSkill(getOwner(), 18726, 1, runtime::Ptr<VisibleObject>(pinnedEgg))->useNoAnimationSkill();
			}, 2500);
		}
	}
	return AggressiveNpcAI::isDestinationReached();
}

// Java SematariuxAI.java:96-119
void SematariuxAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t /*skillLevel*/) {
	switch (skillTemplate->getSkillId()) {
		case 18726: // Extract Essence
			isEggEventActive.set(false);
			setStateIfNot(AIState::FIGHT);
			getOwner().setTarget(getAggroList().getTarget(AggroTarget::MOST_HATED));
			getMoveController()->moveToTargetObject();
			break;
		case 19178: { // Deadly Bolt
			std::vector<runtime::Ptr<Creature>> targets; // Java: streamPlayers().filter(p -> !p.isDead() && isInRange(p, 80)).collect(toList())
			for (const runtime::Ptr<Player>& p : getKnownList().streamPlayers()) {
				if (!p->isDead() && isInRange(*p, 80))
					targets.push_back(p);
			}
			for (size_t maxTargetCount = static_cast<size_t>(std::max(1, static_cast<int32_t>(static_cast<float>(targets.size()) * 0.2f)));
				 targets.size() > maxTargetCount;) {
				runtime::Ptr<Creature>* removed = commons::utils::Rnd::get(targets); // Java: targets.remove(Rnd.get(targets))
				targets.erase(targets.begin() + (removed - targets.data()));
			}
			for (const runtime::Ptr<Creature>& target : targets)
				spawn(281452, target->getX(), target->getY(), target->getZ(), int8_t{0});
			break;
		}
		case 19182: // Ear Piercing Shriek
			// ThreadPoolManager.getInstance().schedule(() -> {
			// PacketSendUtility.broadcastPacket(getOwner(), SM_SYSTEM_MESSAGE.STR_MSG_LF4_DRAMATA_LAY_EGG());
			// WalkManager.startForcedWalking(this, 141.77f, 2142.31f, 440.0f);
			// }, 1500);
			break;
		default:
			break;
	}
}

// Java SematariuxAI.java:121-128
void SematariuxAI::onEffectEnd(runtime::Ptr<Effect> effect) {
	if (effect->getSkillId() == 19186) {
		// shieldRemovalStamp = System.currentTimeMillis();
		despawnNpcs({282123});
		PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_LF4_DRAMATA_AWAKENING());
	}
}

// Java SematariuxAI.java:130-140
void SematariuxAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	// if (System.currentTimeMillis() - shieldRemovalStamp >= TimeUnit.MINUTES.toMillis(5)) {
	// SkillEngine.getInstance().applyEffectDirectly(19186, getOwner(), getOwner());
	// deadThunderShields.set(0);
	// ThreadPoolManager.getInstance().schedule(this::spawnShieldNpcs, Rnd.get(120, 300), TimeUnit.MINUTES);
	// }
	hpPhases->reset();
	despawnNpcs({281453, 281451, 281931, 281932, 281933});
}

// Java SematariuxAI.java:142-146
void SematariuxAI::handleDied() {
	despawnNpcs({281453, 281451, 281931, 281932, 281933});
	AggressiveNpcAI::handleDied();
}

// Java SematariuxAI.java:148-152
void SematariuxAI::handleDespawned() {
	despawnNpcs({281453, 281451, 281931, 281932, 281933});
	AggressiveNpcAI::handleDespawned();
}

// Java SematariuxAI.java:154-157
void SematariuxAI::despawnNpcs(std::initializer_list<int32_t> npcIds) {
	for (const runtime::Ptr<Npc>& npc : getOwner().getWorldMapInstance()->getNpcs(npcIds))
		npc->getController().delete_();
}

// Java SematariuxAI.java:159-163
void SematariuxAI::spawnTornado() {
	runtime::Ptr<Creature> randomTarget = getAggroList().getTarget(AggroTarget::RANDOM, 80);
	if (randomTarget != nullptr)
		spawn(281453, randomTarget->getX(), randomTarget->getY(), randomTarget->getZ(), int8_t{0});
}

// Java SematariuxAI.java:165-180
void SematariuxAI::handleObservedNpcDied(Npc& npc) {
	switch (npc.getNpcId()) {
		case 281931:
			spawnAndObserveNpc(281932, *npc.getPosition());
			spawnAndObserveNpc(281932, *npc.getPosition());
			break;
		case 281932:
			spawnAndObserveNpc(281933, *npc.getPosition());
			spawnAndObserveNpc(281933, *npc.getPosition());
			break;
		case 281933:
			if (deadThunderShields.incrementAndGet() >= 24)
				getOwner().getEffectController()->removeEffect(19186); // Protective Slumber
			break;
		default:
			break;
	}
}

// Java SematariuxAI.java:182-185. The DeathObserver's lambda pins this AI and the observed npc (the npc's despawn clears its observers).
void SematariuxAI::spawnAndObserveNpc(int32_t npcId, WorldPosition& pos) {
	runtime::Ptr<Npc> npc = runtime::cast<Npc>(spawn(npcId, pos.getX() + static_cast<float>(commons::utils::Rnd::get(-5, 5)),
		pos.getY() + static_cast<float>(commons::utils::Rnd::get(-3, 3)), pos.getZ() + 0.4f, int8_t{0}));
	Npc& observed = *npc;
	observed.getObserveController()->attach(*DeathObserver::create(runtime::PinnedCallback<void(Creature&)>({this, &observed},
		[this, &observed](Creature&) { handleObservedNpcDied(observed); })));
}

} // namespace aion::gameserver::handlers::ai::worlds::inggison
