#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

/**
 * The AI of Beshmundir's walk ("beshmundirswalk"), the temple's entrance: the group leader picks the safer or the dangerous path (a
 * question whose answer ports with difficulty 1 or 2); the members follow while one of them is inside.
 * <p>
 * Java: data/handlers/ai/instance/beshmundirTemple/BeshmundirsWalkAI.java, @AIName("beshmundirswalk") (the marker is in the .cpp).
 *
 * @author unknown
 */
class BeshmundirsWalkAI : public ActionItemNpcAI {
public:
	explicit BeshmundirsWalkAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

private:
	bool isAGroupMemberInInstance(Player& player);

	friend struct BeshmundirsWalkAI_AIRequest;

	void moveToInstance(Player& player, int8_t difficult);
};

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
