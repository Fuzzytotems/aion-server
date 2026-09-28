#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"

namespace aion::gameserver::controllers {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). The controller part of House (`std::make_unique<HouseController>()`, late-bound by
 * setOwner). Binds VisibleObjectController's type variable to House: getOwner() returns `House&` (§8.2).
 *
 * @author Rolandas, Neon
 */
class HouseController : public VisibleObjectController {
public:
	HouseController();
	~HouseController() override;

	/** Narrowing accessor (Java: VisibleObjectController<House>.getOwner(), hub-headers.md §8.2). */
	model::house::House& getOwner() const;

	void see(model::gameobjects::VisibleObject& object) override;

	void spawnObjects();

	void onAfterSpawn() override;

private:
	void updateSpawns();

public:
	void onDespawn() override;

	void updateAppearance();

	/** @param kicker null when the server kicks the visitors (Java null checks) */
	void kickVisitors(runtime::Ptr<model::gameobjects::player::Player> kicker, bool kickFriends, bool ownerChanged);

private:
	void moveOutside(model::gameobjects::player::Player& player, bool ownerChanged);

public:
	void teleportNearHouseDoor(model::gameobjects::player::Player& player, bool outsideHouse);

	void updateSign();

	void updateHouseSpawns();

private:
	int32_t getCurrentSignNpcId();
};

} // namespace aion::gameserver::controllers
