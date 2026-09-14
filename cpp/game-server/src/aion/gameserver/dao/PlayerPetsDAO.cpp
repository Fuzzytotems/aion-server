#include "aion/gameserver/dao/PlayerPetsDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerPetsDAO");

void PlayerPetsDAO::saveFeedStatus(int32_t petObjectId, int32_t hungryLevel, int32_t feedProgress, int64_t reuseTime) {
	AION_UNPORTED();
}

void PlayerPetsDAO::saveDopingBag(int32_t petObjectId, model::templates::pet::PetDopingBag& bag) {
	AION_UNPORTED();
}

void PlayerPetsDAO::setTime(int32_t petObjectId, int64_t time) {
	AION_UNPORTED();
}

void PlayerPetsDAO::insertPlayerPet(model::gameobjects::player::Player& player, model::gameobjects::player::PetCommonData& petCommonData) {
	AION_UNPORTED();
}

void PlayerPetsDAO::removePlayerPet(int32_t petObjectId) {
	AION_UNPORTED();
}

std::vector<runtime::Ref<model::gameobjects::player::PetCommonData>> PlayerPetsDAO::getPlayerPets(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerPetsDAO::updatePetName(model::gameobjects::player::PetCommonData& petCommonData) {
	AION_UNPORTED();
}

bool PlayerPetsDAO::savePetMoodData(model::gameobjects::player::PetCommonData& petCommonData) {
	AION_UNPORTED();
}

std::vector<int32_t> PlayerPetsDAO::getUsedIDs() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
