#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/movement/PlayableMoveController.h"
#include "aion/gameserver/controllers/movement/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::controllers::movement {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). The move controller part of Summon (Summon::postConstruct). The empty Java
 * moveToTargetObject is ported inline.
 *
 * @author ATracer
 */
class SummonMoveController : public PlayableMoveController {
public:
	explicit SummonMoveController(model::gameobjects::Summon& owner);
	~SummonMoveController() override;

	virtual void moveToTargetObject() {}
};

} // namespace aion::gameserver::controllers::movement
