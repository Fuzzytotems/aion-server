#pragma once

#include <cstdint>

#include "aion/gameserver/model/team/legion/LegionHistoryAction_Type.h"

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The legion history page of one type (C_REQUEST_GUILD_HISTORY); the reward history only for the brigade general.
 * <p>
 * C++ only: `CM_LEGION_HISTORYTestAccess` (tests/cm_lz) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author Simple, xTz, Sykra
 */
class CM_LEGION_HISTORY : public AionClientPacket {
	friend struct CM_LEGION_HISTORYTestAccess;

private:
	int32_t page{};
	model::team::legion::LegionHistoryAction_Type type{};

public:
	CM_LEGION_HISTORY(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
