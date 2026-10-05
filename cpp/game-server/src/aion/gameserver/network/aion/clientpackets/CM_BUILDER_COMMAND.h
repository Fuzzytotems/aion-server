#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/clientpackets/AbstractGmCommandPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Sent in the following cases:<br>
 * - Clicking buttons in the GM Dialog (Shift + G)<br>
 * - Clicking buttons in the GM Panel (Shift + F1) if "Builder command (//)" is selected in the settings tab of the GM Dialog (Shift + G)<br>
 * - Executing commands prefixed by // (optional) in the command tab of the GM Panel (Shift + F1)<br>
 * - Executing commands prefixed by // in macros if the console has been activated via "\con_disable_console 0" from the command tab of the GM Panel<br>
 *
 * @author ginho1
 */
class CM_BUILDER_COMMAND : public AbstractGmCommandPacket {
public:
	CM_BUILDER_COMMAND(int32_t opcode, const StateSet& validStates);
};

} // namespace aion::gameserver::network::aion::clientpackets
