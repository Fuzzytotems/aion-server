#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ShifterAI.h"

#include <vector>

#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::pvpArenas {

/**
 * The AI of the plaza flame throwers ("plaza_flame_thrower"): usable once the run started; the first use rewards the user, casts the bonus
 * skills of the two linked npcs, and the thrower goes and respawns.
 * <p>
 * Java: data/handlers/ai/instance/pvpArenas/PlazaFlameThrowerAI.java, @AIName("plaza_flame_thrower") (the marker is in the .cpp).
 *
 * @author xTz
 */
class PlazaFlameThrowerAI : public ShifterAI {
private:
	runtime::Field<bool> isRewarded{};

public:
	explicit PlazaFlameThrowerAI(Npc& owner) : ShifterAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

	void handleUseItemFinish(Player& player) override;

private:
	void useSkill(const std::vector<runtime::Ptr<Npc>>& npcs);

	std::vector<runtime::Ptr<Npc>> getNpcs(int32_t npcId);
};

} // namespace aion::gameserver::handlers::ai::instance::pvpArenas
