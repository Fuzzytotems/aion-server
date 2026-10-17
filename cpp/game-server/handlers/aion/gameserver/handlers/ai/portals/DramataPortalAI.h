#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::portals {

/**
 * The AI of the Dramata rift ("dramata_portal"): its spawn in Kaisinel's Academy or Marchutan Priory announces it to the race's players
 * of level 65 and up and sets its target; usable from level 60, ten minutes after its spawn.
 * <p>
 * Java: data/handlers/ai/portals/DramataPortalAI.java, @AIName("dramata_portal") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class DramataPortalAI : public ActionItemNpcAI {
private:
	runtime::Field<runtime::Ref<WorldPosition>> targetLocation{};

public:
	explicit DramataPortalAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::portals
