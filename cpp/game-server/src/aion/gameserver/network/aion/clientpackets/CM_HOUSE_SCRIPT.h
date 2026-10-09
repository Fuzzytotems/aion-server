#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * A butler or relationship crystal script of the own house set or removed (C_HOUSING_SCRIPT).
 * <p>
 * C++ only: `CM_HOUSE_SCRIPTTestAccess` (tests/cm_lz) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author Rolandas, Neon, Sykra
 */
class CM_HOUSE_SCRIPT : public AionClientPacket {
	friend struct CM_HOUSE_SCRIPTTestAccess;

private:
	int32_t address{};
	int32_t scriptId{};
	int32_t totalSize{};
	int32_t compressedSize{};
	int32_t uncompressedSize{};
	std::vector<uint8_t> scriptContent;

public:
	CM_HOUSE_SCRIPT(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
