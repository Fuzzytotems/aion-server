#pragma once

#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::portals {

/**
 * The AI of the Sorcerer's group gates ("groupgate"): a Daeva who is the creator or in his group is asked to move to Sanctum, Pandaemonium or
 * the bind point by the gate's npc id. Java's anonymous AIRequest is the callback struct GroupGateAI_AIRequest of the .cpp (fieldmap
 * ai.portals.GroupGateAI$1).
 * <p>
 * Java: data/handlers/ai/portals/GroupGateAI.java, @AIName("groupgate") (the marker is in the .cpp).
 *
 * @author ATracer, nrg
 */
class GroupGateAI : public gameserver::ai::NpcAI {
private:
	static constexpr int32_t CANCEL_DIALOG_METERS = 9;

public:
	explicit GroupGateAI(Npc& owner) : gameserver::ai::NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

private:
	friend struct GroupGateAI_AIRequest;
};

} // namespace aion::gameserver::handlers::ai::portals
