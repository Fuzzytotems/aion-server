#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

/**
 * The mage preceptor of Empyrean Crucible's stage 8 ("mage_preceptor"): her skills at 75, 50 and 25 %, two images (282363, 282364) at
 * 50 %, which go with her.
 * <p>
 * Java: data/handlers/ai/instance/empyreanCrucible/MagePreceptorAI.java, @AIName("mage_preceptor") (the marker is in the .cpp).
 *
 * @author Luzien
 */
class MagePreceptorAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(75, {50, 25}); // Java: = new HpPhases(75, 50, 25)

public:
	explicit MagePreceptorAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleDespawned() override;

	void handleDied() override;

	void handleBackHome() override;

	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void scheduleSkill(int32_t delay);

	runtime::Ptr<Creature> getRandomTarget();

	void despawnNpcs();

	void despawnNpc(runtime::Ptr<Npc> npc);
};

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
