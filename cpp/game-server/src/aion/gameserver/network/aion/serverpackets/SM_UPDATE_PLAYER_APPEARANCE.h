#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/AbstractPlayerInfoPacket.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Avol, ATracer, Neon
 */
class SM_UPDATE_PLAYER_APPEARANCE : public AbstractPlayerInfoPacket {
private:
	int32_t playerId{};
	std::vector<runtime::Ref<model::gameobjects::Item>> items{};
public:
	SM_UPDATE_PLAYER_APPEARANCE(int32_t playerId, const std::vector<runtime::Ptr<model::gameobjects::Item>>& items);
	~SM_UPDATE_PLAYER_APPEARANCE() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
