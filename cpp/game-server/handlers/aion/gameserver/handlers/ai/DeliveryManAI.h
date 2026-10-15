#pragma once

#include "aion/gameserver/handlers/ai/FollowingNpcAI.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of the express mail postman ("deliveryman", 798100 and 798101): it follows the player who summoned it for five minutes and opens his
 * mailbox in express mode.
 * <p>
 * Java: data/handlers/ai/DeliveryManAI.java, @AIName("deliveryman") (the marker is in the .cpp).
 *
 * @author -Nemesiss-, Neon
 */
class DeliveryManAI : public FollowingNpcAI {
private:
	static constexpr int32_t SERVICE_TIME = 5 * 60 * 1000;

public:
	explicit DeliveryManAI(Npc& owner) : FollowingNpcAI(owner) {}

protected:
	void handleSpawned() override;
	void handleDespawned() override;
	void handleDialogStart(Player& player) override;

private:
	runtime::Ptr<Player> getPlayer();
};

} // namespace aion::gameserver::handlers::ai
