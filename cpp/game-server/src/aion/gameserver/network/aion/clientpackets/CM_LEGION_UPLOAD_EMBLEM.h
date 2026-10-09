#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * A chunk of a custom emblem image (C_UPLOAD_GUILD_EMBLEM_IMG_DATA).
 * <p>
 * C++ only: `CM_LEGION_UPLOAD_EMBLEMTestAccess` (tests/cm_lz) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author Simple
 */
class CM_LEGION_UPLOAD_EMBLEM : public AionClientPacket {
	friend struct CM_LEGION_UPLOAD_EMBLEMTestAccess;

private:
	/** Emblem related information **/
	int32_t size{};
	std::vector<uint8_t> data;

public:
	CM_LEGION_UPLOAD_EMBLEM(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
