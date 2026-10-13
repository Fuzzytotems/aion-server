// M5j item 3 (owner's decision 2026-10-13): the commands that read their own XML - //send (Send.java), ///teleport_to_named, ///addskill,
// ///combineskill, ///deleteskill (the handler data npcs.xml and skills.xml) and ///wish (a read-only stream over npcs.xml and
// item_templates.xml) - on a real Player with a real AionConnection (CommandTestSupport.h), in a scratch working directory with small data files.
// The texts are the Java literals.

#include "CommandTestSupport.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/SkillTreeData.bind.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/handlers/admincommands/Send.h"
#include "aion/gameserver/handlers/consolecommands/Addskill.h"
#include "aion/gameserver/handlers/consolecommands/Combineskill.h"
#include "aion/gameserver/handlers/consolecommands/Deleteskill.h"
#include "aion/gameserver/handlers/consolecommands/Teleport_to_named.h"
#include "aion/gameserver/handlers/consolecommands/Wish.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUSTOM_PACKET.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using serverpackets::SM_CUSTOM_PACKET;

class HandlerXmlCommandsTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		for (const char* alias : {"send", "teleport_to_named", "addskill", "combineskill", "deleteskill", "wish"})
			levels[alias] = 3;
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
		previous = std::filesystem::current_path();
		dir = std::filesystem::temp_directory_path() / ("aion_gs_handler_xml_" + std::to_string(std::random_device()()));
		write("data/handlers/consolecommands/data/npcs.xml", "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<npcs>\n\t<npc id=\"200000\" name=\"SkillZone\"/>\n"
			"\t<npc id=\"798007\" name=\"Minalinerk\"/>\n</npcs>\n");
		write("data/handlers/consolecommands/data/skills.xml", "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<skills>\n\t<skill id=\"1\" name=\"RA_Light_WhiteTiger_G1\"/>\n"
			"\t<skill id=\"30002\" name=\"Harvest\"/>\n</skills>\n");
		std::filesystem::current_path(dir);
	}

	void TearDown() override {
		std::filesystem::current_path(previous);
		std::error_code ignored;
		std::filesystem::remove_all(dir, ignored);
		for (Player* player : stored)
			world::World::getInstance().removeObject(*player);
		stored.clear();
		CommandTest::TearDown();
		if (skillTreePublished)
			dataholders::DataManager::SKILL_TREE_DATA.resetForTests();
	}

	void write(const std::string& relative, std::string_view text) const {
		std::filesystem::create_directories((dir / relative).parent_path());
		std::ofstream out(dir / relative, std::ios::binary);
		out << text;
	}

	Player& gm(int32_t objectId) {
		Player& player = connected(objectId, "Warden", 3);
		spawnInPoeta(player);
		world::World::getInstance().storeObject(player);
		stored.push_back(&player);
		return player;
	}

	std::filesystem::path previous;
	std::filesystem::path dir;
	std::vector<Player*> stored;
	bool skillTreePublished = false;
	xml::LoadContext skillContext;
};

/** Send.java:43-87: the syntax, a missing file, and the packets of a file: their elements (${objectId}, ${targetObjectId}, repeat) and delays */
TEST_F(HandlerXmlCommandsTest, SendSchedulesTheFilesPackets) {
	Player& admin = gm(731401);
	handlers::admincommands::Send send;
	EXPECT_TRUE(send.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(send.getSyntaxInfo()));
	client()->clearSent();
	EXPECT_TRUE(send.process(admin, args({"none"})));
	EXPECT_EQ(client()->sentBytes(), info("File none.xml not found"));

	write("data/packets/demo.xml", "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<packets delay=\"100\">\n"
		"\t<packet opcode=\"0x10\" delay=\"50\">\n\t\t<part type=\"d\" value=\"${objectId}\" repeat=\"2\"/>\n\t\t<part type=\"s\" value=\"x${targetObjectId}\"/>\n\t</packet>\n"
		"\t<packet opcode=\"17\">\n\t\t<part type=\"c\" value=\"7\"/>\n\t</packet>\n</packets>\n");
	client()->clearSent();
	EXPECT_TRUE(send.process(admin, args({"demo"})));
	EXPECT_TRUE(client()->sentBytes().empty()) << "scheduled";
	auto& executor = dynamic_cast<runtime::DeterministicExecutor&>(*utils::ThreadPoolManager::installedBackend());
	SM_CUSTOM_PACKET first(0x10);
	first.addElement(SM_CUSTOM_PACKET::PacketElementType::D, std::to_string(admin.getObjectId()));
	first.addElement(SM_CUSTOM_PACKET::PacketElementType::D, std::to_string(admin.getObjectId()));
	first.addElement(SM_CUSTOM_PACKET::PacketElementType::S, "x0");
	SM_CUSTOM_PACKET second(17);
	second.addElement(SM_CUSTOM_PACKET::PacketElementType::C, "7");
	const std::vector<uint8_t> firstBytes = serialized(std::move(first), client().con());
	const std::vector<uint8_t> secondBytes = serialized(std::move(second), client().con());
	executor.advance(std::chrono::milliseconds(49));
	EXPECT_TRUE(client()->sentBytes().empty());
	executor.advance(std::chrono::milliseconds(1));
	EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{firstBytes}) << "the first packet's delay";
	executor.advance(std::chrono::milliseconds(99));
	EXPECT_EQ(client()->sentBytes().size(), 1u);
	executor.advance(std::chrono::milliseconds(1));
	EXPECT_EQ(client()->sentBytes(), (std::vector<std::vector<uint8_t>>{firstBytes, secondBytes})) << "plus the file's delay";
}

/** Teleport_to_named.java:20-35: the syntax, an unknown name, a known name (case-insensitive) */
TEST_F(HandlerXmlCommandsTest, TeleportToNamed) {
	Player& admin = gm(731402);
	handlers::consolecommands::Teleport_to_named command;
	EXPECT_TRUE(command.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{message("syntax ///teleport_to_named <named name>")});
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"Nobody"})));
	EXPECT_TRUE(client()->sentBytes().empty());
	command.process(admin, args({"minalinerk"})); // TeleportService.teleportToNpc answers on its own (no spawn here)
	const std::vector<std::vector<uint8_t>> sent = client()->sentBytes();
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.front(), message("Teleporting to Npc: 798007"));
}

/** Addskill.java:17-40, Combineskill.java:19-50, Deleteskill.java:20-64 over the handler data skills.xml */
TEST_F(HandlerXmlCommandsTest, AddCombineAndDeleteSkill) {
	Player& admin = gm(731403);
	dataholders::DataManager::SKILL_DATA.resetForTests(); // the base fixture's holder; its TearDown resets this one
	dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext,
		R"(<skill_data><skill_template skill_id="30002" name="Essencetapping" nameId="282933" stack="GATHERING_B" lvl="1" skilltype="NONE")" // skill_templates.xml:204328
		R"( skillsubtype="NONE" tslot="NONE" activation="NONE" cooldown="0" duration="0"><motion name="axe"/></skill_template>)"
		R"(</skill_data>)"));
	if (!dataholders::DataManager::SKILL_TREE_DATA) { // PlayerSkillList.addSkill asks the skill tree (no rows needed)
		dataholders::DataManager::SKILL_TREE_DATA.publish(xml::bindString<dataholders::SkillTreeData>(skillContext, "<skill_tree/>"));
		skillTreePublished = true;
	}
	handlers::consolecommands::Addskill addskill;
	handlers::consolecommands::Combineskill combineskill;
	handlers::consolecommands::Deleteskill deleteskill;
	EXPECT_TRUE(addskill.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{message("syntax ///addskill <skill name>")});
	client()->clearSent();
	EXPECT_TRUE(combineskill.process(admin, args({"harvest"})));
	EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{message("syntax ///addcskill <skill name>")});
	client()->clearSent();
	EXPECT_TRUE(combineskill.process(admin, args({"harvest", "x"})));
	EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{message("Parameters need to be an integer.")});
	client()->clearSent();
	EXPECT_TRUE(addskill.process(admin, args({"Unknown_Skill"})));
	EXPECT_TRUE(client()->sentBytes().empty()) << "no such skill";
	EXPECT_TRUE(deleteskill.process(admin, args({"HARVEST"})));
	EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{message("Player dont have this skill.")});

	client()->clearSent();
	EXPECT_TRUE(addskill.process(admin, args({"harvest"})));
	ASSERT_TRUE(admin.getSkillList()->isSkillPresent(30002));
	EXPECT_EQ(admin.getSkillList()->getSkillLevel(30002), 1);
	EXPECT_EQ(client()->sentBytes().back(), message("You have success add skill"));
	client()->clearSent();
	EXPECT_TRUE(combineskill.process(admin, args({"Harvest", "5"})));
	EXPECT_EQ(admin.getSkillList()->getSkillLevel(30002), 5);
	EXPECT_EQ(client()->sentBytes().back(), message("You have success add skill"));
	client()->clearSent();
	EXPECT_TRUE(deleteskill.process(admin, args({"harvest"})));
	EXPECT_FALSE(admin.getSkillList()->isSkillPresent(30002));
	EXPECT_EQ(client()->sentBytes().back(), message("You have successfully deleted the specified skill."));
}

/** Wish.java:40-141: the syntax, an unknown npc, an unknown item, and the stream finding an item by its client name past a comment, a
 * CDATA section and an escaped name (the item is then found: the id reaches AdminService, which needs the item data this test has not got) */
TEST_F(HandlerXmlCommandsTest, WishFindsIdsInTheStream) {
	Player& admin = gm(731404);
	write("data/static_data/items/item_templates.xml", "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<item_templates>\n"
		"\t<!-- <item_template id=\"1\" cName=\"Axe\"/> -->\n\t<desc><![CDATA[<item_template id=\"2\" cName=\"Bow\"/>]]></desc>\n"
		"\t<item_template\n\t\tid=\"100000001\"\n\t\tcName=\"A&amp;B\"/>\n\t<item_template id=\"100000002\" cName='Sword'>\n\t</item_template>\n</item_templates>\n");
	handlers::consolecommands::Wish wish;
	EXPECT_TRUE(wish.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(wish.getSyntaxInfo()));
	client()->clearSent();
	EXPECT_TRUE(wish.process(admin, args({"nobody"})));
	EXPECT_EQ(client()->sentBytes(), info("There is no NPC with that name."));
	client()->clearSent();
	EXPECT_TRUE(wish.process(admin, args({"2", "Shield"})));
	EXPECT_EQ(client()->sentBytes(), info("There is no item named Shield."));
	for (const char* hidden : {"Axe", "Bow"}) { // in a comment and in a CDATA section
		client()->clearSent();
		EXPECT_TRUE(wish.process(admin, args({"1", hidden})));
		EXPECT_EQ(client()->sentBytes(), info(std::string("There is no item named ") + hidden + "."));
	}
	for (std::vector<std::string> params : {args({"1", "sword"}), args({"a&b", "3"})}) {
		client()->clearSent();
		EXPECT_TRUE(wish.process(admin, params));
		EXPECT_EQ(client()->sentBytes(), info("<Error while executing command>")) << params[0] << ": found, then AdminService without item data";
	}
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
