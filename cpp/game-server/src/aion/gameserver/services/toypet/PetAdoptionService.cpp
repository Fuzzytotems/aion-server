#include "aion/gameserver/services/toypet/PetAdoptionService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services::toypet {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.toypet.PetAdoptionService");

void PetAdoptionService::adoptPet(model::gameobjects::player::Player& player, int32_t eggObjId, int32_t petId, std::string_view name,
	int32_t decorationId) {
	AION_UNPORTED();
}

void PetAdoptionService::addPet(model::gameobjects::player::Player& player, int32_t petId, std::string_view name, int32_t decorationId,
	int32_t expireTime) {
	AION_UNPORTED();
}

bool PetAdoptionService::validateAdoption(model::gameobjects::player::Player& player, const model::templates::item::ItemTemplate* template_,
	int32_t petId) {
	AION_UNPORTED();
}

void PetAdoptionService::surrenderPet(model::gameobjects::player::Player& player, int32_t petId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::toypet
