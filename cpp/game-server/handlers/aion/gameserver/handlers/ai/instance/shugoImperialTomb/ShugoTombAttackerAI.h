#pragma once

#include <array>
#include <cstdint>

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb {

/**
 * The AI of the Shugo Imperial Tomb's attackers ("shugo_tomb_attacker"): it does not think while it walks its route; at the route's last step it
 * stops walking, thinks, and hates the towers and monuments (831250, 831251, 831304, 831305, 831130) within 30 m, the nearest most (1,000,000,
 * then 50,000 less each); without one it is deleted. Not immune to abnormal states, no decay, no loot.
 * <p>
 * Java: data/handlers/ai/instance/shugoImperialTomb/ShugoTombAttackerAI.java, @AIName("shugo_tomb_attacker") (the marker is in the .cpp).
 *
 * @author Ritsu, Estrayl
 */
class ShugoTombAttackerAI : public GeneralNpcAI {
private:
	static constexpr std::array<int32_t, 5> npc_ids{831251, 831250, 831304, 831305, 831130}; // Java: List.of(...)
	runtime::AtomicBoolean isWalkerComplete{AION_LOCK_CLASS(ShugoTombAttackerAI::isWalkerComplete)}; // Java: = new AtomicBoolean()
	runtime::Field<bool> canThink_{true}; // Java: canThink = true (the name clashes with canThink())

public:
	explicit ShugoTombAttackerAI(Npc& owner) : GeneralNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleSpawned() override;

	void handleMoveArrived() override;

	void handleHate();

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb
