#include "RegscanTestSupport.h"

#include <algorithm>

#include "Emitter.h"

using namespace aion::gameserver::tools::regscan;
using namespace aion::gameserver::tools::regscan::test;

namespace {

std::string handlerFile(std::string_view ns, std::string_view body) {
	return "#include \"aion/gameserver/handlers/HandlerRegistry.h\"\n\nnamespace " + std::string(ns) + " {\n" + std::string(body) + "\n}\n";
}

ScanInput handlersOnly(std::vector<SourceFile> files) {
	ScanInput input;
	input.handlersGiven = true;
	input.handlerFiles = std::move(files);
	std::ranges::sort(input.handlerFiles, {}, &SourceFile::relPath);
	return input;
}

const ReportSection& section(const RegistryModel& model, std::string_view title) {
	auto it = std::ranges::find(model.report, title, &ReportSection::title);
	if (it == model.report.end())
		throw std::logic_error("no report section " + std::string(title));
	return *it;
}

} // namespace

TEST(RegistryTest, EntriesAreSortedAndDescribed) {
	RegistryModel model = buildRegistry(handlersOnly({
		source("aion/gameserver/handlers/ai/ZetaAI.cpp", handlerFile("aion::gameserver::handlers::ai", "class ZetaAI {};\nAION_AI(ZetaAI, \"zeta\");")),
		source("aion/gameserver/handlers/ai/AlphaAI.cpp", handlerFile("aion::gameserver::handlers::ai", "class AlphaAI {};\nAION_AI(AlphaAI, \"alpha\");")),
		source("aion/gameserver/handlers/instance/B.cpp", handlerFile("aion::gameserver::handlers::instance", "class B {};\nAION_INSTANCE_HANDLER(B, 300200000);")),
		source("aion/gameserver/handlers/instance/A.cpp", handlerFile("aion::gameserver::handlers::instance", "class A {};\nAION_INSTANCE_HANDLER(A, 300100000);")),
		source("aion/gameserver/handlers/quest/template/_2.cpp", handlerFile("aion::gameserver::handlers::quest::template_", "class _2 {};\nAION_QUEST_HANDLER(_2, 2);")),
		source("aion/gameserver/handlers/consolecommands/C.cpp", handlerFile("aion::gameserver::handlers::consolecommands", "class C {};\nAION_CONSOLE_COMMAND(C);")),
		source("aion/gameserver/handlers/admincommands/Z.cpp", handlerFile("aion::gameserver::handlers::admincommands", "class Z {};\nAION_ADMIN_COMMAND(Z);")),
		source("aion/gameserver/handlers/admincommands/A.cpp", handlerFile("aion::gameserver::handlers::admincommands", "class A {};\nAION_ADMIN_COMMAND(A);")),
	}));
	EXPECT_NO_ERRORS(model.errors);
	ASSERT_EQ(model.ai.size(), 2u);
	EXPECT_EQ(model.ai[0].key, "alpha");
	EXPECT_EQ(model.ai[0].javaClass, "ai.AlphaAI");
	EXPECT_EQ(model.ai[0].function, "AlphaAI_aiFactory");
	EXPECT_EQ(model.ai[0].ns, "aion::gameserver::handlers::ai");
	EXPECT_EQ(model.ai[0].source, "ai/AlphaAI.cpp:5");
	ASSERT_EQ(model.instances.size(), 2u);
	EXPECT_EQ(model.instances[0].number, 300100000);
	ASSERT_EQ(model.quests.size(), 1u);
	EXPECT_EQ(model.quests[0].javaClass, "quest.template._2");
	EXPECT_EQ(model.quests[0].ns, "aion::gameserver::handlers::quest::template_");
	ASSERT_EQ(model.commands.size(), 3u);
	EXPECT_EQ(model.commands[0].javaClass, "admincommands.A");
	EXPECT_EQ(model.commands[1].javaClass, "admincommands.Z");
	EXPECT_EQ(model.commands[2].javaClass, "consolecommands.C");
	EXPECT_EQ(model.filesPerCategory.at("admincommands"), 2u);

	// without Java input the report has no Java column
	EXPECT_EQ(section(model, "ai").ported, 2u);
	EXPECT_FALSE(section(model, "ai").java.has_value());
	EXPECT_EQ(summaryLine(model),
		"ai 2/-, instance 2/-, zone names 0/-, quest 1/-, admin commands 2/-, player commands 0/-, console commands 1/-, client packets 0/-, npc ids spawned by handlers 0/-");
}

TEST(RegistryTest, DuplicateKeysAndClassesFail) {
	const std::string ai = "aion::gameserver::handlers::ai";
	RegistryModel model = buildRegistry(handlersOnly({
		source("aion/gameserver/handlers/ai/A.cpp", handlerFile(ai, "class A {};\nAION_AI(A, \"same\");")),
		source("aion/gameserver/handlers/ai/B.cpp", handlerFile(ai, "class B {};\nAION_AI(B, \"same\");")),
		source("aion/gameserver/handlers/ai/C.cpp", handlerFile(ai, "class C {};\nAION_AI(C, \"c1\");\nAION_AI(C, \"c2\");")),
		source("aion/gameserver/handlers/ai/D.cpp", handlerFile(ai, "AION_AI(Undefined, \"d\");")),
		source("aion/gameserver/handlers/ai/E.cpp", handlerFile(ai, "class A {};")),
		source("aion/gameserver/handlers/instance/I1.cpp", handlerFile("aion::gameserver::handlers::instance", "class I1 {};\nAION_INSTANCE_HANDLER(I1, 1);")),
		source("aion/gameserver/handlers/instance/I2.cpp", handlerFile("aion::gameserver::handlers::instance", "class I2 {};\nAION_INSTANCE_HANDLER(I2, 1);")),
		source("aion/gameserver/handlers/quest/Q1.cpp", handlerFile("aion::gameserver::handlers::quest", "class Q1 {};\nAION_QUEST_HANDLER(Q1, 7);")),
		source("aion/gameserver/handlers/quest/Q2.cpp", handlerFile("aion::gameserver::handlers::quest", "class Q2 {};\nAION_QUEST_HANDLER(Q2, 7);")),
		source("aion/gameserver/handlers/zone/Z1.cpp", handlerFile("aion::gameserver::handlers::zone", "class Z1 {};\nAION_ZONE_HANDLER(Z1, \"A B\");")),
		source("aion/gameserver/handlers/zone/Z2.cpp", handlerFile("aion::gameserver::handlers::zone", "class Z2 {};\nAION_ZONE_HANDLER(Z2, \"C B\");")),
		source("aion/gameserver/handlers/zone/Z3.cpp", handlerFile("aion::gameserver::handlers::zone", "class Z3 {};\nAION_ZONE_HANDLER(Z3, \"D D\");")),
	}));
	EXPECT_ERROR(model.errors, 5, "AI name \"same\" is already registered by A at aion/gameserver/handlers/ai/A.cpp(5)");
	EXPECT_ERROR(model.errors, 6, "class C is already registered by AION_AI at aion/gameserver/handlers/ai/C.cpp(5)");
	EXPECT_ERROR(model.errors, 4, "class Undefined of AION_AI is not defined in namespace aion::gameserver::handlers::ai");
	EXPECT_ERROR(model.errors, 4, "type A is already defined in namespace aion::gameserver::handlers::ai at aion/gameserver/handlers/ai/A.cpp(4)");
	EXPECT_ERROR(model.errors, 5, "map id 1 is already registered by I1");
	EXPECT_ERROR(model.errors, 5, "quest id 7 is already registered by Q1");
	EXPECT_ERROR(model.errors, 5, "zone name B is already registered by Z1");
	EXPECT_ERROR(model.errors, 5, "zone name D is listed twice");
	EXPECT_TRUE(std::is_sorted(model.errors.begin(), model.errors.end()));
	// the first registration of each key stays in the tables
	ASSERT_EQ(model.ai.size(), 2u);
	EXPECT_EQ(model.ai[0].key, "c1");
	EXPECT_EQ(model.ai[1].key, "same");
	EXPECT_EQ(model.instances.size(), 1u);
}

TEST(RegistryTest, JavaCrossChecks) {
	ScanInput input = handlersOnly({
		source("aion/gameserver/handlers/ai/GoodAI.cpp", handlerFile("aion::gameserver::handlers::ai", "class GoodAI {};\nAION_AI(GoodAI, \"good\");")),
		source("aion/gameserver/handlers/ai/RenamedAI.cpp", handlerFile("aion::gameserver::handlers::ai", "class RenamedAI {};\nAION_AI(RenamedAI, \"typo\");")),
		source("aion/gameserver/handlers/ai/ThiefAI.cpp", handlerFile("aion::gameserver::handlers::ai", "class ThiefAI {};\nAION_AI(ThiefAI, \"good_other\");")),
		source("aion/gameserver/handlers/ai/NewAI.cpp", handlerFile("aion::gameserver::handlers::ai", "class NewAI {};\nAION_AI(NewAI, \"new\");")),
		source("aion/gameserver/handlers/quest/_100Q.cpp", handlerFile("aion::gameserver::handlers::quest", "class _100Q {};\nAION_QUEST_HANDLER(_100Q, 101);")),
		source("aion/gameserver/handlers/instance/I.cpp", handlerFile("aion::gameserver::handlers::instance", "class I {};\nAION_INSTANCE_HANDLER(I, 5);")),
		source("aion/gameserver/handlers/zone/Z.cpp", handlerFile("aion::gameserver::handlers::zone", "class Z {};\nAION_ZONE_HANDLER(Z, \"A B\");")),
		source("aion/gameserver/handlers/playercommands/Cmd.cpp", handlerFile("aion::gameserver::handlers::playercommands", "class Cmd {};\nAION_PLAYER_COMMAND(Cmd);")),
	});
	input.javaHandlersGiven = true;
	input.javaHandlerFiles = {
		source("ai/GoodAI.java", "package ai;\n@AIName(\"good\")\npublic class GoodAI extends NpcAI {}\n"),
		source("ai/RenamedAI.java", "package ai;\n@AIName(\"renamed\")\npublic class RenamedAI extends NpcAI {}\n"),
		source("ai/OtherAI.java", "package ai;\n@AIName(\"good_other\")\npublic class OtherAI extends NpcAI {}\n"),
		source("ai/ThiefAI.java", "package ai;\n@AIName(\"thief\")\npublic class ThiefAI extends NpcAI {}\n"),
		source("quest/_100Q.java", "package quest;\npublic class _100Q extends AbstractQuestHandler {\n\tpublic _100Q() {\n\t\tsuper(100);\n\t}\n}\n"),
		source("instance/I.java", "package instance;\npublic class I extends GeneralInstanceHandler {}\n"),
		source("zone/Z.java", "package zone;\n@ZoneNameAnnotation(value = \"A B\", questId = 3)\npublic class Z extends QuestZoneHandler {}\n"),
		source("playercommands/Cmd.java", "package playercommands;\npublic class Cmd extends AdminCommand {}\n"),
	};
	RegistryModel model = buildRegistry(input);
	EXPECT_ERROR(model.errors, 5, "AI name \"typo\" differs from Java @AIName(\"renamed\") of ai.RenamedAI");
	EXPECT_ERROR(model.errors, 5, "AI name \"good_other\" belongs to Java class ai.OtherAI");
	EXPECT_ERROR(model.errors, 5, "quest id 101 differs from Java super(100) of quest._100Q");
	EXPECT_ERROR(model.errors, 5, "Java class instance.I registers no @InstanceID");
	EXPECT_ERROR(model.errors, 5, "zone names/questId (\"A B\", 0) differ from Java @ZoneNameAnnotation(\"A B\", 3) of zone.Z");
	EXPECT_ERROR(model.errors, 5, "AION_PLAYER_COMMAND does not match Java playercommands.Cmd, which extends AdminCommand");
	EXPECT_FALSE(hasError(model.errors, 0, "GoodAI"));
	EXPECT_FALSE(hasError(model.errors, 0, "NewAI")) << "C++ classes without a Java counterpart are reported, not rejected";

	const ReportSection& aiSection = section(model, "ai");
	EXPECT_EQ(aiSection.ported, 4u);
	EXPECT_EQ(aiSection.java, 4u);
	EXPECT_EQ(aiSection.missing, (std::vector<std::string>{"renamed\tai.RenamedAI", "thief\tai.ThiefAI"}));
	EXPECT_EQ(aiSection.unknownToJava, (std::vector<std::string>{"new\tai.NewAI"}));
	EXPECT_EQ(section(model, "admin commands").java, 1u);
	EXPECT_EQ(section(model, "player commands").java, 0u);
}

TEST(RegistryTest, ClientPacketsAgainstTheJavaFactory) {
	ScanInput input;
	input.clientPacketsGiven = true;
	input.clientPacketFiles = {
		source("aion/gameserver/network/aion/clientpackets/CM_MOVE.cpp", handlerFile("aion::gameserver::network::aion::clientpackets", "class CM_MOVE {};\nAION_CLIENT_PACKET(CM_MOVE);")),
		source("aion/gameserver/network/aion/clientpackets/CM_FAKE.cpp", handlerFile("aion::gameserver::network::aion::clientpackets", "class CM_FAKE {};\nAION_CLIENT_PACKET(CM_FAKE);")),
	};
	input.javaClientPacketFactory = source("AionClientPacketFactory.java", "packets[48] = new PacketInfo<>(CM_MOVE.class, State.IN_GAME);\npackets[3] = new PacketInfo<>(CM_QUIT.class, State.IN_GAME);\n");
	RegistryModel model = buildRegistry(input);
	EXPECT_ERROR(model.errors, 5, "CM_FAKE is not in the client packet table of AionClientPacketFactory.java");
	ASSERT_EQ(model.clientPackets.size(), 2u);
	EXPECT_EQ(model.clientPackets[0].key, "CM_FAKE");
	EXPECT_EQ(model.clientPackets[1].source, "network/aion/clientpackets/CM_MOVE.cpp:5");
	EXPECT_EQ(model.clientPackets[1].function, "CM_MOVE_clientPacketFactory");
	const ReportSection& packets = section(model, "client packets");
	EXPECT_EQ(packets.java, 2u);
	EXPECT_EQ(packets.missing, (std::vector<std::string>{"CM_QUIT"}));
}

TEST(RegistryTest, NpcIdsComeFromAiInstanceAndQuestFiles) {
	RegistryModel model = buildRegistry(handlersOnly({
		source("aion/gameserver/handlers/ai/A.h", "#pragma once\nnamespace aion::gameserver::handlers::ai {\n// spawn(200001)\n}\n"),
		source("aion/gameserver/handlers/instance/I.cpp", "namespace aion::gameserver::handlers::instance {\nint f() { return sp(200002, 1); }\n}\n"),
		source("aion/gameserver/handlers/quest/Q.cpp", "namespace aion::gameserver::handlers::quest {\nint g() { return spawn(x ? 200003 : 200004); }\n}\n"),
		source("aion/gameserver/handlers/zone/Z.cpp", "namespace aion::gameserver::handlers::zone {\nint h() { return spawn(200005); }\n}\n"),
		source("aion/gameserver/handlers/admincommands/C.cpp", "namespace aion::gameserver::handlers::admincommands {\nint h() { return spawn(200006); }\n}\n"),
	}));
	EXPECT_NO_ERRORS(model.errors);
	EXPECT_EQ(model.npcIds, (std::set<int32_t>{200001, 200002, 200003, 200004}));
}

TEST(RegistryTest, JavaClassNameOfKeywordDirectories) {
	Marker marker{MarkerKind::QUEST_HANDLER, "_1", "", 1, "", "", "aion/gameserver/handlers/quest/template_/_1.cpp", 1, 1};
	EXPECT_EQ(javaClassName(marker), "quest.template._1");
	marker.relPath = "aion/gameserver/handlers/quest/delete_me/_1.cpp";
	EXPECT_EQ(javaClassName(marker), "quest.delete_me._1");
	marker.kind = MarkerKind::CLIENT_PACKET;
	marker.className = "CM_MOVE";
	EXPECT_EQ(javaClassName(marker), "CM_MOVE");
}
