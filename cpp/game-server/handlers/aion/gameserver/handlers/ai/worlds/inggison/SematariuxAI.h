#pragma once

#include <initializer_list>

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::handlers::ai::worlds::inggison {

/**
 * The AI of Sematariux in Inggison ("sematariux"), the Dramata of Sematariux's Egg Chamber: 3 s after his spawn he sleeps under 19186
 * (Protective Slumber), guarded by six thunder shields (281931) that split in two twice when they die (281932, then 281933) and three
 * 282123; when the 24th of the last shields dies the slumber ends, which takes the 282123 and tells the map STR_MSG_LF4_DRAMATA_AWAKENING.
 * His HP phases at 90, 70, 50, 30, 20 and 10 % call a tornado (281453) on a random hated player within 80 m, at 5 % he queues 18730
 * (Berserk State). Deadly Bolt (19178) marks a fifth (at least one) of the living players within 80 m with 281452. Forced walking to his
 * nest spawns an egg (281451), 2.5 s later Extract Essence (18726) on it; its end sends him back at his most hated. Back home, dead or
 * despawned he takes his spawns away (back home also resets his phases). 20,880,000 max HP and 2200 physical attack.
 * <p>
 * Java: data/handlers/ai/worlds/inggison/SematariuxAI.java, @AIName("sematariux") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class SematariuxAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(90, {70, 50, 30, 20, 10, 5}); // Java: = new HpPhases(90, 70, 50, 30, 20, 10, 5)
	runtime::AtomicInteger deadThunderShields{AION_LOCK_CLASS(SematariuxAI::deadThunderShields)}; // Java: = new AtomicInteger()
	runtime::AtomicBoolean isEggEventActive{AION_LOCK_CLASS(SematariuxAI::isEggEventActive)}; // Java: = new AtomicBoolean()

public:
	explicit SematariuxAI(Npc& owner) : AggressiveNpcAI(owner) {}

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

	bool isDestinationReached() override;

	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

	void onEffectEnd(runtime::Ptr<Effect> effect) override;

protected:
	void handleBackHome() override;

	void handleDied() override;

	void handleDespawned() override;

private:
	void despawnNpcs(std::initializer_list<int32_t> npcIds);

	void spawnTornado();

	void handleObservedNpcDied(Npc& npc);

	void spawnAndObserveNpc(int32_t npcId, WorldPosition& pos);
};

} // namespace aion::gameserver::handlers::ai::worlds::inggison
