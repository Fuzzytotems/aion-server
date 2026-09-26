#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

class SM_ATTACK_RESPONSE : public AionServerPacket {
private:
	int32_t message{};
	int32_t attackCount{};

public:
	static SM_ATTACK_RESPONSE TARGET_IN_DIFFERENT_AREA(int32_t count);
	static SM_ATTACK_RESPONSE STOP_INVALID_TARGET(int32_t count);
	static SM_ATTACK_RESPONSE TARGET_TOO_FAR_AWAY(int32_t count);
	static SM_ATTACK_RESPONSE STOP_OBSTACLE_IN_THE_WAY(int32_t count);
	static SM_ATTACK_RESPONSE STOP_TOO_CLOSE_TO_ATTACK(int32_t count);
	static SM_ATTACK_RESPONSE STOP_WITHOUT_MESSAGE(int32_t count);

private:
	SM_ATTACK_RESPONSE(int32_t message, int32_t attackCount);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
