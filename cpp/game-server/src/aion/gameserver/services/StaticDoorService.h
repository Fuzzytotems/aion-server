#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * @author Wakizashi
 */
class StaticDoorService : public runtime::Immortal {
public:
	static StaticDoorService& getInstance(); // Java singleton
	void openStaticDoor(model::gameobjects::player::Player& player, int32_t doorId);
	void changeStaticDoorState(model::gameobjects::player::Player& player, int32_t doorId, bool open, int32_t state);
private:
	runtime::Ptr<model::gameobjects::StaticDoor> getDoor(model::gameobjects::player::Player& player, int32_t doorId);
	bool checkStaticDoorKey(model::gameobjects::player::Player& player, model::gameobjects::StaticDoor& door, int32_t keyId);
};

} // namespace aion::gameserver::services
