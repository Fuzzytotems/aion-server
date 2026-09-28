#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController_Destination.h"
#include "aion/gameserver/controllers/movement/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/geometry/fwd.h"
#include "aion/gameserver/model/templates/walker/fwd.h"

namespace aion::gameserver::controllers::movement {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). The move controller part of Npc (`moveController.set(
 * std::make_unique<NpcMoveController>(*this))` in Npc::postConstruct). Binds CreatureMoveController's type variable to Npc (bodies cast the
 * owner). `isStop` carries a trailing underscore (the class has a method of that name).
 *
 * @author ATracer
 */
class NpcMoveController : public CreatureMoveController {
private:
	using Destination = NpcMoveController_Destination;

	static constexpr float MOVE_OFFSET = 0.05f;
	static constexpr int32_t MAX_GEO_POINT_DISTANCE = 5;

	runtime::Field<Destination> destination{Destination::TARGET_OBJECT};

	runtime::Field<float> pointX{};
	runtime::Field<float> pointY{};
	runtime::Field<float> pointZ{};
	runtime::Field<bool> nextPointFromGeo{};
	runtime::Field<bool> isStop_{}; // Java: isStop

	runtime::Field<runtime::Ref<runtime::RcLinkedList<runtime::Ref<model::geometry::Point3D>>>> lastSteps{};

	runtime::Field<const model::templates::walker::WalkerTemplate*> walkerTemplate{};
	runtime::Field<const model::templates::walker::RouteStep*> currentStep{};

public:
	explicit NpcMoveController(model::gameobjects::Npc& owner);
	~NpcMoveController() override;

	/**
	 * Move to current target
	 */
	void moveToTargetObject();

	bool moveToPoint(float x, float y, float z);

	void forcedMoveToPoint(float x, float y, float z);

	void moveToNextPoint();

	void moveToDestination() override;

private:
	bool isOnGround(model::gameobjects::Creature& creature);

	/**
	 * Sets pointX, pointY and pointZ to valid geo coordinates near or at given position. Tries to detect and stop at cliffs or steep hills.
	 *
	 * @return True if a valid point was set
	 */
	bool trySetValidGeoPoint(float targetX, float targetY);

	void moveToLocation(float targetX, float targetY, float targetZ);

	int8_t getMoveMask(bool directionChanged);

public:
	void abortMove() override;

	/**
	 * Initialize values to default ones
	 */
	void resetMove();

	const model::templates::walker::WalkerTemplate* getWalkerTemplate() const { return walkerTemplate.get(); }

	void setWalkerTemplate(const model::templates::walker::WalkerTemplate* walkerTemplate, int32_t stepIndex);

	void setRouteStep(const model::templates::walker::RouteStep* step);

	const model::templates::walker::RouteStep* getCurrentStep() const { return currentStep.get(); }

	bool isReachedPoint();

	bool isNextRouteStepChosen();

	bool isChangingDirection();

	float getTargetX2() override final;

	float getTargetY2() override final;

	float getTargetZ2() override final;

	bool isStop() const { return isStop_.get(); }

private:
	void tryStoreStep(float x, float y, float z); // synchronized

public:
	void returnToLastStepOrSpawn(); // synchronized

	void clearBackSteps(); // synchronized
};

} // namespace aion::gameserver::controllers::movement
