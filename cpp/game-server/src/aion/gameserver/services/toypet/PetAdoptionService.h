#pragma once

#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/services/toypet/fwd.h"

namespace aion::gameserver::services::toypet {

/**
 * @author ATracer
 */
class PetAdoptionService {
public:
	/** Create a pet for player (with validation) */
	static void adoptPet(model::gameobjects::player::Player& player, int32_t eggObjId, int32_t petId, std::string_view name, int32_t decorationId);
	/** Add pet to player */
	static void addPet(model::gameobjects::player::Player& player, int32_t petId, std::string_view name, int32_t decorationId, int32_t expireTime);
private:
	static bool validateAdoption(model::gameobjects::player::Player& player, const model::templates::item::ItemTemplate* template_, int32_t petId);
public:
	/** Delete pet */
	static void surrenderPet(model::gameobjects::player::Player& player, int32_t petId);
};

} // namespace aion::gameserver::services::toypet
