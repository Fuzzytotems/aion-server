#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of Kahrun before Tiamat's stronghold door ("kharun"): dialog 1011; the first choice deletes him and 3 s later his fighting self
 * (800335) appears, casts 20943 on the Aetheric Field (730613), speaks, the door (730612) dies and the field goes.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/KharunAI.java, @AIName("kharun") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class KharunAI : public NpcAI {
public:
	explicit KharunAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

private:
	void startKharunEvent();
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
