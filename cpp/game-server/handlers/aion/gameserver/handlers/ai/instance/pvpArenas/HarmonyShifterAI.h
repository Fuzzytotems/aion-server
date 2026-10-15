#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ShifterAI.h"

#include <vector>

#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::instance::pvpArenas {

/**
 * The AI of the Harmony arenas' shifters ("harmony_shifter"): the first use rewards the user, casts the bonus skills of the linked npcs,
 * and the shifter goes and respawns.
 * <p>
 * Java: data/handlers/ai/instance/pvpArenas/HarmonyShifterAI.java, @AIName("harmony_shifter") (the marker is in the .cpp).
 *
 * @author xTz
 */
class HarmonyShifterAI : public ShifterAI {
private:
	runtime::AtomicBoolean isRewarded{AION_LOCK_CLASS(HarmonyShifterAI::isRewarded)}; // Java: = new AtomicBoolean(false)

public:
	explicit HarmonyShifterAI(Npc& owner) : ShifterAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;

private:
	void useSkill(const std::vector<runtime::Ptr<Npc>>& npcs);

	std::vector<runtime::Ptr<Npc>> getNpcs(int32_t npcId);
};

} // namespace aion::gameserver::handlers::ai::instance::pvpArenas
