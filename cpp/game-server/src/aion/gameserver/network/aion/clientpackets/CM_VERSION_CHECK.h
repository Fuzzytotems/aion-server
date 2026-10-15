#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The first packet of the client after SM_KEY: its version. Answered with SM_VERSION_CHECK.
 *
 * @author -Nemesiss-
 */
class CM_VERSION_CHECK : public AionClientPacket {
private:
	int32_t aionClientVersion{};
	int32_t npcScriptInterfaceVersion{}; // Java: @SuppressWarnings("unused")
	int32_t windowsEncoding{};           // Java: @SuppressWarnings("unused")
	int32_t windowsVersion{};            // Java: @SuppressWarnings("unused")
	int32_t windowsSubVersion{};         // Java: @SuppressWarnings("unused")
	int32_t liteInfo{};                  // Java: @SuppressWarnings("unused")

public:
	CM_VERSION_CHECK(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
