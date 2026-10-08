#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The legion actions of the legion window and the legion chat commands (C_GUILD): create, invite, leave, kick, brigade general, rank, notice, info, announcement, self introduction, permissions, level up, nickname, legion dominion.
 * <p>
 * C++ only: `CM_LEGIONTestAccess` (tests/cm_lz) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author Simple
 */
class CM_LEGION : public AionClientPacket {
	friend struct CM_LEGIONTestAccess;

private:
	int32_t exOpcode{};
	int16_t deputyPermission{};
	int16_t centurionPermission{};
	int16_t legionarPermission{};
	int16_t volunteerPermission{};
	int32_t rank{};
	int32_t legionDominionId{};
	std::optional<std::string> legionName;
	std::optional<std::string> charName;
	std::optional<std::string> newNickname;
	std::optional<std::string> announcement;
	std::optional<std::string> newSelfIntro;

public:
	CM_LEGION(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
