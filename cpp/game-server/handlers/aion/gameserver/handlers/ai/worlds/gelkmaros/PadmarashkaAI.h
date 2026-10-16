#pragma once

#include <initializer_list>

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::handlers::ai::worlds::gelkmaros {

/**
 * The AI of Padmarashka in Gelkmaros ("padmarashka_world_boss"), the Dramata of Padmarashka's Cave: 3 s after her spawn she sleeps under 19186
 * (Protective Slumber), guarded by four protectors (281938-281941); when all four died the slumber ends and the map is told
 * STR_MSG_DF4_DRAMATA_AWAKENING. At 33 % HP forty rock slides (281936) fall in a ring of 40 to 70 m around (2940.2, 851.29), at 5 % she queues
 * 18730 (Berserk State). Back home she resets her phases and clears the rock slides; despawned she clears the protectors and the rock
 * slides. 20,880,000 max HP and 2200 physical attack.
 * <p>
 * Java: data/handlers/ai/worlds/gelkmaros/PadmarashkaAI.java, @AIName("padmarashka_world_boss") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class PadmarashkaAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(33, {5}); // Java: = new HpPhases(33, 5)
	runtime::AtomicInteger deadProtectors{AION_LOCK_CLASS(PadmarashkaAI::deadProtectors)}; // Java: = new AtomicInteger()

public:
	explicit PadmarashkaAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

public:
	void modifyOwnerStat(Stat2& stat) override;

private:
	void spawnShieldNpcs();

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void spawnRockSlides();

public:
	void onEffectEnd(runtime::Ptr<Effect> effect) override;

protected:
	void handleBackHome() override;

	void handleDespawned() override;

private:
	void despawnNpcs(std::initializer_list<int32_t> npcIds);

	void handleObservedNpcDied(Npc& npc);

	void spawnAndObserveNpc(int32_t npcId, float x, float y, float z, int8_t h);
};

} // namespace aion::gameserver::handlers::ai::worlds::gelkmaros
