#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/town/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * @author ViAl
 */
class TownService : public runtime::Immortal {
private:
	runtime::HashMap<int32_t, runtime::Ref<model::town::Town>> elyosTowns{AION_LOCK_CLASS(TownService::elyosTowns)};
	runtime::HashMap<int32_t, runtime::Ref<model::town::Town>> asmosTowns{AION_LOCK_CLASS(TownService::asmosTowns)};
public:
	static TownService& getInstance(); // Java singleton
private:
	TownService();
public:
	runtime::Ptr<model::town::Town> getTownById(int32_t townId);
	int32_t getTownResidence(model::gameobjects::player::Player& player);
	int32_t getTownIdByPosition(model::gameobjects::Creature& creature);
	void onEnterWorld(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::services
