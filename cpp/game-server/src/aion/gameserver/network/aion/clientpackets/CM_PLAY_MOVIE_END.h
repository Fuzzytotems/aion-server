#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Returns data sent in {@link com.aionemu.gameserver.network.aion.serverpackets.SM_PLAY_MOVIE} after the cutscene has finished or is skipped.
 * <p>
 * C++ only: `CM_PLAY_MOVIE_ENDTestAccess` (tests/cm_lz/AscensionPacketsTest.cpp) reads the fields readImpl decoded, which Java keeps private
 * without a getter.
 *
 * @author MrPoke
 */
class CM_PLAY_MOVIE_END : public AionClientPacket {
	friend struct CM_PLAY_MOVIE_ENDTestAccess;

private:
	int8_t type{};
	int32_t targetObjectId{};
	int32_t questId{};
	int32_t movieId{};
	[[maybe_unused]] bool canSkip{}; // Java: @SuppressWarnings({ "unused", "FieldCanBeLocal" })

public:
	CM_PLAY_MOVIE_END(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
