#include "RegscanTestSupport.h"

using namespace aion::gameserver::tools::regscan;
using namespace aion::gameserver::tools::regscan::test;

namespace {

constexpr std::string_view AI_FILE = "aion/gameserver/handlers/ai/TestAI.cpp";

} // namespace

TEST(SourceScannerTest, ExpectedNamespaceAndKeywordSegments) {
	EXPECT_EQ(expectedNamespace("aion/gameserver/handlers/ai/instance/darkPoeta/X.cpp"), "aion::gameserver::handlers::ai::instance::darkPoeta");
	EXPECT_EQ(expectedNamespace("aion/gameserver/handlers/quest/template/X.cpp"), "aion::gameserver::handlers::quest::template_");
	EXPECT_EQ(expectedNamespace("aion/gameserver/handlers/quest/template_/X.cpp"), "aion::gameserver::handlers::quest::template_");
	EXPECT_EQ(expectedNamespace("X.cpp"), "");
}

TEST(SourceScannerTest, ValidMarkersOfEveryKind) {
	FileScan ai = scanHandler(AI_FILE, R"cpp(
#include "aion/gameserver/handlers/HandlerRegistry.h"

namespace aion::gameserver::handlers::ai {

class TestAI final : public Base {
public:
	void f() { if (x) { g(); } }
};
AION_AI(TestAI, "test_ai"); // comment after the marker

} // namespace aion::gameserver::handlers::ai
)cpp");
	EXPECT_NO_ERRORS(ai.errors);
	ASSERT_EQ(ai.markers.size(), 1u);
	EXPECT_EQ(ai.markers[0].kind, MarkerKind::AI);
	EXPECT_EQ(ai.markers[0].className, "TestAI");
	EXPECT_EQ(ai.markers[0].text, "test_ai");
	EXPECT_EQ(ai.markers[0].ns, "aion::gameserver::handlers::ai");
	EXPECT_EQ(ai.markers[0].line, 10u);
	EXPECT_EQ(ai.markers[0].column, 1u);
	ASSERT_EQ(ai.types.size(), 1u);
	EXPECT_EQ(ai.types[0].name, "TestAI");
	EXPECT_EQ(ai.types[0].ns, "aion::gameserver::handlers::ai");

	FileScan zone = scanHandler("aion/gameserver/handlers/zone/Z.cpp", "namespace aion::gameserver::handlers::zone {\nstruct Z {};\nAION_ZONE_HANDLER(Z, \"A B\", 12);\nstruct W {};\nAION_ZONE_HANDLER(W, \"C\");\n}\n");
	EXPECT_NO_ERRORS(zone.errors);
	ASSERT_EQ(zone.markers.size(), 2u);
	EXPECT_EQ(zone.markers[0].text, "A B");
	EXPECT_EQ(zone.markers[0].number, 12);
	EXPECT_EQ(zone.markers[1].number, std::nullopt);

	FileScan quest = scanHandler("aion/gameserver/handlers/quest/template/_1.cpp",
		"namespace aion::gameserver::handlers::quest::template_ {\nclass _1 final : public A<B, C> {};\nAION_QUEST_HANDLER(_1, 1500);\n}\n");
	EXPECT_NO_ERRORS(quest.errors);
	ASSERT_EQ(quest.markers.size(), 1u);
	EXPECT_EQ(quest.markers[0].number, 1500);

	FileScan commands = scanHandler("aion/gameserver/handlers/admincommands/Add.cpp",
		"namespace aion {\nnamespace gameserver::handlers {\nnamespace admincommands {\nclass Add {};\nAION_ADMIN_COMMAND(Add);\n}\n}\n}\n");
	EXPECT_NO_ERRORS(commands.errors);
	ASSERT_EQ(commands.markers.size(), 1u);
	EXPECT_EQ(commands.markers[0].ns, "aion::gameserver::handlers::admincommands");

	FileScan instance = scanHandler("aion/gameserver/handlers/instance/I.cpp", "namespace aion::gameserver::handlers::instance {\nclass I {};\nAION_INSTANCE_HANDLER(I, 300110000);\n}\n");
	EXPECT_NO_ERRORS(instance.errors);
	EXPECT_EQ(instance.markers.at(0).number, 300110000);

	FileScan packet = scanClientPacket("aion/gameserver/network/aion/clientpackets/CM_MOVE.cpp", R"cpp(
namespace aion::gameserver::network::aion::clientpackets {
namespace { static const int x = 0; }
static void helper() {}
AION_CLIENT_PACKET(CM_MOVE);
}
)cpp");
	EXPECT_NO_ERRORS(packet.errors);
	ASSERT_EQ(packet.markers.size(), 1u);
	EXPECT_EQ(packet.markers[0].kind, MarkerKind::CLIENT_PACKET);
}

TEST(SourceScannerTest, CommentedAndQuotedMarkersAreIgnored) {
	FileScan scan = scanHandler(AI_FILE, R"cpp(
// AION_AI(Commented, "x");
/* AION_AI(Block, "y"); */
namespace aion::gameserver::handlers::ai {
const char* s = "AION_AI(Quoted, \"z\");";
const char* r = R"(AION_AI(Raw, "w");)";
}
)cpp");
	EXPECT_NO_ERRORS(scan.errors);
	EXPECT_TRUE(scan.markers.empty());
}

TEST(SourceScannerTest, MarkerSyntaxViolations) {
	auto scan = [](std::string_view markerLine) {
		std::string code = "namespace aion::gameserver::handlers::ai {\nclass TestAI {};\n" + std::string(markerLine) + "\n}\n";
		return scanHandler(AI_FILE, code);
	};
	EXPECT_ERROR(scan("AION_AI(TestAI,\n\"x\");").errors, 3, "on one line");
	EXPECT_ERROR(scan("AION_AI(TestAI, NAME);").errors, 3, "plain string literal");
	EXPECT_ERROR(scan("AION_AI(TestAI, u8\"x\");").errors, 3, "plain string literal");
	EXPECT_ERROR(scan("AION_AI(TestAI, \"a\" \"b\");").errors, 3, "malformed marker");
	EXPECT_ERROR(scan("AION_AI(TestAI, \"\");").errors, 3, "must not be empty");
	EXPECT_ERROR(scan("AION_AI(TestAI, \"a\\\"b\");").errors, 3, "without escapes");
	EXPECT_ERROR(scan("AION_AI(TestAI);").errors, 3, "wrong number of arguments");
	EXPECT_ERROR(scan("AION_AI(TestAI, \"x\")").errors, 3, "ending with ';'");
	EXPECT_ERROR(scan("AION_AI(TestAI, \"x\"); int y;").errors, 3, "nothing but a comment");
	EXPECT_ERROR(scan("int y; AION_AI(TestAI, \"x\");").errors, 3, "must start its own line");
	EXPECT_ERROR(scan("AION_AI(ns::TestAI, \"x\");").errors, 3, "malformed marker");
	EXPECT_ERROR(scan("AION_AI(class, \"x\");").errors, 3, "class name");
	EXPECT_ERROR(scan("AION_AI(TestAI, 1 + 2);").errors, 3, "malformed marker");
	EXPECT_ERROR(scan("AION_DETAIL_COMMAND(TestAI, Base);").errors, 3, "is internal");

	auto number = [](std::string_view markerLine) {
		std::string code = "namespace aion::gameserver::handlers::quest {\nclass Q {};\n" + std::string(markerLine) + "\n}\n";
		return scanHandler("aion/gameserver/handlers/quest/Q.cpp", code);
	};
	EXPECT_NO_ERRORS(number("AION_QUEST_HANDLER(Q, 2147483647);").errors);
	EXPECT_ERROR(number("AION_QUEST_HANDLER(Q, 2147483648);").errors, 3, "decimal int literal");
	EXPECT_ERROR(number("AION_QUEST_HANDLER(Q, 0x10);").errors, 3, "decimal int literal");
	EXPECT_ERROR(number("AION_QUEST_HANDLER(Q, 015);").errors, 3, "decimal int literal");
	EXPECT_ERROR(number("AION_QUEST_HANDLER(Q, 1'500);").errors, 3, "decimal int literal");
	EXPECT_ERROR(number("AION_QUEST_HANDLER(Q, 15u);").errors, 3, "decimal int literal");
	EXPECT_ERROR(number("AION_QUEST_HANDLER(Q, -15);").errors, 3, "literals");
	EXPECT_ERROR(number("AION_QUEST_HANDLER(Q, 0);").errors, 3, "greater than 0");
	EXPECT_ERROR(number("AION_QUEST_HANDLER(Q, \"1500\");").errors, 3, "decimal int literal");

	auto zone = [](std::string_view markerLine) {
		std::string code = "namespace aion::gameserver::handlers::zone {\nclass Z {};\n" + std::string(markerLine) + "\n}\n";
		return scanHandler("aion/gameserver/handlers/zone/Z.cpp", code);
	};
	EXPECT_ERROR(zone("AION_ZONE_HANDLER(Z, \"A  B\");").errors, 3, "single spaces");
	EXPECT_ERROR(zone("AION_ZONE_HANDLER(Z, \" A\");").errors, 3, "single spaces");
	EXPECT_ERROR(zone("AION_ZONE_HANDLER(Z, \"A\", 1, 2);").errors, 3, "wrong number of arguments");
	EXPECT_NO_ERRORS(zone("AION_ZONE_HANDLER(Z, \"A\", 0);").errors);
}

TEST(SourceScannerTest, MarkerPlacementViolations) {
	// wrong namespace (also a unity rule violation of the namespace block)
	FileScan wrongNamespace = scanHandler(AI_FILE, "namespace aion::gameserver::handlers::instance {\nclass TestAI {};\nAION_AI(TestAI, \"x\");\n}\n");
	EXPECT_ERROR(wrongNamespace.errors, 3, "must be in namespace aion::gameserver::handlers::ai");
	EXPECT_ERROR(wrongNamespace.errors, 1, "does not match the file's directory");
	EXPECT_TRUE(wrongNamespace.markers.empty());

	FileScan global = scanHandler(AI_FILE, "class TestAI {};\nAION_AI(TestAI, \"x\");\n");
	EXPECT_ERROR(global.errors, 2, "found the global namespace");

	FileScan inClass = scanHandler(AI_FILE, "namespace aion::gameserver::handlers::ai {\nclass TestAI {\n\tAION_AI(TestAI, \"x\");\n};\n}\n");
	EXPECT_ERROR(inClass.errors, 3, "must be at namespace scope");

	FileScan inFunction = scanHandler(AI_FILE, "namespace aion::gameserver::handlers::ai {\nvoid f() {\nAION_AI(TestAI, \"x\");\n}\n}\n");
	EXPECT_ERROR(inFunction.errors, 3, "must be at namespace scope");

	FileScan inIf = scanHandler(AI_FILE, "namespace aion::gameserver::handlers::ai {\nclass TestAI {};\n#ifdef PORTED\nAION_AI(TestAI, \"x\");\n#endif\n}\n");
	EXPECT_ERROR(inIf.errors, 4, "inside an #if block");

	FileScan inHeader = scanHandler("aion/gameserver/handlers/ai/TestAI.h", "#pragma once\nnamespace aion::gameserver::handlers::ai {\nclass TestAI {};\nAION_AI(TestAI, \"x\");\n}\n");
	EXPECT_ERROR(inHeader.errors, 4, "not in a header");

	FileScan wrongCategory = scanHandler("aion/gameserver/handlers/quest/TestAI.cpp", "namespace aion::gameserver::handlers::quest {\nclass TestAI {};\nAION_AI(TestAI, \"x\");\n}\n");
	EXPECT_ERROR(wrongCategory.errors, 3, "belongs in aion/gameserver/handlers/ai/");

	FileScan packetInHandlers = scanHandler(AI_FILE, "namespace aion::gameserver::handlers::ai {\nclass CM_X {};\nAION_CLIENT_PACKET(CM_X);\n}\n");
	EXPECT_ERROR(packetInHandlers.errors, 3, "only allowed in aion/gameserver/network/aion/clientpackets");

	FileScan handlerInPackets = scanClientPacket("aion/gameserver/network/aion/clientpackets/X.cpp",
		"namespace aion::gameserver::network::aion::clientpackets {\nclass X {};\nAION_AI(X, \"x\");\n}\n");
	EXPECT_ERROR(handlerInPackets.errors, 3, "only allowed in handler files");

	FileScan packetWrongNamespace = scanClientPacket("aion/gameserver/network/aion/clientpackets/CM_X.cpp", "namespace { \nAION_CLIENT_PACKET(CM_X);\n}\n");
	EXPECT_ERROR(packetWrongNamespace.errors, 2, "must be in namespace aion::gameserver::network::aion::clientpackets");

	FileScan packetSubdirectory = scanClientPacket("aion/gameserver/network/aion/clientpackets/sub/CM_X.cpp",
		"namespace aion::gameserver::network::aion::clientpackets::sub {\nclass CM_X {};\nAION_CLIENT_PACKET(CM_X);\n}\n");
	EXPECT_ERROR(packetSubdirectory.errors, 3, "belongs directly in aion/gameserver/network/aion/clientpackets");

	FileScan inDefine = scanHandler(AI_FILE, "#define REGISTER(C) AION_AI(C, \"c\")\nnamespace aion::gameserver::handlers::ai {\n}\n");
	EXPECT_ERROR(inDefine.errors, 1, "must not appear in preprocessor directives");
}

TEST(SourceScannerTest, UnityFileRules) {
	FileScan anonymous = scanHandler(AI_FILE, "namespace aion::gameserver::handlers::ai {\nnamespace {\nint helper() { return 1; }\n}\n}\n");
	EXPECT_ERROR(anonymous.errors, 2, "anonymous namespaces are not allowed");

	FileScan staticFunction = scanHandler(AI_FILE, "namespace aion::gameserver::handlers::ai {\nstatic int helper() { return 1; }\n}\n");
	EXPECT_ERROR(staticFunction.errors, 2, "namespace-scope 'static'");

	FileScan staticMember = scanHandler(AI_FILE, "namespace aion::gameserver::handlers::ai {\nclass A {\n\tstatic int helper() { return 1; }\n\tstatic inline int x = 0;\n};\nstatic_assert(true);\n}\n");
	EXPECT_NO_ERRORS(staticMember.errors);

	FileScan usingNamespace = scanHandler(AI_FILE, "namespace aion::gameserver::handlers::ai {\nvoid f() {\n\tusing namespace std;\n}\n}\n");
	EXPECT_ERROR(usingNamespace.errors, 3, "'using namespace' is not allowed");

	FileScan usingDeclaration = scanHandler(AI_FILE, "namespace aion::gameserver::handlers::ai {\nusing gameserver::ai::NpcAI;\nusing Alias = int;\n}\n");
	EXPECT_NO_ERRORS(usingDeclaration.errors);

	FileScan globalCode = scanHandler(AI_FILE, "#include <string>\nint counter = 0;\nnamespace aion::gameserver::handlers::ai {\n}\n");
	EXPECT_ERROR(globalCode.errors, 2, "handler code must be inside namespace aion::gameserver::handlers::ai");

	FileScan otherNamespace = scanHandler(AI_FILE, "namespace aion::gameserver::model {\nclass Npc;\n}\n");
	EXPECT_ERROR(otherNamespace.errors, 1, "does not match the file's directory");

	FileScan nested = scanHandler(AI_FILE, "namespace aion::gameserver::handlers::ai {\nnamespace detail {\nint x;\n}\n}\n");
	EXPECT_ERROR(nested.errors, 2, "does not match the file's directory");

	FileScan partialPrefix = scanHandler(AI_FILE, "namespace aion::gameserver {\nclass Leak {};\nnamespace handlers::ai {\n}\n}\n");
	EXPECT_ERROR(partialPrefix.errors, 2, "found it in namespace aion::gameserver");

	FileScan unbalanced = scanHandler(AI_FILE, "namespace aion::gameserver::handlers::ai {\nvoid f() {\n}\n");
	EXPECT_ERROR(unbalanced.errors, 0, "unbalanced '{'");

	FileScan unbalancedClose = scanHandler(AI_FILE, "namespace aion::gameserver::handlers::ai {\n}\n}\n");
	EXPECT_ERROR(unbalancedClose.errors, 3, "unbalanced '}'");

	FileScan unterminatedIf = scanHandler(AI_FILE, "#if 1\nnamespace aion::gameserver::handlers::ai {\n}\n");
	EXPECT_ERROR(unterminatedIf.errors, 0, "unterminated #if");

	// the rules do not apply to core sources
	FileScan core = scanClientPacket("aion/gameserver/network/aion/clientpackets/CM_X.cpp",
		"using namespace std;\nstatic int x;\nnamespace aion::gameserver::network::aion::clientpackets {\nnamespace { int y; }\n}\n");
	EXPECT_NO_ERRORS(core.errors);
}

TEST(SourceScannerTest, TypeDefinitions) {
	FileScan scan = scanHandler(AI_FILE, R"cpp(
namespace aion::gameserver::handlers::ai {
class Forward;
struct Plain {};
class Derived final : public Base<int, std::map<int, int>> {
	struct Nested {};
};
enum class Kind : int32_t { A, B };
enum Opaque : int;
template <class T> class Templ { };
template <> class Templ<int> { };
struct Plain* pointerToPlain();
union U { int a; };
class [[deprecated]] Old {};
}
)cpp");
	EXPECT_NO_ERRORS(scan.errors);
	std::vector<std::string> names;
	for (const TypeDefinition& type : scan.types)
		names.push_back(type.name);
	EXPECT_EQ(names, (std::vector<std::string>{"Plain", "Derived", "Kind", "Templ", "U", "Old"}));
}
