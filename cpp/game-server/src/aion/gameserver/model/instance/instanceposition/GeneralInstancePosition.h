#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/instance/instanceposition/InstancePositionHandler.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"

namespace aion::gameserver::model::instance::instanceposition {

/**
 * Java abstract class GeneralInstancePosition implements InstancePositionHandler (fieldmap: RefCounted, Field<int32_t> members).
 *
 * @author xTz
 */
class GeneralInstancePosition : public runtime::RefCounted, public InstancePositionHandler {
protected:
	runtime::Field<int32_t> mapId{};
	runtime::Field<int32_t> instanceId{};

	GeneralInstancePosition();
	~GeneralInstancePosition() override;

	void teleport(gameobjects::player::Player& player, float x, float y, float z, int8_t h);

public:
	/** Java: final */
	void initialize(int32_t mapId, int32_t instanceId) override final;
};

} // namespace aion::gameserver::model::instance::instanceposition
