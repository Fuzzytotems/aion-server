#include "aion/gameserver/network/PacketWriteHelper.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network {

void PacketWriteHelper::writeD(commons::utils::ByteBuffer& buf, int32_t value) {
	AION_UNPORTED();
}

void PacketWriteHelper::writeH(commons::utils::ByteBuffer& buf, int32_t value) {
	AION_UNPORTED();
}

void PacketWriteHelper::writeC(commons::utils::ByteBuffer& buf, int32_t value) {
	AION_UNPORTED();
}

void PacketWriteHelper::writeDF(commons::utils::ByteBuffer& buf, double value) {
	AION_UNPORTED();
}

void PacketWriteHelper::writeF(commons::utils::ByteBuffer& buf, float value) {
	AION_UNPORTED();
}

void PacketWriteHelper::writeQ(commons::utils::ByteBuffer& buf, int64_t value) {
	AION_UNPORTED();
}

void PacketWriteHelper::writeS(commons::utils::ByteBuffer& buf, std::string_view text) {
	AION_UNPORTED();
}

void PacketWriteHelper::writeS(commons::utils::ByteBuffer& buf, std::string_view text, int32_t size) {
	AION_UNPORTED();
}

void PacketWriteHelper::writeB(commons::utils::ByteBuffer& buf, std::span<const uint8_t> data) {
	AION_UNPORTED();
}

void PacketWriteHelper::skip(commons::utils::ByteBuffer& buf, int32_t bytes) {
	AION_UNPORTED();
}

void PacketWriteHelper::writeDyeInfo(commons::utils::ByteBuffer& buf, std::optional<int32_t> rgb) {
	AION_UNPORTED();
}

PacketWriteHelper::~PacketWriteHelper() = default;

} // namespace aion::gameserver::network
