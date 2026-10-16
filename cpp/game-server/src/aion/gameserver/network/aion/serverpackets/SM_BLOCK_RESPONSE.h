#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * Responses to block list related requests
 *
 * @author Ben
 */
class SM_BLOCK_RESPONSE : public AionServerPacket {
public:
	static constexpr int32_t BLOCK_SUCCESSFUL = 0;
	static constexpr int32_t UNBLOCK_SUCCESSFUL = 1;
	static constexpr int32_t TARGET_NOT_FOUND = 2;
	static constexpr int32_t LIST_FULL = 3;
	static constexpr int32_t CANT_BLOCK_SELF = 4;
	static constexpr int32_t EDIT_NOTE = 5;

private:
	int32_t code{};
	std::string playerName{};

public:
	/** Constructs a new block request response packet */
	SM_BLOCK_RESPONSE(int32_t code, std::string_view playerName);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
