#include "aion/gameserver/utils/collections/DynamicServerPacketBodySplitList.h"

#include "aion/gameserver/network/aion/AionServerPacket.h"

namespace aion::gameserver::utils::collections::detail {

int32_t maxUsablePacketBodySize() noexcept {
	return network::aion::AionServerPacket::MAX_USABLE_PACKET_BODY_SIZE;
}

} // namespace aion::gameserver::utils::collections::detail
