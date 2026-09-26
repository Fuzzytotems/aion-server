#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author ATracer, Neon
 */
class SM_PET_EMOTE : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::Pet> pet{};
	model::gameobjects::PetEmote emote{};
	int32_t emotionId{};
	int32_t param1{};
public:
	SM_PET_EMOTE(model::gameobjects::Pet& pet, model::gameobjects::PetEmote emote);
	SM_PET_EMOTE(model::gameobjects::Pet& pet, model::gameobjects::PetEmote emote, int32_t emotionId, int32_t param1);
	~SM_PET_EMOTE() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
