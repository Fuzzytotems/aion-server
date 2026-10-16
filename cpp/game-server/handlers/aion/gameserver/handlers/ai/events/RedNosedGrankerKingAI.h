#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/OneDmgAI.h"

#include "aion/gameserver/ai/HpPhases.h"

namespace aion::gameserver::handlers::ai::events {

/**
 * The AI of the Red-Nosed Granker King ("rednosedgrankerking"): at half its hp the two kings call three helpers each; its death drops a
 * stolen box for each attacker within 50 m and removes the helpers.
 * <p>
 * Java: data/handlers/ai/events/RedNosedGrankerKingAI.java, @AIName("rednosedgrankerking") (the marker is in the .cpp).
 *
 * @author Tibald, Neon
 */
class RedNosedGrankerKingAI : public OneDmgAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(50); // Java: = new HpPhases(50)

public:
	explicit RedNosedGrankerKingAI(Npc& owner) : OneDmgAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

protected:
	void handleBackHome() override;

	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::events
