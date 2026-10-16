#include "aion/gameserver/handlers/ai/worlds/gelkmaros/PadmarashkaAI.h"

#include <cmath>
#include <numbers>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/observer/DeathObserver.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::ai::worlds::gelkmaros {

AION_AI(PadmarashkaAI, "padmarashka_world_boss");

// Java PadmarashkaAI.java:33-38
void PadmarashkaAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] { SkillEngine::getInstance().applyEffectDirectly(19186, getOwner(), getOwner()); }, 3000);
	spawnShieldNpcs();
}

// Java PadmarashkaAI.java:40-46
void PadmarashkaAI::modifyOwnerStat(Stat2& stat) {
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

// Java PadmarashkaAI.java:48-53
void PadmarashkaAI::spawnShieldNpcs() {
	spawnAndObserveNpc(281938, 2906.05f, 865.15f, 35.289f, int8_t{107});
	spawnAndObserveNpc(281939, 2920.70f, 878.94f, 35.289f, int8_t{94});
	spawnAndObserveNpc(281940, 2952.03f, 878.61f, 35.266f, int8_t{81});
	spawnAndObserveNpc(281941, 2963.97f, 859.07f, 35.289f, int8_t{69});
}

// Java PadmarashkaAI.java:55-59
void PadmarashkaAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java PadmarashkaAI.java:61-67
void PadmarashkaAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 33:
			spawnRockSlides();
			break;
		case 5:
			getOwner().queueSkill(18730, 1, 3000); // Berserk State
			break;
		default:
			break;
	}
}

// Java PadmarashkaAI.java:69-77
void PadmarashkaAI::spawnRockSlides() {
	for (int32_t i = 0; i < 360; i += 9) {
		float distance = static_cast<float>(commons::utils::Rnd::get(400, 700)) * 0.1f;
		double radian = static_cast<double>(i) * std::numbers::pi / 180.0; // Java: Math.toRadians
		float x = static_cast<float>(std::cos(radian) * distance);
		float y = static_cast<float>(std::sin(radian) * distance);
		spawn(281936, 2940.20f + x, 851.29f + y, 35.89f, int8_t{0});
	}
}

// Java PadmarashkaAI.java:79-83
void PadmarashkaAI::onEffectEnd(runtime::Ptr<Effect> effect) {
	if (effect->getSkillId() == 19186)
		PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_DF4_DRAMATA_AWAKENING());
}

// Java PadmarashkaAI.java:85-90
void PadmarashkaAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	hpPhases->reset();
	despawnNpcs({281936});
}

// Java PadmarashkaAI.java:92-96
void PadmarashkaAI::handleDespawned() {
	despawnNpcs({281938, 281939, 281940, 281941, 281936});
	AggressiveNpcAI::handleDespawned();
}

// Java PadmarashkaAI.java:98-100
void PadmarashkaAI::despawnNpcs(std::initializer_list<int32_t> npcIds) {
	for (const runtime::Ptr<Npc>& npc : getOwner().getWorldMapInstance()->getNpcs(npcIds))
		npc->getController().delete_();
}

// Java PadmarashkaAI.java:102-112
void PadmarashkaAI::handleObservedNpcDied(Npc& npc) {
	switch (npc.getNpcId()) {
		case 281938:
		case 281939:
		case 281940:
		case 281941:
			if (deadProtectors.incrementAndGet() >= 4)
				getOwner().getEffectController()->removeEffect(19186); // Protective Slumber
			break;
		default:
			break;
	}
}

// Java PadmarashkaAI.java:114-117. The DeathObserver's lambda pins this AI and the observed npc (the npc's despawn clears its observers).
void PadmarashkaAI::spawnAndObserveNpc(int32_t npcId, float x, float y, float z, int8_t h) {
	runtime::Ptr<Npc> npc = runtime::cast<Npc>(spawn(npcId, x, y, z, h));
	Npc& observed = *npc;
	observed.getObserveController()->attach(*DeathObserver::create(runtime::PinnedCallback<void(Creature&)>({this, &observed},
		[this, &observed](Creature&) { handleObservedNpcDied(observed); })));
}

} // namespace aion::gameserver::handlers::ai::worlds::gelkmaros
