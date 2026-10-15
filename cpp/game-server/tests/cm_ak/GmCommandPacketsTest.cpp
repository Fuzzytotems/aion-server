// m5j K-06: the GM command packets CM_BUILDER_COMMAND, CM_BUILDER_CONTROL and CM_DEBUG_COMMAND on AbstractGmCommandPacket
// (AbstractGmCommandPacket.java, CM_BUILDER_COMMAND.java, CM_BUILDER_CONTROL.java, CM_DEBUG_COMMAND.java), driven on a real Player with a real
// AionConnection (InWorldPacketRunSupport.h), and AbstractGmCommandPacket.replaceUnsupportedCommandChars.

#include "InWorldPacketRunSupport.h"

#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/network/aion/clientpackets/AbstractGmCommandPacket.h"
#include "aion/gameserver/network/aion/clientpackets/CM_BUILDER_COMMAND.h"
#include "aion/gameserver/network/aion/clientpackets/CM_BUILDER_CONTROL.h"
#include "aion/gameserver/network/aion/clientpackets/CM_DEBUG_COMMAND.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using network::test::LogCapture;
using network::test::PacketWriter;

/** the decoded opcodes (ClientPacketInfo.gen.inc:52, :53, :158) */
constexpr int32_t BUILDER_COMMAND_OPCODE = 41;
constexpr int32_t BUILDER_CONTROL_OPCODE = 42;
constexpr int32_t DEBUG_COMMAND_OPCODE = 180;

class GmCommandPacketsTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		actor = makePlayer(730300, 731300, "Warden");
		client = std::make_unique<TestClient>();
		client->enterWorld(actor);
		(*client)->clearSent();
	}

	void TearDown() override {
		actor.player->setClientConnection(nullptr);
		client.reset();
		actor = {};
		InWorldPacketTest::TearDown();
	}

	/** what ChatProcessor.handleConsoleCommand answers for an unknown console command: PacketSendUtility.sendMessage */
	std::vector<uint8_t> notImplemented(std::string_view name) {
		return serialized(serverpackets::SM_MESSAGE(0, "", "The command " + std::string(name) + " is not implemented.", model::ChatType::GOLDEN_YELLOW),
			client->con());
	}

	PlayerFixture actor;
	std::unique_ptr<TestClient> client;
};

TEST_F(GmCommandPacketsTest, BuilderCommandAndControlHandTheStringToTheConsoleCommands) {
	Driver<CM_BUILDER_COMMAND> command(BUILDER_COMMAND_OPCODE);
	command.readAndRun(PacketWriter().S("nosuchcmd 1 2").data, client->get());
	EXPECT_EQ((*client)->sentBytes(), exactly({notImplemented("nosuchcmd")})) << "ChatProcessor.handleConsoleCommand";

	(*client)->clearSent();
	Driver<CM_BUILDER_CONTROL> control(BUILDER_CONTROL_OPCODE);
	control.readAndRun(PacketWriter().S("other").data, client->get());
	EXPECT_EQ((*client)->sentBytes(), exactly({notImplemented("other")}));

	(*client)->clearSent();
	Driver<CM_BUILDER_COMMAND> empty(BUILDER_COMMAND_OPCODE);
	empty.readAndRun(PacketWriter().S("").data, client->get());
	EXPECT_TRUE((*client)->sentBytes().empty()) << "an empty command is ignored";
}

TEST_F(GmCommandPacketsTest, TheDebugCommandIsOnlyAudited) {
	LogCapture audit({"ADMINAUDIT_LOG"});
	Driver<CM_DEBUG_COMMAND> debug(DEBUG_COMMAND_OPCODE);
	debug.readAndRun(PacketWriter().S("show fps").data, client->get());
	EXPECT_TRUE((*client)->sentBytes().empty()) << "no console command runs";
	EXPECT_TRUE(audit.contains(actor.player->toString() + " sent debug command ////show fps")) << audit.dump();
}

TEST_F(GmCommandPacketsTest, ReplaceUnsupportedCommandCharsReplacesEveryCodePointAboveU013E) {
	EXPECT_EQ(AbstractGmCommandPacket::replaceUnsupportedCommandChars(u"Abcľ"), u"Abcľ") << "U+0000..U+013E stay";
	EXPECT_EQ(AbstractGmCommandPacket::replaceUnsupportedCommandChars(u"aĿb한"), u"a?b?");
	EXPECT_EQ(AbstractGmCommandPacket::replaceUnsupportedCommandChars(u"x\U0001F600y"), u"x?y") << "a surrogate pair is one code point";
	EXPECT_EQ(AbstractGmCommandPacket::replaceUnsupportedCommandChars(std::u16string(1, u'\xD800') + u"z"), u"?z") << "a lone surrogate";
	EXPECT_EQ(AbstractGmCommandPacket::UNSUPPORTED_COMMAND_CHAR_PLACEHOLDER, u"?");
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
