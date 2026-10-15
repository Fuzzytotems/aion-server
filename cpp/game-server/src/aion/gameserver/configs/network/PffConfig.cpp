#include "aion/gameserver/configs/network/PffConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::network {

void PffConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.network.pff.mode", PFF_MODE, "1");
	AION_BIND_PATTERN(p, "^gameserver\\.network\\.pff\\.packet\\.(0[xX][0-9a-fA-F]+)$", THRESHOLD_MILLIS_BY_PACKET_OPCODE);
}

} // namespace aion::gameserver::configs::network
