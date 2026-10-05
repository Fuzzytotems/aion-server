#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::portals {

/**
 * The AI of a portal ("portal": instance entrances and exits, rift-like gates): a click tells the quests (USE_OBJECT) and starts
 * ActionItemNpcAI's use bar; at its end the portal's use path of portal_template2.xml moves the player through PortalService, else the
 * npc's teleporter data sends him to its first location, else the plain use-item finish runs.
 * <p>
 * Java: data/handlers/ai/portals/PortalAI.java, @AIName("portal") (the marker is in the .cpp).
 *
 * @author xTz
 */
class PortalAI : public ActionItemNpcAI {
protected:
	runtime::Field<const TeleporterTemplate*> teleportTemplate{};

public:
	explicit PortalAI(Npc& owner) : ActionItemNpcAI(owner) {}

	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

protected:
	void handleSpawned() override;

	void handleDialogStart(Player& player) override;

	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::portals
