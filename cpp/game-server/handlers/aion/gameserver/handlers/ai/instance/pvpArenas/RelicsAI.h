#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::pvpArenas {

/**
 * The AI of the arenas' relics ("pvparenarelics"): usable once the run started; the first use rewards the user and the relic goes (and
 * respawns, but for the random relics 701187 and 701188).
 * <p>
 * Java: data/handlers/ai/instance/pvpArenas/RelicsAI.java, @AIName("pvparenarelics") (the marker is in the .cpp).
 *
 * @author xTz
 */
class RelicsAI : public ActionItemNpcAI {
private:
	runtime::Field<bool> isRewarded{};

public:
	explicit RelicsAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::instance::pvpArenas
