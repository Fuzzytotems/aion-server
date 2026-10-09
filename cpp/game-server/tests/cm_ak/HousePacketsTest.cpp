// M5h HS-1 (P5-15): the studio client packets CM_HOUSE_EDIT, CM_HOUSE_DECORATE, CM_HOUSE_SETTINGS, CM_HOUSE_SCRIPT, CM_HOUSE_KICK. The read cases
// lay each body out field by field from the Java readImpl. The run cases reach HousingService (Player.getActiveHouse), whose construction reads
// the database, so they live with the database fixture in tests/legionhouse/HousePacketRunTest.cpp.

#include "ItemPacketTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/aion/clientpackets/CM_HOUSE_DECORATE.h"
#include "aion/gameserver/network/aion/clientpackets/CM_HOUSE_EDIT.h"
#include "aion/gameserver/network/aion/clientpackets/CM_HOUSE_KICK.h"
#include "aion/gameserver/network/aion/clientpackets/CM_HOUSE_SCRIPT.h"
#include "aion/gameserver/network/aion/clientpackets/CM_HOUSE_SETTINGS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_EDIT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_REGISTRY.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_SCRIPTS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

std::unique_ptr<AionClientPacket> CM_HOUSE_EDIT_clientPacketFactory(int32_t opcode, const StateSet& validStates);
std::unique_ptr<AionClientPacket> CM_HOUSE_SCRIPT_clientPacketFactory(int32_t opcode, const StateSet& validStates);

/** The friends the headers declare: the fields readImpl decoded, which Java keeps private */
struct CM_HOUSE_EDITTestAccess {
	static std::vector<int32_t> ints(const CM_HOUSE_EDIT& p) { return {p.action, p.itemObjectId, p.rotation, p.buildingId}; }
	static std::vector<float> position(const CM_HOUSE_EDIT& p) { return {p.x, p.y, p.z}; }
};
struct CM_HOUSE_DECORATETestAccess {
	static std::vector<int32_t> fields(const CM_HOUSE_DECORATE& p) { return {p.objectId, p.lineNo}; }
};
struct CM_HOUSE_SETTINGSTestAccess {
	static int8_t doorState(const CM_HOUSE_SETTINGS& p) { return p.doorState; }
	static bool showOwnerName(const CM_HOUSE_SETTINGS& p) { return p.showOwnerName; }
	static std::string signNotice(const CM_HOUSE_SETTINGS& p) { return p.signNotice; }
};
struct CM_HOUSE_SCRIPTTestAccess {
	static std::vector<int32_t> fields(const CM_HOUSE_SCRIPT& p) { return {p.address, p.scriptId, p.totalSize, p.compressedSize, p.uncompressedSize}; }
	static std::vector<uint8_t> content(const CM_HOUSE_SCRIPT& p) { return p.scriptContent; }
};
struct CM_HOUSE_KICKTestAccess {
	static int8_t option(const CM_HOUSE_KICK& p) { return p.option; }
};

namespace testing {
namespace {

using network::test::PacketWriter;

using serverpackets::SM_HOUSE_EDIT;
using serverpackets::SM_HOUSE_REGISTRY;
using serverpackets::SM_HOUSE_SCRIPTS;
using serverpackets::SM_SYSTEM_MESSAGE;

// the decoded opcodes of ClientPacketInfo.gen.inc
constexpr int32_t CM_HOUSE_SCRIPT_OPCODE = 30;
constexpr int32_t CM_HOUSE_KICK_OPCODE = 72;
constexpr int32_t CM_HOUSE_SETTINGS_OPCODE = 73;
constexpr int32_t CM_HOUSE_DECORATE_OPCODE = 75;
constexpr int32_t CM_HOUSE_EDIT_OPCODE = 82;

template <class P>
std::unique_ptr<P> readPacket(const std::vector<uint8_t>& data, int32_t opcode, int32_t& unread) {
	std::vector<uint8_t> copy = data;
	auto packet = std::make_unique<P>(opcode, StateSet{AionConnection_State::IN_GAME});
	packet->setBuffer(commons::utils::ByteBuffer::wrap(copy));
	if (!packet->read())
		return nullptr;
	unread = packet->getRemainingBytes();
	return packet;
}

TEST(HousePacketsReadTest, HouseEditReadsTheFieldsOfEachAction) {
	int32_t unread = -1;
	for (int32_t action : {3, 4, 7}) {
		auto p = readPacket<CM_HOUSE_EDIT>(PacketWriter().C(action).D(0x01020304).data, CM_HOUSE_EDIT_OPCODE, unread);
		ASSERT_NE(p, nullptr);
		EXPECT_EQ(CM_HOUSE_EDITTestAccess::ints(*p), (std::vector<int32_t>{action, 0x01020304, 0, 0})) << action;
		EXPECT_EQ(unread, 0);
	}
	for (int32_t action : {5, 6}) {
		auto p = readPacket<CM_HOUSE_EDIT>(PacketWriter().C(action).D(77).F(1.5f).F(2.5f).F(3.5f).H(0xFFFF).data, CM_HOUSE_EDIT_OPCODE, unread);
		ASSERT_NE(p, nullptr);
		EXPECT_EQ(CM_HOUSE_EDITTestAccess::ints(*p), (std::vector<int32_t>{action, 77, 0xFFFF, 0})) << "readUH: the rotation is unsigned";
		EXPECT_EQ(CM_HOUSE_EDITTestAccess::position(*p), (std::vector<float>{1.5f, 2.5f, 3.5f}));
		EXPECT_EQ(unread, 0);
	}
	auto renovation = readPacket<CM_HOUSE_EDIT>(PacketWriter().C(16).D(25051).data, CM_HOUSE_EDIT_OPCODE, unread);
	EXPECT_EQ(CM_HOUSE_EDITTestAccess::ints(*renovation), (std::vector<int32_t>{16, 0, 0, 25051}));
	auto mode = readPacket<CM_HOUSE_EDIT>(PacketWriter().C(1).data, CM_HOUSE_EDIT_OPCODE, unread);
	EXPECT_EQ(CM_HOUSE_EDITTestAccess::ints(*mode)[0], 1);
	EXPECT_EQ(unread, 0) << "actions 1, 2, 14, 15 read nothing more";
}

TEST(HousePacketsReadTest, DecorateSettingsScriptAndKick) {
	int32_t unread = -1;
	auto decorate = readPacket<CM_HOUSE_DECORATE>(PacketWriter().D(5000).D(170000023).H(0x8001).data, CM_HOUSE_DECORATE_OPCODE, unread);
	ASSERT_NE(decorate, nullptr);
	EXPECT_EQ(CM_HOUSE_DECORATETestAccess::fields(*decorate), (std::vector<int32_t>{5000, 0x8001})) << "the template id is skipped, lineNo readUH";
	EXPECT_EQ(unread, 0);

	auto settings = readPacket<CM_HOUSE_SETTINGS>(PacketWriter().C(3).C(1).S("welcome").data, CM_HOUSE_SETTINGS_OPCODE, unread);
	ASSERT_NE(settings, nullptr);
	EXPECT_EQ(CM_HOUSE_SETTINGSTestAccess::doorState(*settings), 3);
	EXPECT_TRUE(CM_HOUSE_SETTINGSTestAccess::showOwnerName(*settings));
	EXPECT_EQ(CM_HOUSE_SETTINGSTestAccess::signNotice(*settings), "welcome");
	auto hidden = readPacket<CM_HOUSE_SETTINGS>(PacketWriter().C(1).C(2).S("").data, CM_HOUSE_SETTINGS_OPCODE, unread);
	EXPECT_FALSE(CM_HOUSE_SETTINGSTestAccess::showOwnerName(*hidden)) << "only 1 shows the owner name";

	const std::vector<uint8_t> script{9, 8, 7, 6};
	auto set = readPacket<CM_HOUSE_SCRIPT>(PacketWriter().D(1001).C(2).H(12).D(4).D(40).B(script).data, CM_HOUSE_SCRIPT_OPCODE, unread);
	ASSERT_NE(set, nullptr);
	EXPECT_EQ(CM_HOUSE_SCRIPTTestAccess::fields(*set), (std::vector<int32_t>{1001, 2, 12, 4, 40}));
	EXPECT_EQ(CM_HOUSE_SCRIPTTestAccess::content(*set), script);
	EXPECT_EQ(unread, 0);
	auto removal = readPacket<CM_HOUSE_SCRIPT>(PacketWriter().D(1001).C(3).H(0).data, CM_HOUSE_SCRIPT_OPCODE, unread);
	EXPECT_EQ(CM_HOUSE_SCRIPTTestAccess::fields(*removal), (std::vector<int32_t>{1001, 3, 0, 0, 0})) << "a zero total size reads no more";
	const int32_t tooLarge = SM_HOUSE_SCRIPTS::MAX_COMPRESSED_SCRIPT_SIZE + 1;
	auto overflow = readPacket<CM_HOUSE_SCRIPT>(PacketWriter().D(1001).C(4).H(100).D(tooLarge).D(77).data, CM_HOUSE_SCRIPT_OPCODE, unread);
	ASSERT_NE(overflow, nullptr);
	EXPECT_EQ(CM_HOUSE_SCRIPTTestAccess::fields(*overflow), (std::vector<int32_t>{1001, 4, 100, tooLarge, 0})) << "an oversized script is not read";
	EXPECT_EQ(unread, 4) << "neither the uncompressed size nor the content";

	auto kick = readPacket<CM_HOUSE_KICK>(PacketWriter().C(2).H(0).data, CM_HOUSE_KICK_OPCODE, unread);
	ASSERT_NE(kick, nullptr);
	EXPECT_EQ(CM_HOUSE_KICKTestAccess::option(*kick), 2);
	EXPECT_EQ(unread, 0);
}

TEST(HousePacketsReadTest, TheMarkersRegisterTheClasses) {
	const StateSet inGame{AionConnection_State::IN_GAME};
	EXPECT_NE(dynamic_cast<CM_HOUSE_EDIT*>(CM_HOUSE_EDIT_clientPacketFactory(CM_HOUSE_EDIT_OPCODE, inGame).get()), nullptr);
	EXPECT_NE(dynamic_cast<CM_HOUSE_SCRIPT*>(CM_HOUSE_SCRIPT_clientPacketFactory(CM_HOUSE_SCRIPT_OPCODE, inGame).get()), nullptr);
}

} // namespace
} // namespace testing
} // namespace aion::gameserver::network::aion::clientpackets
