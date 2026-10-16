#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ChestAI.h"

namespace aion::gameserver::handlers::ai::events {

/**
 * The AI of the fake cake ("pandoras_box"): its use is logged as cheating, and the cake dies.
 * <p>
 * Java: data/handlers/ai/events/FakeCakeAI.java, @AIName("pandoras_box") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class FakeCakeAI : public ChestAI {
public:
	explicit FakeCakeAI(Npc& owner) : ChestAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;

private:
	void punishment(Player& player);
};

} // namespace aion::gameserver::handlers::ai::events
