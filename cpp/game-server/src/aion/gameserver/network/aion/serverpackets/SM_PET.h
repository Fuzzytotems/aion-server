#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/animations/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author M@xx, xTz, Rolandas
 */
class SM_PET : public AionServerPacket {
private:
	model::gameobjects::PetAction action{};
	runtime::Ref<model::gameobjects::Pet> pet{};
	int32_t petObjectId{};
	runtime::Ref<model::gameobjects::player::PetCommonData> commonData{};
	std::string petName{};
	int32_t itemObjectId{};
	std::vector<runtime::Ref<model::gameobjects::player::PetCommonData>> pets{};
	int32_t count{};
	int32_t subType{};
	int32_t shuggleEmotion{};
	bool isActing{};
	int32_t lootNpcObjId{};
	int32_t dopeAction{};
	int32_t dopeSlot{};
	int8_t animationId{};
public:
	SM_PET(int32_t subType, int32_t itemObjectId, int32_t count, model::gameobjects::Pet& pet);
	explicit SM_PET(model::gameobjects::PetAction action);
	SM_PET(int32_t petObjectId, std::string_view petName);
	explicit SM_PET(model::gameobjects::Pet& pet);
	SM_PET(model::gameobjects::player::PetCommonData& commonData, bool isAdopt);
	SM_PET(int32_t petId, int32_t petObjectId);
	/** For listing all pets on this character */
	explicit SM_PET(const std::vector<runtime::Ptr<model::gameobjects::player::PetCommonData>>& pets);
	SM_PET(model::gameobjects::PetSpecialFunction specialFunction, bool active);
	SM_PET(model::gameobjects::PetSpecialFunction specialFunction, bool active, int32_t npcObjId);
	SM_PET(int32_t dopeAction, int32_t itemId, int32_t slot);
	/** For mood only */
	SM_PET(model::gameobjects::Pet& pet, int32_t subType, int32_t shuggleEmotion);
	/** For deleting pet visually. */
	SM_PET(int32_t petObjectId, model::animations::ObjectDeleteAnimation animation);
	~SM_PET() override;
protected:
	void writeImpl(AionConnection* con) override;
private:
	void writePetData(model::gameobjects::player::PetCommonData& petCommonData);
	void writeAppearance(model::gameobjects::player::PetCommonData& petCommonData);
};

} // namespace aion::gameserver::network::aion::serverpackets
