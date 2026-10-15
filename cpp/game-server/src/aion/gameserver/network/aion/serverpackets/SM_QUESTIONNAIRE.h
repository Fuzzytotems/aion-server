#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * Sends Survey HTML data to the client. This packet can be splitted over max 255 packets The max length of the HTML may therefore be 255 * 65525 byte
 *
 * @author lhw and Kaipo
 */
class SM_QUESTIONNAIRE : public AionServerPacket {
private:
	int32_t messageId{};
	int8_t chunk{};
	int8_t count{};
	std::string html{};
public:
	SM_QUESTIONNAIRE(int32_t messageId, int8_t chunk, int8_t count, std::string_view html);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
