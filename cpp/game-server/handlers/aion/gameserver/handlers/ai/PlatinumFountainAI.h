#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of the platinum fountain ("fountain"): a gold medal thrown in returns a platinum medal (10 %) or a rusty one.
 * <p>
 * Java: data/handlers/ai/PlatinumFountainAI.java, @AIName("fountain"). The German texts are Java's, written as UTF-8 here (the Java file
 * is ISO-8859-1: its 0xFC is the u with diaeresis).
 */
class PlatinumFountainAI : public ActionItemNpcAI {
public:
	explicit PlatinumFountainAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai
