#include "aion/gameserver/services/toypet/PetService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PET.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::services::toypet {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   com.aionemu.gameserver.services.toypet.PetService@L154:46
//   com.aionemu.gameserver.services.toypet.PetService@L161:46
//   com.aionemu.gameserver.services.toypet.PetService@L168:46
//   com.aionemu.gameserver.services.toypet.PetService@L83:44

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.toypet.PetService");

PetService& PetService::getInstance() {
	static PetService instance; // Java SingletonHolder
	return instance;
}

PetService::PetService() = default;

void PetService::renamePet(model::gameobjects::player::Player& player, std::string_view name) {
	AION_UNPORTED();
}

void PetService::onPlayerLogin(model::gameobjects::player::Player& player) {
	std::vector<runtime::Ptr<model::gameobjects::player::PetCommonData>> playerPets = player.getPetList().getPets();
	if (!playerPets.empty())
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_PET(playerPets));
}

void PetService::removeObject(int32_t objectId, int32_t value, model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PetService::schedule(model::gameobjects::Pet& pet, model::gameobjects::player::Player& player, model::gameobjects::Item& item, int32_t value) {
	AION_UNPORTED();
}

void PetService::checkFeeding(model::gameobjects::Pet& pet, model::gameobjects::player::Player& player, model::gameobjects::Item& item,
	int32_t value) {
	AION_UNPORTED();
}

void PetService::useDoping(model::gameobjects::Pet& pet, int32_t action, int32_t itemId, int32_t slot, int32_t slot2) {
	AION_UNPORTED();
}

bool PetService::validateSetDopeItem(model::gameobjects::Pet& pet, int32_t itemId, int32_t slot) {
	AION_UNPORTED();
}

bool PetService::isPetItemUseAllowed(model::gameobjects::player::Player& player, model::gameobjects::Item& item) {
	AION_UNPORTED();
}

void PetService::activateLoot(model::gameobjects::Pet& pet, bool activate) {
	AION_UNPORTED();
}

void PetService::activateAutoSell(model::gameobjects::Pet& pet, bool activate) {
	AION_UNPORTED();
}

void PetService::sell(runtime::Ptr<model::gameobjects::Pet> pet, const std::vector<runtime::Ptr<model::gameobjects::Item>>& items) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::toypet
