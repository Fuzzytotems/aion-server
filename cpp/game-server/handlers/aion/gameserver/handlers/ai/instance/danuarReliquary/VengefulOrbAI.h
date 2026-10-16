#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::danuarReliquary {

/**
 * The AI of Modor's vengeful orb ("vengeful_orb"): 100 ms after its spawn it casts 21178 while alive, whose end removes it.
 * <p>
 * Java: data/handlers/ai/instance/danuarReliquary/VengefulOrbAI.java, @AIName("vengeful_orb") (the marker is in the .cpp).
 *
 * @author Ritsu, Estrayl, Yeats
 */
class VengefulOrbAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> skillTask{};

public:
	explicit VengefulOrbAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

public:
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

protected:
	void handleDespawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::danuarReliquary
