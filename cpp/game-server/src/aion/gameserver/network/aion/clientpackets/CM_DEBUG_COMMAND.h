#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/clientpackets/AbstractGmCommandPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Sent in the following cases:<br>
 * - Executing commands prefixed by / or //// in the command tab of the GM Panel (Shift + F1) if "Builder control (///)" is selected in the settings tab of
 *   the GM Dialog (Shift + G)<br>
 * - Executing commands prefixed by ///// in the command tab of the GM Panel (Shift + F1) if "Builder command (//)" is selected in the settings tab of
 *   the GM Dialog (Shift + G)<br>
 * - Executing commands prefixed by //// in macros if the console has been activated via "\con_disable_console 0" from the command tab of the GM Panel<br>
 */
class CM_DEBUG_COMMAND : public AbstractGmCommandPacket {
public:
	CM_DEBUG_COMMAND(int32_t opcode, const StateSet& validStates);

protected:
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
