#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::handlers::ai::instance::azoturanFortress {

/**
 * The AI of the betrayer Icaronix in Azoturan Fortress ("betrayericaronix"): at half its HP it leaves its place to Icaronix the Betrayer, who
 * comes in with half of his HP.
 * <p>
 * Java: data/handlers/ai/instance/azoturanFortress/BetrayerIcaronixAI.java, @AIName("betrayericaronix").
 *
 * @author Antraxx, Neon
 */
class BetrayerIcaronixAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(50); // Java: = new HpPhases(50)

public:
	explicit BetrayerIcaronixAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;
};

} // namespace aion::gameserver::handlers::ai::instance::azoturanFortress
