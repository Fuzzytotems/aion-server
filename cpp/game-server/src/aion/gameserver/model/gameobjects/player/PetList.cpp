#include "aion/gameserver/model/gameobjects/player/PetList.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::model::gameobjects::player {

PetList::PetList(Player& player) : OwnedPart(player) {
	// Java: loadPets(player) (PlayerPetsDAO)
	AION_UNPORTED();
}

PetList::~PetList() = default;

void PetList::loadPets(Player& player) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<PetCommonData>> PetList::getPets() {
	AION_UNPORTED();
}

runtime::Ptr<PetCommonData> PetList::getPet(int32_t petId) {
	AION_UNPORTED();
}

runtime::Ptr<PetCommonData> PetList::getLastUsedPet() {
	AION_UNPORTED();
}

runtime::Ref<PetCommonData> PetList::addPet(Player& player, int32_t petId, int32_t decorationId, std::string_view name, int32_t expireTime) {
	AION_UNPORTED();
}

runtime::Ref<PetCommonData> PetList::addPet(Player& player, int32_t petId, int32_t decorationId, int64_t birthday, std::string_view name,
	int32_t expireTime) {
	AION_UNPORTED();
}

bool PetList::hasPet(int32_t templateId) {
	AION_UNPORTED();
}

runtime::Ptr<PetCommonData> PetList::deletePet(int32_t templateId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player
