#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::sauroBase {

/**
 * The AI of Sauro Supply Base's hidden passage ("saurohiddenpassage"): the third and fourth boss choices take one or two keys - all the
 * player's keys - and open the passage to that boss.
 * <p>
 * Java: data/handlers/ai/instance/sauroBase/SauroFinalTeleportAI.java, @AIName("saurohiddenpassage") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class SauroFinalTeleportAI : public NpcAI {
public:
	explicit SauroFinalTeleportAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

private:
	void switchWay(Player& player, int32_t dialogActionId);

	void checkKeys(Player& player, int64_t keyCount);
};

} // namespace aion::gameserver::handlers::ai::instance::sauroBase
