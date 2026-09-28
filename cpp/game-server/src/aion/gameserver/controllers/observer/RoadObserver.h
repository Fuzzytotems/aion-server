#pragma once

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/road/fwd.h"

namespace aion::gameserver::controllers::observer {

/**
 * Teleports a player who crosses a road to the road's exit.
 * <p>
 * RefCounted ActionObserver (fieldmap K4), created by RoadController::see and stored in the player's ObserveController and the controller's
 * `observed` map.
 *
 * @author SheppeR
 */
class RoadObserver : public ActionObserver {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<model::gameobjects::player::Player> player;
	const runtime::Ref<model::road::Road> road;
	runtime::Field<geoEngine::math::Vector3f> oldPosition{};

protected:
	RoadObserver(model::road::Road& road, model::gameobjects::player::Player& player);
	~RoadObserver() override;

public:
	/** Java: new RoadObserver(road, player) */
	static runtime::Ref<RoadObserver> create(model::road::Road& road, model::gameobjects::player::Player& player);

	void moved() override;
};

} // namespace aion::gameserver::controllers::observer
