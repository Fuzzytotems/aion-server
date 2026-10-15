#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/portals/PortalAI.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::portals {

/**
 * The AI of a portal with a dialog ("portal_dialog": instance entrance statues and gates whose window lists the routes): a click opens the
 * quest page when a talk quest of the npc is running, the quest start page when one can be started, else the teleport page of
 * portal_template2.xml (ten npcs have a hard-coded page); a route chosen there moves the player through PortalService. A portal with a talk
 * delay runs ActionItemNpcAI's use bar first.
 * <p>
 * Java: data/handlers/ai/portals/PortalDialogAI.java, @AIName("portal_dialog") (the marker is in the .cpp).
 *
 * @author xTz, vlog
 */
class PortalDialogAI : public PortalAI {
public:
	explicit PortalDialogAI(Npc& owner) : PortalAI(owner) {}

protected:
	/** Standard value. Can be changed through override */
	runtime::Field<int32_t> rewardDialogId{5};
	/** Standard value. Can be changed through override */
	runtime::Field<int32_t> startingDialogId{10};
	/** Standard value. Can be changed through override */
	runtime::Field<int32_t> questDialogId{10};

	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

protected:
	void handleUseItemFinish(Player& player) override;

	virtual void checkDialog(Player& player);
};

} // namespace aion::gameserver::handlers::ai::portals
