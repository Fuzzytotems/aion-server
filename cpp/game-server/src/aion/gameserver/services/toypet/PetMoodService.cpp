#include "aion/gameserver/services/toypet/PetMoodService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services::toypet {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.toypet.PetMoodService");

void PetMoodService::checkMood(model::gameobjects::Pet& pet, int32_t type, int32_t shuggleEmotion) {
	AION_UNPORTED();
}

void PetMoodService::requestPresent(model::gameobjects::Pet& pet) {
	AION_UNPORTED();
}

void PetMoodService::interactWithPet(model::gameobjects::Pet& pet, int32_t shuggleEmotion) {
	AION_UNPORTED();
}

void PetMoodService::startCheckingMood(model::gameobjects::Pet& pet) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::toypet
