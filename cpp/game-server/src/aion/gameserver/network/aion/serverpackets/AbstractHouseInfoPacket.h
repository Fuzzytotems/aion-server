#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * Base of the packets that write the common house info (SM_HOUSE_RENDER, SM_HOUSE_UPDATE).
 * <p>
 * S0c declaration header (hub-headers.md §12). C++ difference: the constructor takes the subclass' opcode first (`opcodeOf<SM_X>`), which
 * Java takes from the dynamic class.
 *
 * @author Neon
 */
class AbstractHouseInfoPacket : public AionServerPacket {
public:
	static constexpr int32_t SIGN_NOTICE_MAX_LENGTH = 64;

protected:
	runtime::Ref<model::house::House> house;

	/** Java: AbstractHouseInfoPacket(House house); C++: plus the subclass' opcode */
	AbstractHouseInfoPacket(int32_t opCode, model::house::House& house);

public:
	~AbstractHouseInfoPacket() override;

protected:
	void writeCommonInfo();
};

} // namespace aion::gameserver::network::aion::serverpackets
