#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author xTz
 */
class SM_TRANSFORM_IN_SUMMON : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::player::Player> player{};
	int32_t summonObject{};
public:
	SM_TRANSFORM_IN_SUMMON(model::gameobjects::player::Player& player, model::gameobjects::Creature& creature);
	SM_TRANSFORM_IN_SUMMON(model::gameobjects::player::Player& player, int32_t creatureObjectId);
	~SM_TRANSFORM_IN_SUMMON() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
