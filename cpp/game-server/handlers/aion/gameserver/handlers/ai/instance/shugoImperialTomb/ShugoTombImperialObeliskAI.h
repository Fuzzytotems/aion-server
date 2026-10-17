#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb {

/**
 * The AI of the Shugo Imperial Tomb's obelisks ("shugo_tomb_imperial_obelisk"): it does not think; spawned it puts 21097 on itself, at 70 %
 * HP 21098, at 35 % 21099. No decay, respawn or rewards; immune to abnormal states.
 * <p>
 * Java: data/handlers/ai/instance/shugoImperialTomb/ShugoTombImperialObeliskAI.java, @AIName("shugo_tomb_imperial_obelisk") (the marker is in
 * the .cpp).
 *
 * @author Ritsu
 */
class ShugoTombImperialObeliskAI : public GeneralNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(70, {35}); // Java: = new HpPhases(70, 35)

public:
	explicit ShugoTombImperialObeliskAI(Npc& owner) : GeneralNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

protected:
	void handleSpawned() override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb
