#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

/**
 * The AI of Engineer Lahulahu on the Steel Rake ("engineerlahulahu"): at 95 % HP he finds his twelve devices (281108-281114, 281322-281326),
 * casts 18131 and starts a round: every 10 s, while he is spawned, alive and past his first phase, one or two random devices cast 18153 on
 * themselves. At 25 % he swaps 18131 for 18132. Back home his phases start over.
 * <p>
 * Java: data/handlers/ai/instance/rakes/EngineerLahulahuAI.java, @AIName("engineerlahulahu") (the marker is in the .cpp).
 *
 * @author xTz
 */
class EngineerLahulahuAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(95, {25}); // Java: = new HpPhases(95, 25)
	runtime::Field<int32_t> skill{18153}; // Java: = 18153
	runtime::Field<runtime::Ref<Npc>> npc{};
	runtime::Field<runtime::Ref<Npc>> npc1{};
	runtime::Field<runtime::Ref<Npc>> npc2{};
	runtime::Field<runtime::Ref<Npc>> npc3{};
	runtime::Field<runtime::Ref<Npc>> npc4{};
	runtime::Field<runtime::Ref<Npc>> npc5{};
	runtime::Field<runtime::Ref<Npc>> npc6{};
	runtime::Field<runtime::Ref<Npc>> npc7{};
	runtime::Field<runtime::Ref<Npc>> npc8{};
	runtime::Field<runtime::Ref<Npc>> npc9{};
	runtime::Field<runtime::Ref<Npc>> npc10{};
	runtime::Field<runtime::Ref<Npc>> npc11{};

public:
	explicit EngineerLahulahuAI(Npc& owner) : AggressiveNpcAI(owner) {}

private:
	void registerNpcs();

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

protected:
	void handleBackHome() override;

private:
	void doSchedule();

	void useSkills();

	/** C++ only: one device's cast of the round (Java repeats the four statements per device) */
	void castOn(runtime::Field<runtime::Ref<Npc>>& device);
};

} // namespace aion::gameserver::handlers::ai::instance::rakes
