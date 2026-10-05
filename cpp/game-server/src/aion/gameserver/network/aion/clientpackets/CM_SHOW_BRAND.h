#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * A target mark: the team leader (or an alliance captain) sets it for the team, a solo player for himself (C_SET_BRAND).
 *
 * @author Sweetkr, Simple
 */
class CM_SHOW_BRAND : public AionClientPacket {
private:
	[[maybe_unused]] int32_t action{}; // Java @SuppressWarnings("unused")
	int32_t brandId{};
	int32_t targetObjectId{};

public:
	CM_SHOW_BRAND(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
