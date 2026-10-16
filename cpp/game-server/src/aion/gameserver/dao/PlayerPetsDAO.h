#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/pet/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author Xitanium, Kamui, Rolandas, M@xx, xTz
 */
class PlayerPetsDAO {
public:
	static void saveFeedStatus(int32_t petObjectId, int32_t hungryLevel, int32_t feedProgress, int64_t reuseTime);
	static void saveDopingBag(int32_t petObjectId, model::templates::pet::PetDopingBag& bag);
	static void setTime(int32_t petObjectId, int64_t time);
	static void insertPlayerPet(model::gameobjects::player::Player& player, model::gameobjects::player::PetCommonData& petCommonData);
	static void removePlayerPet(int32_t petObjectId);
	static std::vector<runtime::Ref<model::gameobjects::player::PetCommonData>> getPlayerPets(model::gameobjects::player::Player& player);
	static void updatePetName(model::gameobjects::player::PetCommonData& petCommonData);
	static bool savePetMoodData(model::gameobjects::player::PetCommonData& petCommonData);
	static std::vector<int32_t> getUsedIDs();
};

} // namespace aion::gameserver::dao
