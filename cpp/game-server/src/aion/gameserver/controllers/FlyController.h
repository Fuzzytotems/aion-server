#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::controllers {

/**
 * Starts and ends flying and gliding of a player.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). A part of Player (fieldmap: `Player.flyController`, `PartSlot<FlyController>`),
 * bound to its owner in the constructor (the Java `player` field is the OwnerRef).
 *
 * @author ATracer
 */
class FlyController : public runtime::OwnedPart {
private:
	static constexpr int64_t FLY_REUSE_TIME = 10000;

	runtime::OwnerRef<model::gameobjects::player::Player> player;

public:
	explicit FlyController(model::gameobjects::player::Player& player);
	~FlyController() override;

	void onStopGliding();

	/**
	 * Ends flying 1) by CM_EMOTION (pageDown or fly button press) 2) from server side during teleport (abyss gates should not break flying) 3)
	 * when FP is decreased to 0
	 */
	void endFly(bool broadcastPacket);

	/**
	 * This method is called to start flying (called by CM_EMOTION when pageUp or pressed fly button, on revive or after teleport in some cases)
	 *
	 * @param broadcastPacket notify the players client, and all players in range that he started flying
	 * @param ignoreFlightCooldown if true, this will skip cooldown check and not set a new cooldown
	 * @return False if the player could not start flying due to some restriction, true otherwise
	 */
	bool startFly(bool broadcastPacket, bool ignoreFlightCooldown);

private:
	static bool canFly(model::gameobjects::player::Player& player);

public:
	/**
	 * Switching to glide mode (called by CM_MOVE with VALIDATE_GLIDE movement type) 1) from standing state 2) from flying state
	 */
	bool switchToGliding();

private:
	static bool canGlide(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::controllers
