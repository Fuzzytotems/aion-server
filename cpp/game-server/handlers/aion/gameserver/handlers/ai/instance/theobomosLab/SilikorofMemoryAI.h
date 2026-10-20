#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::handlers::ai::instance::theobomosLab {

/**
 * The AI of Silikor of Memory in Theobomos Lab ("silikor"): Silikor (214668) casts 18481 on himself when he spawns; at 50, 25 and 10 % HP a
 * 281054 and a 281053 appear up to 2 m from him; his death takes them away, back home the phases start over.
 * <p>
 * Java: data/handlers/ai/instance/theobomosLab/SilikorofMemoryAI.java, @AIName("silikor") (the marker is in the .cpp).
 *
 * @author Ritsu
 */
class SilikorofMemoryAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(50, {25, 10}); // Java: = new HpPhases(50, 25, 10)

public:
	explicit SilikorofMemoryAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void sp(int32_t npcId);

protected:
	void handleSpawned() override;

	void handleBackHome() override;

	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::theobomosLab
