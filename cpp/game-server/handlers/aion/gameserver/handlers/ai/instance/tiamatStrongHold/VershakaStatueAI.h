#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of Vershaka's statue in Tiamat Stronghold ("vershakastatue"): dialog 1011; the first choice puts 300 on the player; any choice closes
 * the dialog.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/VershakaStatueAI.java, @AIName("vershakastatue") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class VershakaStatueAI : public NpcAI {
public:
	explicit VershakaStatueAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
