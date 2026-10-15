#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * So far I've found only one usage for this packet - to stop character blinking (just after login into game, player's character is blinking)<br>
 * states: 0 - normal char, 1- crouched invisible char, 64 - standing blinking char 128 - char is invisible
 *
 * @author Luno, Sweetkr
 */
class SM_PLAYER_STATE : public AionServerPacket {
private:
	int32_t playerObjId{};
	int32_t visualState{};
	int32_t seeState{};
public:
	explicit SM_PLAYER_STATE(model::gameobjects::Creature& creature);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
