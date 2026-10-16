#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/zone/fwd.h"

namespace aion::gameserver::controllers::observer {

/**
 * Observes a player's movement inside a quest zone (distance scouted, distance to the zone center, steps, time).
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). Abstract RefCounted ActionObserver (fieldmap K4), held by
 * `QuestZoneHandler::observed`; zone handlers create their subclasses. The constructor reads the player's position and the clock (member
 * stores only); the move task (Java anonymous Runnable, fieldmap `AbstractQuestZoneObserver_Runnable`, capturing only `this`) is a lambda
 * pinned to the observer.
 *
 * @author Rolandas
 */
class AbstractQuestZoneObserver : public ActionObserver {
	AION_MAKE_REF_FRIEND
protected:
	const runtime::Ref<model::gameobjects::player::Player> player;
	const geoEngine::math::Vector3f startPos;
	const int64_t startTime;
	const model::templates::zone::ZoneTemplate* observedZone;
	runtime::Field<geoEngine::math::Vector3f> oldPos{};
	runtime::Field<int32_t> stepCount{};

private:
	runtime::AtomicBoolean isRunning{AION_LOCK_CLASS(AbstractQuestZoneObserver::isRunning)};

protected:
	AbstractQuestZoneObserver(model::gameobjects::player::Player& player, const model::templates::zone::ZoneTemplate* zoneTemplate);
	~AbstractQuestZoneObserver() override;

public:
	void moved() override;

	virtual void onMoved(float distanceScouted, float distanceToCenter, int32_t steps, int64_t timeSpent) = 0;
};

} // namespace aion::gameserver::controllers::observer
