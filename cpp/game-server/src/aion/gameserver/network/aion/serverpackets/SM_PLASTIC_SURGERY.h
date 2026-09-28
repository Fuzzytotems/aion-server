#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author IlBuono
 */
class SM_PLASTIC_SURGERY : public AionServerPacket {
private:
	int32_t playerObjId{};
	bool hasTicket{};
	bool isGenderSwitch{};
public:
	SM_PLASTIC_SURGERY(model::gameobjects::player::Player& player, bool isGenderSwitch);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
