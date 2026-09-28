#pragma once

#include <cstdint>

#include "aion/gameserver/model/animations/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author ATracer
 */
class SM_ACTION_ANIMATION : public AionServerPacket {
private:
	int32_t targetObjectId{};
	model::animations::ActionAnimation actionAnimation{};
	int32_t levelOrObjectId{};

public:
	SM_ACTION_ANIMATION(int32_t targetObjectId, model::animations::ActionAnimation actionAnimation);
	SM_ACTION_ANIMATION(int32_t targetObjectId, model::animations::ActionAnimation actionAnimation, int32_t levelOrObjectId);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
