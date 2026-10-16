#include <gtest/gtest.h>

#include <fmt/format.h>

#include "aion/commons/network/packet/BasePacket.h"

using namespace aion::commons::network::packet;

namespace {

class SM_TEST_PACKET : public BasePacket {
public:
	explicit SM_TEST_PACKET(int32_t opCode) : BasePacket(opCode) {}
};

class PaddedPacket : public BasePacket {
public:
	PaddedPacket() { setOpCode(0x1F); }

protected:
	int32_t getOpCodeZeroPadding() const override { return 5; }
};

class NamedPacket : public BasePacket {
public:
	NamedPacket() : BasePacket(1) {}
	std::string getPacketName() const override { return "CM_CUSTOM"; }
};

template <typename T>
class TemplatePacket : public BasePacket {
public:
	TemplatePacket() : BasePacket(2) {}
};

} // namespace

TEST(BasePacketTest, PacketNameIsSimpleClassName) {
	EXPECT_EQ(SM_TEST_PACKET(7).getPacketName(), "SM_TEST_PACKET");
	EXPECT_EQ((TemplatePacket<std::pair<int, std::string>>().getPacketName()), "TemplatePacket");
	EXPECT_EQ(NamedPacket().getPacketName(), "CM_CUSTOM");
}

TEST(BasePacketTest, FormattedPacketNameString) {
	EXPECT_EQ(SM_TEST_PACKET(7).toFormattedPacketNameString(), "[007] SM_TEST_PACKET");
	EXPECT_EQ(SM_TEST_PACKET(1234).toString(), "[1234] SM_TEST_PACKET");
	EXPECT_EQ(PaddedPacket().toString(), "[00031] PaddedPacket");
	EXPECT_EQ(PaddedPacket().getOpCode(), 0x1F);
	EXPECT_EQ(NamedPacket().toString(), "[001] CM_CUSTOM");
}

TEST(BasePacketTest, StaticFormatMatchesJavaStringFormat) {
	EXPECT_EQ(BasePacket::toFormattedPacketNameString(3, 5, "CM_UNK"), "[005] CM_UNK");
	EXPECT_EQ(BasePacket::toFormattedPacketNameString(3, -5, "CM_UNK"), "[-05] CM_UNK"); // Java: String.format("%03d", -5)
	EXPECT_EQ(BasePacket::toFormattedPacketNameString(2, 999, "X"), "[999] X");
	EXPECT_EQ(BasePacket::toFormattedPacketNameString(0, 42, "X"), "[42] X");
}

TEST(BasePacketTest, FormattableWithFmt) {
	SM_TEST_PACKET packet(12);
	EXPECT_EQ(fmt::format("sent {}", packet), "sent [012] SM_TEST_PACKET");
}
