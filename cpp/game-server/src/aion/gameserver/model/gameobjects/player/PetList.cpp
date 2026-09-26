#include "aion/gameserver/model/gameobjects/player/PetList.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dao/PlayerPetsDAO.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::model::gameobjects::player {

namespace {

/** PetList::setPlayerPetsLoaderForTests (C++ only test seam); nullptr: PlayerPetsDAO */
runtime::Field<PetList::PlayerPetsLoader> playerPetsLoaderForTests{nullptr};

/** Java PlayerPetsDAO.getPlayerPets(player), or the test loader while one is set */
std::vector<runtime::Ref<PetCommonData>> getPlayerPets(Player& player) {
	if (PetList::PlayerPetsLoader loader = playerPetsLoaderForTests.get())
		return loader(player);
	return dao::PlayerPetsDAO::getPlayerPets(player);
}

/** Java ExpireTimerTask.getInstance().registerExpirable(expirable, player); the task manager (P5-14) has no C++ header yet */
void registerExpirable(PetCommonData& expirable, Player& player) {
	static_cast<void>(expirable);
	static_cast<void>(player);
	AION_UNPORTED();
}

} // namespace

PetList::PetList(Player& player) : OwnedPart(player) {
	loadPets(player);
}

PetList::PetList(Player& player, DeferredLoad) : OwnedPart(player) {
}

void PetList::setPlayerPetsLoaderForTests(PlayerPetsLoader loader) noexcept {
	playerPetsLoaderForTests.set(loader);
}

PetList::~PetList() = default;

void PetList::loadPets(Player& player) {
	std::vector<runtime::Ref<PetCommonData>> playerPets = getPlayerPets(player);
	runtime::Ptr<PetCommonData> lastUsedPet;
	for (const runtime::Ref<PetCommonData>& pet : playerPets) {
		registerExpirable(*pet, player);
		pets.put(pet->getTemplateId(), pet); // the client only sends template ids for spawn/dismiss, so we cannot support multiple same pets
		if (!lastUsedPet || pet->getDespawnTime().value() > lastUsedPet->getDespawnTime().value())
			lastUsedPet = pet;
	}

	// Java bug kept: stores the object id although the map is keyed by template id (getLastUsedPet then looks up the object id)
	if (lastUsedPet)
		lastUsedPetTemplateId.set(lastUsedPet->getObjectId());
}

std::vector<runtime::Ptr<PetCommonData>> PetList::getPets() {
	return pets.values();
}

runtime::Ptr<PetCommonData> PetList::getPet(int32_t petId) {
	return pets.get(petId);
}

runtime::Ptr<PetCommonData> PetList::getLastUsedPet() {
	return getPet(lastUsedPetTemplateId.get());
}

runtime::Ref<PetCommonData> PetList::addPet(Player& player, int32_t petId, int32_t decorationId, std::string_view name, int32_t expireTime) {
	return addPet(player, petId, decorationId, commons::utils::currentTimeMillis(), name, expireTime);
}

runtime::Ref<PetCommonData> PetList::addPet(Player& player, int32_t petId, int32_t decorationId, int64_t birthday, std::string_view name,
	int32_t expireTime) {
	runtime::Ref<PetCommonData> petCommonData =
		PetCommonData::create(utils::idfactory::IDFactory::getInstance().nextId(), petId, player.getObjectId(), expireTime);
	petCommonData->setDecoration(decorationId);
	petCommonData->setName(name);
	petCommonData->setBirthday(commons::database::Timestamp(std::chrono::milliseconds(birthday)));
	petCommonData->setDespawnTime(commons::database::Timestamp(std::chrono::milliseconds(commons::utils::currentTimeMillis())));
	dao::PlayerPetsDAO::insertPlayerPet(player, *petCommonData);
	pets.put(petId, petCommonData);
	return petCommonData;
}

bool PetList::hasPet(int32_t templateId) {
	return pets.containsKey(templateId);
}

runtime::Ptr<PetCommonData> PetList::deletePet(int32_t templateId) {
	runtime::Ptr<PetCommonData> petCommonData = pets.remove(templateId);
	if (petCommonData) {
		dao::PlayerPetsDAO::removePlayerPet(petCommonData->getObjectId());
		return petCommonData;
	}
	return nullptr;
}

} // namespace aion::gameserver::model::gameobjects::player
