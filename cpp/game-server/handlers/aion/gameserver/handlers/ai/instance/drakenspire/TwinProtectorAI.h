#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNoLootNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

/**
 * The AI of the twin protectors ("twin_protector"): at 65, 40 and 15 % hp it queues Raging Hellfire (21644), at 25 and 10 % adds
 * (855622 for an even npc id, else 855621) on up to 1 or 3 targets; after the hellfire 21645 and 855625 adds; home or gone it takes its
 * adds.
 * <p>
 * Java: data/handlers/ai/instance/drakenspire/TwinProtectorAI.java, @AIName("twin_protector") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class TwinProtectorAI : public AggressiveNoLootNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	// Java: = new HpPhases(65, 40, 25, 15, 10)
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(65, {40, 25, 15, 10});
	runtime::ArrayList<runtime::Ref<Npc>> adds{AION_LOCK_CLASS(TwinProtectorAI::adds)}; // Java: = new ArrayList<>()

public:
	explicit TwinProtectorAI(Npc& owner) : AggressiveNoLootNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

private:
	void spawnAdds(int32_t npcId, int32_t hpThreshold);

	void despawnAdds();

protected:
	void handleDespawned() override;

	void handleBackHome() override;
};

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
