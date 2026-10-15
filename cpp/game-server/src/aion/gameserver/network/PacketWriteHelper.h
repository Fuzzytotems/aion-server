#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/fwd.h"

namespace aion::gameserver::network {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author -Nemesiss-
 */
class PacketWriteHelper : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
protected:
	virtual void writeMe(commons::utils::ByteBuffer& buf) = 0;

	/** Write int to buffer. */
	static void writeD(commons::utils::ByteBuffer& buf, int32_t value);

	/** Write short to buffer. */
	static void writeH(commons::utils::ByteBuffer& buf, int32_t value);

	/** Write byte to buffer. */
	static void writeC(commons::utils::ByteBuffer& buf, int32_t value);

	/** Write double to buffer. */
	static void writeDF(commons::utils::ByteBuffer& buf, double value);

	/** Write float to buffer. */
	static void writeF(commons::utils::ByteBuffer& buf, float value);

	/** Write long to buffer. */
	static void writeQ(commons::utils::ByteBuffer& buf, int64_t value);

	/** Write String to buffer (Java null writes the same bytes as an empty string: pass "") */
	static void writeS(commons::utils::ByteBuffer& buf, std::string_view text);

	/** Write String to buffer (Java null writes the same bytes as an empty string: pass "") */
	static void writeS(commons::utils::ByteBuffer& buf, std::string_view text, int32_t size);

	/** Write byte array to buffer. */
	static void writeB(commons::utils::ByteBuffer& buf, std::span<const uint8_t> data);

	/** Skip specified amount of bytes */
	static void skip(commons::utils::ByteBuffer& buf, int32_t bytes);

	static void writeDyeInfo(commons::utils::ByteBuffer& buf, std::optional<int32_t> rgb);

	~PacketWriteHelper() override;
};

} // namespace aion::gameserver::network
