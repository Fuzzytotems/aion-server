#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
#include "aion/gameserver/controllers/movement/PlayableMoveController_MovementModifierDirection.h"
#include "aion/gameserver/controllers/movement/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::controllers::movement {

/**
 * Base class for summon & player move controller
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). Java `PlayableMoveController<T extends Creature>` is one non-template class (§8.1):
 * the owner is a Creature (casts where Java used T). A part of Creature (OwnedPart through CreatureMoveController). setNewDirection(x, y, z)
 * is public here, so the 4-argument base overload is re-exposed with a using-declaration.
 *
 * @author ATracer
 */
class PlayableMoveController : public CreatureMoveController {
public:
	using MovementModifierDirection = PlayableMoveController_MovementModifierDirection;

private:
	runtime::Field<bool> sendMovePacket{true};
	runtime::Field<MovementModifierDirection> movementModifierDirection{MovementModifierDirection::NONE};

public:
	runtime::Field<float> vehicleX{};
	runtime::Field<float> vehicleY{};
	runtime::Field<float> vehicleZ{};

	runtime::Field<float> vectorX{};
	runtime::Field<float> vectorY{};
	runtime::Field<float> vectorZ{};
	runtime::Field<int8_t> glideFlag{};
	runtime::Field<int32_t> unk1{};
	runtime::Field<int32_t> unk2{};
	runtime::Field<int32_t> geyserLocationId{}; // locationId from windstreams.xml

protected:
	/** Java: public constructor of the abstract class */
	explicit PlayableMoveController(model::gameobjects::Creature& owner);

public:
	~PlayableMoveController() override;

	using CreatureMoveController::setNewDirection;

	void startMovingToDestination() override;

private:
	bool isControlled();

	void sendForcedMovePacket();

public:
	void moveToDestination() override;

	void abortMove() override;

	void setNewDirection(float x, float y, float z) override;

	MovementModifierDirection getMovementDirection();
};

} // namespace aion::gameserver::controllers::movement
