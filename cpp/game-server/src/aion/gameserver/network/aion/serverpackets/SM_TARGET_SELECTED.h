#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Sweetkr, -Enomine-
 */
class SM_TARGET_SELECTED : public AionServerPacket {
private:
	int32_t targetObjId{};
	int32_t level{};
	int32_t maxHp{};
	int32_t currentHp{};
	int32_t maxMp{};
	int32_t currentMp{};
public:
	explicit SM_TARGET_SELECTED(runtime::Ptr<model::gameobjects::VisibleObject> target);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
