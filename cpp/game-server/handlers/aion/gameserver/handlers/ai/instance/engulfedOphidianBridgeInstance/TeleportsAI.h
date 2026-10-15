#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::engulfedOphidianBridgeInstance {

/**
 * The AI of the teleporters in Engulfed Ophidan Bridge ("engulfedophidianteleport"): with a scroll (164000279) the dialog teleports north
 * or south and takes the scroll.
 * <p>
 * Java: data/handlers/ai/instance/engulfedOphidianBridgeInstance/TeleportsAI.java, @AIName("engulfedophidianteleport") (the marker is in
 * the .cpp).
 *
 * @author cheatkiller
 */
class TeleportsAI : public ActionItemNpcAI {
public:
	explicit TeleportsAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

private:
	bool checkScroll(Player& player);
};

} // namespace aion::gameserver::handlers::ai::instance::engulfedOphidianBridgeInstance
