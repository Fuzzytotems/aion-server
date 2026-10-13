#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::portals {

/**
 * The Eternal Challenge's teleporter ("custom_instance_teleporter"): at the end of the use bar a level 65 player who has not entered today
 * enters the custom instance (D12, owner 2026-10-13).
 * <p>
 * Java: data/handlers/ai/portals/CustomInstanceTeleporter.java.
 */
class CustomInstanceTeleporter : public ActionItemNpcAI {
public:
	explicit CustomInstanceTeleporter(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::portals
