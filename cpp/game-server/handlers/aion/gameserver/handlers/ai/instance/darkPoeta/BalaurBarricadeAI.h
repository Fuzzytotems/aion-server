#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/OneDmgNoActionAI.h"
#include "aion/gameserver/ai/HpPhases.h"

namespace aion::gameserver::handlers::ai::instance::darkPoeta {

/**
 * Dark Poeta's Balaur barricades ("balaurbarricade"): two protectors at 50 % and two more at 10 %.
 * <p>
 * Java: data/handlers/ai/instance/darkPoeta/BalaurBarricadeAI.java, @AIName("balaurbarricade") (the marker is in the .cpp).
 *
 * @author Ritsu, Estrayl
 */
class BalaurBarricadeAI : public OneDmgNoActionAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(50, {10}); // Java: = new HpPhases(50, 10)

public:
	explicit BalaurBarricadeAI(Npc& owner) : OneDmgNoActionAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void spawnProtectors(bool isFirstSpawn);

protected:
	void handleBackHome() override;
};

} // namespace aion::gameserver::handlers::ai::instance::darkPoeta
