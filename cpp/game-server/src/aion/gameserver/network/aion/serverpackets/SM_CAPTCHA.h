#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Cura
 */
class SM_CAPTCHA : public AionServerPacket {
private:
	int32_t type{};
	int32_t count{};
	int32_t size{};
	std::vector<int8_t> data{};
	bool isCorrect{};
	int32_t banTime{};

public:
	SM_CAPTCHA(int32_t count, std::span<const uint8_t> data);
	SM_CAPTCHA(bool isCorrect, int32_t banTime);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
