#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The start of a custom emblem upload: its total size and colours (C_UPLOAD_GUILD_EMBLEM_IMG_BEGIN).
 * <p>
 * C++ only: `CM_LEGION_UPLOAD_INFOTestAccess` (tests/cm_lz) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author Simple, cura
 */
class CM_LEGION_UPLOAD_INFO : public AionClientPacket {
	friend struct CM_LEGION_UPLOAD_INFOTestAccess;

private:
	/** Emblem related information **/
	int32_t totalSize{};
	int32_t alpha{};
	int32_t red{};
	int32_t green{};
	int32_t blue{};

public:
	CM_LEGION_UPLOAD_INFO(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
