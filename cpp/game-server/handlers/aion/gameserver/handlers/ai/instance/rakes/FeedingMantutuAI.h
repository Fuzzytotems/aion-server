#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ShifterAI.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

/**
 * The AI of Golden Eye Mantutu's supplies on the Steel Rake ("feeding_mantutu": the water supply 701387, the feed supply 701386): usable only
 * while no supply device (281128, 281129) stands; used while Golden Eye Mantutu (219033) lives, it spawns its device, sends the boss to it
 * (custom event 1 with the device) and is deleted.
 * <p>
 * Java: data/handlers/ai/instance/rakes/FeedingMantutuAI.java, @AIName("feeding_mantutu") (the marker is in the .cpp).
 *
 * @author xTz
 */
class FeedingMantutuAI : public ShifterAI {
public:
	explicit FeedingMantutuAI(Npc& owner) : ShifterAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::instance::rakes
