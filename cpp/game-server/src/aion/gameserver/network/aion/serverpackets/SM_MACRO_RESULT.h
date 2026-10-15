#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * S0c declaration header (hub-headers.md §12). C++ difference: the two Java `public static` packet instances are static objects defined in the
 * .cpp (Java never reassigns them); sending one serializes it again, which only reads `code`.
 *
 * @author xavier
 */
class SM_MACRO_RESULT : public AionServerPacket {
public:
	static SM_MACRO_RESULT SM_MACRO_CREATED; // lint: L4 cached packet: the ported constructor only stores code, serialize() only reads it
	static SM_MACRO_RESULT SM_MACRO_DELETED; // lint: L4 cached packet: the ported constructor only stores code, serialize() only reads it

private:
	int32_t code{};

	explicit SM_MACRO_RESULT(int32_t code);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
