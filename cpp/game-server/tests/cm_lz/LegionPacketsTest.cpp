// M5h P-01 (P5-16): the eight legion client packets - CM_LEGION (C_GUILD) and its 14 arms, CM_LEGION_HISTORY, CM_LEGION_MODIFY_EMBLEM,
// CM_LEGION_SEND_EMBLEM, CM_LEGION_SEND_EMBLEM_INFO, CM_LEGION_UPLOAD_EMBLEM, CM_LEGION_UPLOAD_INFO, CM_LEGION_WH_KINAH. The read cases lay each
// body out field by field from the Java readImpl; the service calls of runImpl are LegionService's (tests/legionhouse/LegionServiceTest.cpp).

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "../support/NetworkTestSupport.h"
#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/clientpackets/CM_LEGION.h"
#include "aion/gameserver/network/aion/clientpackets/CM_LEGION_HISTORY.h"
#include "aion/gameserver/network/aion/clientpackets/CM_LEGION_MODIFY_EMBLEM.h"
#include "aion/gameserver/network/aion/clientpackets/CM_LEGION_SEND_EMBLEM.h"
#include "aion/gameserver/network/aion/clientpackets/CM_LEGION_SEND_EMBLEM_INFO.h"
#include "aion/gameserver/network/aion/clientpackets/CM_LEGION_UPLOAD_EMBLEM.h"
#include "aion/gameserver/network/aion/clientpackets/CM_LEGION_UPLOAD_INFO.h"
#include "aion/gameserver/network/aion/clientpackets/CM_LEGION_WH_KINAH.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

std::unique_ptr<AionClientPacket> CM_LEGION_clientPacketFactory(int32_t opcode, const StateSet& validStates);
std::unique_ptr<AionClientPacket> CM_LEGION_HISTORY_clientPacketFactory(int32_t opcode, const StateSet& validStates);
std::unique_ptr<AionClientPacket> CM_LEGION_WH_KINAH_clientPacketFactory(int32_t opcode, const StateSet& validStates);

/** The friends the headers declare: the fields readImpl decoded, which Java keeps private */
struct CM_LEGIONTestAccess {
	static int32_t exOpcode(const CM_LEGION& p) { return p.exOpcode; }
	static std::optional<std::string> legionName(const CM_LEGION& p) { return p.legionName; }
	static std::optional<std::string> charName(const CM_LEGION& p) { return p.charName; }
	static std::optional<std::string> newNickname(const CM_LEGION& p) { return p.newNickname; }
	static std::optional<std::string> announcement(const CM_LEGION& p) { return p.announcement; }
	static std::optional<std::string> newSelfIntro(const CM_LEGION& p) { return p.newSelfIntro; }
	static int32_t rank(const CM_LEGION& p) { return p.rank; }
	static int32_t legionDominionId(const CM_LEGION& p) { return p.legionDominionId; }
	static std::vector<int16_t> permissions(const CM_LEGION& p) {
		return {p.deputyPermission, p.centurionPermission, p.legionarPermission, p.volunteerPermission};
	}
};
struct CM_LEGION_HISTORYTestAccess {
	static int32_t page(const CM_LEGION_HISTORY& p) { return p.page; }
	static model::team::legion::LegionHistoryAction_Type type(const CM_LEGION_HISTORY& p) { return p.type; }
};
struct CM_LEGION_MODIFY_EMBLEMTestAccess {
	static std::vector<int32_t> fields(const CM_LEGION_MODIFY_EMBLEM& p) { return {p.legionId, p.emblemId, p.alpha, p.red, p.green, p.blue}; }
	static model::team::legion::LegionEmblemType emblemType(const CM_LEGION_MODIFY_EMBLEM& p) { return p.emblemType; }
};
struct CM_LEGION_SEND_EMBLEMTestAccess {
	static int32_t legionId(const CM_LEGION_SEND_EMBLEM& p) { return p.legionId; }
};
struct CM_LEGION_SEND_EMBLEM_INFOTestAccess {
	static int32_t legionId(const CM_LEGION_SEND_EMBLEM_INFO& p) { return p.legionId; }
};
struct CM_LEGION_UPLOAD_EMBLEMTestAccess {
	static int32_t size(const CM_LEGION_UPLOAD_EMBLEM& p) { return p.size; }
	static std::vector<uint8_t> data(const CM_LEGION_UPLOAD_EMBLEM& p) { return p.data; }
};
struct CM_LEGION_UPLOAD_INFOTestAccess {
	static std::vector<int32_t> fields(const CM_LEGION_UPLOAD_INFO& p) { return {p.totalSize, p.alpha, p.red, p.green, p.blue}; }
};
struct CM_LEGION_WH_KINAHTestAccess {
	static int64_t amount(const CM_LEGION_WH_KINAH& p) { return p.amount; }
	static int8_t actionType(const CM_LEGION_WH_KINAH& p) { return p.actionType; }
};

namespace testing::legion {
namespace {

using model::team::legion::LegionEmblemType;
using model::team::legion::LegionHistoryAction_Type;
using network::test::LogCapture;
using network::test::PacketWriter;

// the decoded opcodes of ClientPacketInfo.gen.inc
constexpr int32_t CM_LEGION_SEND_EMBLEM_INFO_OPCODE = 16;
constexpr int32_t CM_LEGION_OPCODE = 45;
constexpr int32_t CM_LEGION_SEND_EMBLEM_OPCODE = 47;
constexpr int32_t CM_LEGION_HISTORY_OPCODE = 55;
constexpr int32_t CM_LEGION_MODIFY_EMBLEM_OPCODE = 59;
constexpr int32_t CM_LEGION_WH_KINAH_OPCODE = 76;
constexpr int32_t CM_LEGION_UPLOAD_INFO_OPCODE = 160;
constexpr int32_t CM_LEGION_UPLOAD_EMBLEM_OPCODE = 161;

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

std::unique_ptr<CM_LEGION> legion(const PacketWriter& body, int32_t& unread) {
	return readPacket<CM_LEGION>(body.data, CM_LEGION_OPCODE, unread);
}

TEST(LegionPacketsReadTest, TheStringArmsReadTheirSkippedIntAndTheirStrings) {
	int32_t unread = -1;
	auto created = legion(PacketWriter().C(0x00).D(0x40001978).S("Founders"), unread);
	ASSERT_NE(created, nullptr);
	EXPECT_EQ(CM_LEGIONTestAccess::legionName(*created), "Founders");
	EXPECT_FALSE(CM_LEGIONTestAccess::charName(*created)) << "Java null: no char name read";
	EXPECT_EQ(unread, 0);
	for (int32_t exOpcode : {0x01, 0x04, 0x05}) {
		auto p = legion(PacketWriter().C(exOpcode).D(0).S("bravo"), unread);
		ASSERT_NE(p, nullptr) << exOpcode;
		EXPECT_EQ(CM_LEGIONTestAccess::charName(*p), "bravo") << exOpcode;
		EXPECT_EQ(unread, 0) << exOpcode;
	}
	auto rank = legion(PacketWriter().C(0x06).D(3).S("bravo"), unread);
	EXPECT_EQ(CM_LEGIONTestAccess::rank(*rank), 3);
	EXPECT_EQ(CM_LEGIONTestAccess::charName(*rank), "bravo");
	auto notice = legion(PacketWriter().C(0x09).D(0).S("hello all"), unread);
	EXPECT_EQ(CM_LEGIONTestAccess::announcement(*notice), "hello all");
	auto intro = legion(PacketWriter().C(0x0A).D(0).S("me"), unread);
	EXPECT_EQ(CM_LEGIONTestAccess::newSelfIntro(*intro), "me");
	auto nickname = legion(PacketWriter().C(0x0F).S("bravo").S("bee"), unread);
	EXPECT_EQ(CM_LEGIONTestAccess::charName(*nickname), "bravo");
	EXPECT_EQ(CM_LEGIONTestAccess::newNickname(*nickname), "bee");
	EXPECT_EQ(unread, 0);
}

TEST(LegionPacketsReadTest, TheEmptyArmsReadAnIntAndAShortAndTheRestTheirNumbers) {
	int32_t unread = -1;
	for (int32_t exOpcode : {0x02, 0x07, 0x08, 0x0E}) {
		auto p = legion(PacketWriter().C(exOpcode).D(0).H(0), unread);
		ASSERT_NE(p, nullptr) << exOpcode;
		EXPECT_EQ(unread, 0) << exOpcode;
		EXPECT_EQ(CM_LEGIONTestAccess::exOpcode(*p), exOpcode);
	}
	auto permissions = legion(PacketWriter().C(0x0D).H(0x1E0C).H(0x1C08).H(0x1800).H(0xFFFF), unread);
	EXPECT_EQ(CM_LEGIONTestAccess::permissions(*permissions), (std::vector<int16_t>{0x1E0C, 0x1C08, 0x1800, -1})) << "readH is signed";
	EXPECT_EQ(unread, 0);
	auto dominion = legion(PacketWriter().C(0x10).D(5), unread);
	EXPECT_EQ(CM_LEGIONTestAccess::legionDominionId(*dominion), 5);
	auto unsignedOpcode = legion(PacketWriter().C(0x80), unread);
	EXPECT_EQ(CM_LEGIONTestAccess::exOpcode(*unsignedOpcode), 0x80) << "readUC";
}

TEST(LegionPacketsReadTest, AnUnknownExOpcodeIsLoggedInUpperCaseHex) {
	LogCapture capture({"com.aionemu.gameserver.network.aion.clientpackets.CM_LEGION"});
	int32_t unread = -1;
	auto p = legion(PacketWriter().C(0x1B), unread);
	ASSERT_NE(p, nullptr);
	EXPECT_TRUE(capture.contains("Unknown Legion exOpcode 0x1B")) << capture.dump();
}

TEST(LegionPacketsReadTest, HistoryEmblemUploadAndKinahPackets) {
	int32_t unread = -1;
	auto history = readPacket<CM_LEGION_HISTORY>(PacketWriter().D(2).C(2).data, CM_LEGION_HISTORY_OPCODE, unread);
	ASSERT_NE(history, nullptr);
	EXPECT_EQ(CM_LEGION_HISTORYTestAccess::page(*history), 2);
	EXPECT_EQ(CM_LEGION_HISTORYTestAccess::type(*history), LegionHistoryAction_Type::WAREHOUSE);
	EXPECT_EQ(unread, 0);
	{
		LogCapture capture({"com.aionemu.commons.network.packet.BaseClientPacket"});
		EXPECT_EQ(readPacket<CM_LEGION_HISTORY>(PacketWriter().D(0).C(3).data, CM_LEGION_HISTORY_OPCODE, unread), nullptr)
			<< "Type.values()[3] throws; read() logs it and drops the packet";
		EXPECT_TRUE(capture.contains("Reading failed for packet")) << capture.dump();
	}

	auto byDefault = readPacket<CM_LEGION_MODIFY_EMBLEM>(PacketWriter().D(77).C(12).C(0).C(255).C(1).C(2).C(3).data, CM_LEGION_MODIFY_EMBLEM_OPCODE, unread);
	ASSERT_NE(byDefault, nullptr);
	EXPECT_EQ(CM_LEGION_MODIFY_EMBLEMTestAccess::fields(*byDefault), (std::vector<int32_t>{77, 12, 255, 1, 2, 3}));
	EXPECT_EQ(CM_LEGION_MODIFY_EMBLEMTestAccess::emblemType(*byDefault), LegionEmblemType::DEFAULT);
	auto custom = readPacket<CM_LEGION_MODIFY_EMBLEM>(PacketWriter().D(77).C(12).C(0x80).C(0).C(0).C(0).C(0).data, CM_LEGION_MODIFY_EMBLEM_OPCODE, unread);
	EXPECT_EQ(CM_LEGION_MODIFY_EMBLEMTestAccess::emblemType(*custom), LegionEmblemType::CUSTOM) << "any byte other than DEFAULT's 0";

	EXPECT_EQ(CM_LEGION_SEND_EMBLEMTestAccess::legionId(*readPacket<CM_LEGION_SEND_EMBLEM>(PacketWriter().D(9).data, CM_LEGION_SEND_EMBLEM_OPCODE, unread)), 9);
	EXPECT_EQ(CM_LEGION_SEND_EMBLEM_INFOTestAccess::legionId(
				  *readPacket<CM_LEGION_SEND_EMBLEM_INFO>(PacketWriter().D(10).data, CM_LEGION_SEND_EMBLEM_INFO_OPCODE, unread)),
		10);

	const std::vector<uint8_t> chunk{1, 2, 3, 250};
	auto upload = readPacket<CM_LEGION_UPLOAD_EMBLEM>(PacketWriter().D(4).B(chunk).data, CM_LEGION_UPLOAD_EMBLEM_OPCODE, unread);
	ASSERT_NE(upload, nullptr);
	EXPECT_EQ(CM_LEGION_UPLOAD_EMBLEMTestAccess::size(*upload), 4);
	EXPECT_EQ(CM_LEGION_UPLOAD_EMBLEMTestAccess::data(*upload), chunk);
	EXPECT_EQ(unread, 0);

	auto info = readPacket<CM_LEGION_UPLOAD_INFO>(PacketWriter().D(16000).C(255).C(10).C(20).C(30).data, CM_LEGION_UPLOAD_INFO_OPCODE, unread);
	ASSERT_NE(info, nullptr);
	EXPECT_EQ(CM_LEGION_UPLOAD_INFOTestAccess::fields(*info), (std::vector<int32_t>{16000, 255, 10, 20, 30}));

	auto kinah = readPacket<CM_LEGION_WH_KINAH>(PacketWriter().Q(5000000000LL).C(1).data, CM_LEGION_WH_KINAH_OPCODE, unread);
	ASSERT_NE(kinah, nullptr);
	EXPECT_EQ(CM_LEGION_WH_KINAHTestAccess::amount(*kinah), 5000000000LL);
	EXPECT_EQ(CM_LEGION_WH_KINAHTestAccess::actionType(*kinah), 1);
	EXPECT_EQ(unread, 0);
}

TEST(LegionPacketsReadTest, TheMarkersRegisterTheClassesUnderTheirJavaOpcodes) {
	const StateSet inGame{AionConnection_State::IN_GAME};
	EXPECT_NE(dynamic_cast<CM_LEGION*>(CM_LEGION_clientPacketFactory(CM_LEGION_OPCODE, inGame).get()), nullptr);
	EXPECT_NE(dynamic_cast<CM_LEGION_HISTORY*>(CM_LEGION_HISTORY_clientPacketFactory(CM_LEGION_HISTORY_OPCODE, inGame).get()), nullptr);
	EXPECT_NE(dynamic_cast<CM_LEGION_WH_KINAH*>(CM_LEGION_WH_KINAH_clientPacketFactory(CM_LEGION_WH_KINAH_OPCODE, inGame).get()), nullptr);
}

} // namespace
} // namespace testing::legion
} // namespace aion::gameserver::network::aion::clientpackets
