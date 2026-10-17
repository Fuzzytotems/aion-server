#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::events::iceFestival {

/**
 * The AI of Atirunerk of the ice festival ("atirunerk"): the dialog also selects quest 80719.
 * <p>
 * Java: data/handlers/ai/events/iceFestival/AtirunerkAI.java, @AIName("atirunerk") (the marker is in the .cpp).
 *
 * @author (none)
 */
class AtirunerkAI : public GeneralNpcAI {
public:
	explicit AtirunerkAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::events::iceFestival
