#include "aion/gameserver/network/aion/serverpackets/SM_TIME_CHECK.h"

#include <chrono>

#include "aion/commons/utils/info/SystemInfo.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_TIME_CHECK::SM_TIME_CHECK(int32_t nanoTimeValue)
	: AionServerPacket(opcodeOf<SM_TIME_CHECK>), nanoTime(nanoTimeValue) {
	// Java: (int) ManagementFactory.getRuntimeMXBean().getUptime(), the milliseconds since the process started
	const auto uptime = std::chrono::system_clock::now() - commons::utils::info::SystemInfo::getProcessStartTime();
	serverUpTime = static_cast<int32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(uptime).count());
}

void SM_TIME_CHECK::writeImpl(AionConnection* con) {
	writeD(serverUpTime);
	writeD(nanoTime);
}

} // namespace aion::gameserver::network::aion::serverpackets
