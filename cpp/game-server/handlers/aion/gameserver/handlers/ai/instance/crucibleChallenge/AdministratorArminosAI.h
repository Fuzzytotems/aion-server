#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::crucibleChallenge {

/**
 * Crucible Challenge's administrator Arminos ("administratorarminos"): the dialog spawns the three bonus chests of stage 4's arena and
 * deletes the administrator.
 * <p>
 * Java: data/handlers/ai/instance/crucibleChallenge/AdministratorArminosAI.java, @AIName("administratorarminos") (the marker is in the .cpp).
 *
 * @author xTz
 */
class AdministratorArminosAI : public NpcAI {
public:
	explicit AdministratorArminosAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;
};

} // namespace aion::gameserver::handlers::ai::instance::crucibleChallenge
