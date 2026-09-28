#pragma once

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/model/flyring/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::controllers::observer {

/**
 * Detects a player flying through a fly ring and applies Wings of Aether where the ring gives it.
 * <p>
 * RefCounted ActionObserver (fieldmap K4), created by FlyRingController::see and stored in the player's ObserveController and the controller's
 * `observed` map (cycles: the player -> ObserveController -> observer -> player edge is cut by LogoutBreakers L7 and FlyRingController::notSee).
 *
 * @author xavier, Source
 */
class FlyRingObserver : public ActionObserver {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<model::gameobjects::player::Player> player;
	const runtime::Ref<model::flyring::FlyRing> ring;
	runtime::Field<geoEngine::math::Vector3f> oldPosition{};

protected:
	FlyRingObserver(model::flyring::FlyRing& ring, model::gameobjects::player::Player& player);
	~FlyRingObserver() override;

public:
	/** Java: new FlyRingObserver(ring, player) */
	static runtime::Ref<FlyRingObserver> create(model::flyring::FlyRing& ring, model::gameobjects::player::Player& player);

	void moved() override;

private:
	bool isInstancetactive();

	bool isQuestactive();
};

} // namespace aion::gameserver::controllers::observer
