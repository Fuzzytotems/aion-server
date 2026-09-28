#pragma once

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/movement/SummonMoveController.h"
#include "aion/gameserver/controllers/movement/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::controllers::movement {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). The move controller part of siege weapon summons (Summon::postConstruct).
 *
 * @author xTz
 */
class SiegeWeaponMoveController : public SummonMoveController {
private:
	runtime::Field<float> pointX{};
	runtime::Field<float> pointY{};
	runtime::Field<float> pointZ{};

public:
	explicit SiegeWeaponMoveController(model::gameobjects::Summon& owner);
	~SiegeWeaponMoveController() override;

	/**
	 * @return if destination reached
	 */
	void moveToDestination() override;

	void moveToTargetObject() override;

	void abortMove() override;

protected:
	void moveToLocation(float targetX, float targetY, float targetZ);
};

} // namespace aion::gameserver::controllers::movement
