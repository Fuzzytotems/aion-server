#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author xTz, KID
 */
class SM_REPURCHASE : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::player::Player> player{};
	int32_t targetObjectId{};
	std::vector<runtime::Ref<model::gameobjects::Item>> items{};
public:
	SM_REPURCHASE(model::gameobjects::player::Player& player, int32_t npcId);
	~SM_REPURCHASE() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
