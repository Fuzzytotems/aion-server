#include "aion/gameserver/network/aion/AionServerPacket.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion {

int32_t AionServerPacket::byteLengthForString(std::string_view text) {
	AION_UNPORTED();
}

int32_t AionServerPacket::byteLengthForFixedString(int32_t fixedLength) {
	AION_UNPORTED();
}

AionServerPacket::AionServerPacket(int32_t opCode) : BaseServerPacket(opCode) {
}

AionServerPacket::~AionServerPacket() = default;

void AionServerPacket::writeOP() {
	AION_UNPORTED();
}

SerializedBody AionServerPacket::serialize(AionConnection* con) {
	AION_UNPORTED();
}

commons::utils::ByteBuffer& AionServerPacket::getBuf() {
	AION_UNPORTED();
}

void AionServerPacket::writeD(int32_t value) {
	AION_UNPORTED();
}

void AionServerPacket::writeH(int32_t value) {
	AION_UNPORTED();
}

void AionServerPacket::writeC(int32_t value) {
	AION_UNPORTED();
}

void AionServerPacket::writeDF(double value) {
	AION_UNPORTED();
}

void AionServerPacket::writeF(float value) {
	AION_UNPORTED();
}

void AionServerPacket::writeQ(int64_t value) {
	AION_UNPORTED();
}

void AionServerPacket::writeS(std::string_view text) {
	AION_UNPORTED();
}

void AionServerPacket::writeB(std::span<const uint8_t> data) {
	AION_UNPORTED();
}

void AionServerPacket::writeS(std::string_view text, int32_t fixedLength) {
	AION_UNPORTED();
}

void AionServerPacket::writeDyeInfo(std::optional<int32_t> rgb) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion
