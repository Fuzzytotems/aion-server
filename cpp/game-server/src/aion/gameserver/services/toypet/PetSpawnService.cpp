#include "aion/gameserver/services/toypet/PetSpawnService.h"

#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/PeriodicSaveConfig.h"
#include "aion/gameserver/controllers/PetController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/PetSpecialFunction.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/pet/PetTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PET.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sched/Pin.h"
#include "aion/gameserver/services/toypet/PetFeedProgress.h"
#include "aion/gameserver/spawnengine/VisibleObjectSpawner.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

namespace aion::gameserver::services::toypet {

// Java PetSpawnService.java:25-61
void PetSpawnService::summonPet(model::gameobjects::player::Player& player, int32_t templateId) {
	using network::aion::serverpackets::SM_PET;
	runtime::Ptr<model::gameobjects::player::PetCommonData> lastPetCommonData;

	if (runtime::Ptr<model::gameobjects::Pet> current = player.getPet()) {
		if (current->getObjectTemplate()->getTemplateId() == templateId)
			return;
		lastPetCommonData = current->getCommonData();
		current->getController().delete_();
	} else {
		lastPetCommonData = player.getPetList().getLastUsedPet();
	}

	if (lastPetCommonData != nullptr && lastPetCommonData->getTemplateId() != templateId) // reset mood if other pet is spawned
		lastPetCommonData->clearMoodStatistics();

	const int64_t period = static_cast<int64_t>(configs::main::PeriodicSaveConfig::PLAYER_PETS.load()) * 1000;
	runtime::Ref<controllers::PetController::PetUpdateTask> updateTask = controllers::PetController::PetUpdateTask::create(player);
	player.getController().addTask(model::TaskId::PET_UPDATE, utils::ThreadPoolManager::getInstance().scheduleAtFixedRate(
		runtime::Pin(updateTask), [updateTask] { updateTask->run(); }, period, period));

	runtime::Ptr<model::gameobjects::Pet> pet = spawnengine::VisibleObjectSpawner::spawnPet(player, templateId);
	if (pet == nullptr) {
		utils::audit::AuditLogger::log(player, "tried to spawn invalid pet with id " + std::to_string(templateId));
		return;
	}

	runtime::Ptr<model::gameobjects::player::PetCommonData> petCommonData = pet->getCommonData();
	if (petCommonData->getRefeedDelay() > 0) {
		petCommonData->scheduleRefeed(petCommonData->getRefeedDelay());
	} else if (petCommonData->getFeedProgress() != nullptr)
		petCommonData->getFeedProgress()->setHungryLevel(PetHungryLevel::HUNGRY);

	const std::optional<commons::database::Timestamp> despawnTime = petCommonData->getDespawnTime();
	if (!despawnTime) // Java: getDespawnTime().getTime() on null
		throw runtime::NullPointerException("PetCommonData.getDespawnTime()");
	if (commons::utils::currentTimeMillis() - despawnTime->time_since_epoch().count() > 10 * 60 * 1000) // reset mood if despawned for > 10 minutes
		petCommonData->clearMoodStatistics();

	player.getPetList().setLastUsedPetTemplateId(templateId);

	if (petCommonData->isLooting())
		utils::PacketSendUtility::sendPacket(player, SM_PET(model::gameobjects::PetSpecialFunction::AUTOLOOT, true));
	if (petCommonData->isSelling())
		utils::PacketSendUtility::sendPacket(player, SM_PET(model::gameobjects::PetSpecialFunction::AUTOSELL, true));
}

} // namespace aion::gameserver::services::toypet
