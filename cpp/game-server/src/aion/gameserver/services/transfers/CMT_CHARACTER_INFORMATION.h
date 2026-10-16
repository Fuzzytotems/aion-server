#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "aion/commons/logging/Logger.h"
#include "aion/commons/network/packet/BaseClientPacket.h"
#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/transfers/fwd.h"

namespace aion::gameserver::services::transfers {

/**
 * The character data of a player transfer, read from the login server's buffer (PlayerTransfer.getDB) into a new character of the target
 * account. Java extends BaseClientPacket only for its read helpers: it is never received, read or run as a packet.
 * <p>
 * C++: a new class of the transfer (m5j-plan.md §18.1 S-08, header request m5j-s1-03); the constructor is public (Java: protected, reached from
 * PlayerTransferService in the same package).
 *
 * @author KID
 */
class CMT_CHARACTER_INFORMATION : public commons::network::packet::BaseClientPacket<network::aion::AionConnection> {
public:
	explicit CMT_CHARACTER_INFORMATION(commons::utils::ByteBuffer byteBuffer);

	void run() override {}

	/**
	 * Creates and stores the character the buffer describes.
	 *
	 * @return the new character, null if it could not be stored
	 */
	runtime::Ptr<model::gameobjects::player::Player> readInfo(std::string_view name, int32_t targetAccount, std::string_view accountName,
		const std::vector<int32_t>& rsList, const commons::logging::Logger& textLog);

protected:
	void readImpl() override {}

	void runImpl() override {}
};

} // namespace aion::gameserver::services::transfers
