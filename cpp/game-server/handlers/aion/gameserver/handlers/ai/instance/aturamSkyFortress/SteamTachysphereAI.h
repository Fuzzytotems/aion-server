#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress {

/**
 * The AI of the steam tachysphere ("steam_tachysphere"): open to those who completed the race's quest (18302 or 28302); its first choice
 * flies the player up with the movie 471 and the mark 19502.
 * <p>
 * Java: data/handlers/ai/instance/aturamSkyFortress/SteamTachysphereAI.java, @AIName("steam_tachysphere") (the marker is in the .cpp).
 *
 * @author xTz
 */
class SteamTachysphereAI : public ActionItemNpcAI {
public:
	explicit SteamTachysphereAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;
};

} // namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress
