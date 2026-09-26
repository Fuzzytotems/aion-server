#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/movement/PlayableMoveController.h"
#include "aion/gameserver/controllers/movement/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::controllers::movement {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). The move controller part of Player (`moveController.set(
 * std::make_unique<PlayerMoveController>(*this))` in Player::postConstruct).
 *
 * @author ATracer
 */
class PlayerMoveController : public PlayableMoveController {
private:
	runtime::Field<float> fallDistance{};
	runtime::Field<float> lastFallZ{};
	runtime::Field<int8_t> lastMovementMask{};
	runtime::Field<int64_t> lastPositionFromClientMillis{};
	runtime::Field<runtime::Ref<world::WorldPosition>> lastPositionFromClient{};
	runtime::Field<int64_t> lastRandomMoveLocEffectTimeMillis{};

public:
	explicit PlayerMoveController(model::gameobjects::player::Player& owner);
	~PlayerMoveController() override;

	void abortMove() override;

	int8_t getLastMovementMask() const { return lastMovementMask.get(); }

	int64_t getLastPositionFromClientMillis() const { return lastPositionFromClientMillis.get(); }

	runtime::Ptr<world::WorldPosition> getLastPositionFromClient() const { return lastPositionFromClient.get(); }

	void resetLastPositionFromClient();

	/**
	 * This method should only be called from player move packets, not any calculated intermediate position updates by the server
	 */
	void onMoveFromClient();

	void resetToLastPositionFromClient();

	void updateFalling(float newZ);

	void stopFalling(float newZ);

	void setHasMovedByRandomMoveLocEffect(skillengine::model::Skill& skill);

	bool hasMovedByRandomMoveLocEffect();
};

} // namespace aion::gameserver::controllers::movement
