#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). A part of Player (`const std::unique_ptr<PetList>` toyPetList), bound to the player
 * in the constructor (Java keeps no owner field). The constructor loads the pets from the database (PlayerPetsDAO through loadPets), so it stays
 * `AION_UNPORTED` after binding the owner.
 *
 * @author ATracer
 */
class PetList : public runtime::OwnedPart {
private:
	runtime::Field<int32_t> lastUsedPetTemplateId{};
	runtime::LinkedHashMap<int32_t, runtime::Ref<PetCommonData>> pets{AION_LOCK_CLASS(PetList::pets)};

public:
	/** Java package-private */
	explicit PetList(Player& player);

	~PetList() override;

	void loadPets(Player& player);

	std::vector<runtime::Ptr<PetCommonData>> getPets();

	/** @return the pet with the template id, null if the player has none */
	runtime::Ptr<PetCommonData> getPet(int32_t petId);

	/** @return the last used pet, null if there is none */
	runtime::Ptr<PetCommonData> getLastUsedPet();

	void setLastUsedPetTemplateId(int32_t value) { lastUsedPetTemplateId.set(value); }

	runtime::Ref<PetCommonData> addPet(Player& player, int32_t petId, int32_t decorationId, std::string_view name, int32_t expireTime);

	runtime::Ref<PetCommonData> addPet(Player& player, int32_t petId, int32_t decorationId, int64_t birthday, std::string_view name,
		int32_t expireTime);

	bool hasPet(int32_t templateId);

	/** @return the removed pet, null if there was none */
	runtime::Ptr<PetCommonData> deletePet(int32_t templateId);
};

} // namespace aion::gameserver::model::gameobjects::player
