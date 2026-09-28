#include "RegscanTestSupport.h"

#include "JavaScanner.h"

using namespace aion::gameserver::tools::regscan;

namespace {

struct JavaFile {
	std::string_view relPath;
	std::string_view code;
};

/** scans several files and resolves the registrations; keeps only the registered classes, like the Java class listeners */
JavaHandlerScan scanFiles(std::initializer_list<JavaFile> files) {
	JavaHandlerScan scan;
	for (const JavaFile& file : files)
		scanJavaHandlerFile(file.relPath, file.relPath, file.code, scan);
	resolveJavaHandlerRegistrations(scan);
	std::erase_if(scan.classes, [](const JavaHandlerClass& cls) { return !cls.registered; });
	return scan;
}

JavaHandlerScan scanOne(std::string_view relPath, std::string_view code) {
	return scanFiles({{relPath, code}});
}

const JavaHandlerClass* findClass(const JavaHandlerScan& scan, std::string_view javaClass) {
	auto it = std::ranges::find(scan.classes, javaClass, &JavaHandlerClass::javaClass);
	return it == scan.classes.end() ? nullptr : &*it;
}

} // namespace

TEST(JavaScannerTest, AINameOnPublicNonAbstractClassesOnly) {
	JavaHandlerScan scan = scanOne("ai/instance/darkPoeta/CalindiFlamelordAI.java", R"java(
package ai.instance.darkPoeta;

import ai.AggressiveNpcAI;

/**
 * @AIName("in_javadoc")
 */
@SuppressWarnings("unused")
@AIName("calindi_flamelord")
public class CalindiFlamelordAI<T extends Npc> extends AggressiveNpcAI {

	@Override
	protected void handleSpawned() {
		spawn(281267, 1f, 2f, 3f, (byte) 0); // sp(281268 in a comment)
		new Runnable() { public void run() {} };
	}
}

@AIName("package_private")
class Helper extends NpcAI {
}
)java");
	EXPECT_NO_ERRORS(scan.errors);
	ASSERT_EQ(scan.classes.size(), 1u);
	EXPECT_EQ(scan.classes[0].javaClass, "ai.instance.darkPoeta.CalindiFlamelordAI");
	EXPECT_EQ(scan.classes[0].aiName, "calindi_flamelord");
	EXPECT_EQ(scan.classes[0].line, 11u);
	EXPECT_EQ(scan.npcIds, (std::set<int32_t>{281267, 281268}));

	JavaHandlerScan abstractScan = scanOne("ai/AbstractShieldAI.java", "package ai;\n@AIName(\"shield\")\npublic abstract class AbstractShieldAI extends NpcAI {}\n");
	EXPECT_NO_ERRORS(abstractScan.errors);
	EXPECT_TRUE(abstractScan.classes.empty());
}

TEST(JavaScannerTest, InstanceAndZoneAnnotations) {
	JavaHandlerScan instance = scanOne("instance/DredgionInstance.java", "package instance;\n@InstanceID(300110000)\npublic class DredgionInstance extends GeneralInstanceHandler {}\n");
	EXPECT_NO_ERRORS(instance.errors);
	ASSERT_EQ(instance.classes.size(), 1u);
	EXPECT_EQ(instance.classes[0].instanceId, 300110000);

	JavaHandlerScan zone = scanOne("zone/_1012SensoryArea.java",
		"package zone;\n@ZoneNameAnnotation(\n\tvalue = \"A B C\",\n\tquestId = 1012)\npublic class _1012SensoryArea extends QuestZoneHandler {}\n");
	EXPECT_NO_ERRORS(zone.errors);
	ASSERT_EQ(zone.classes.size(), 1u);
	ASSERT_TRUE(zone.classes[0].zone.has_value());
	EXPECT_EQ(zone.classes[0].zone->names, "A B C");
	EXPECT_EQ(zone.classes[0].zone->questId, 1012);

	JavaHandlerScan zoneDefault = scanOne("zone/pvpZones/PvPAreaZone.java", "package zone.pvpZones;\n@ZoneNameAnnotation(\"X Y\")\npublic class PvPAreaZone extends PvPZone {}\n");
	ASSERT_EQ(zoneDefault.classes.size(), 1u);
	EXPECT_EQ(zoneDefault.classes[0].zone->names, "X Y");
	EXPECT_EQ(zoneDefault.classes[0].zone->questId, 0);

	JavaHandlerScan bad = scanOne("instance/Bad.java", "package instance;\n@InstanceID(BASE + 1)\npublic class Bad extends GeneralInstanceHandler {}\n");
	EXPECT_ERROR(bad.errors, 2, "unsupported @InstanceID arguments");
}

TEST(JavaScannerTest, QuestIdsFromSuperCalls) {
	JavaHandlerScan literal = scanOne("quest/heiron/_1500OrdersFromPerento.java", R"java(
package quest.heiron;
public class _1500OrdersFromPerento extends AbstractQuestHandler {
	public _1500OrdersFromPerento() {
		super(1500);
	}
	@Override
	public boolean onDialogEvent(QuestEnv env) {
		return super.onDialogEvent(env);
	}
}
)java");
	EXPECT_NO_ERRORS(literal.errors);
	ASSERT_EQ(literal.classes.size(), 1u);
	EXPECT_EQ(literal.classes[0].questId, 1500);

	JavaHandlerScan constant = scanOne("quest/reshanta/_1719X.java", R"java(
package quest.reshanta;
public class _1719X extends AbstractQuestHandler {
	private final static int _questId = 1719;
	private final static int[] mobs = { 1, 2 };
	public _1719X() {
		super(_questId);
	}
}
)java");
	EXPECT_NO_ERRORS(constant.errors);
	ASSERT_EQ(constant.classes.size(), 1u);
	EXPECT_EQ(constant.classes[0].questId, 1719);

	JavaHandlerScan unknown = scanOne("quest/X.java", "package quest;\npublic class X extends AbstractQuestHandler {\n\tpublic X() {\n\t\tsuper(compute());\n\t}\n}\n");
	EXPECT_ERROR(unknown.errors, 2, "cannot determine the quest id of X");

	JavaHandlerScan notAHandler = scanOne("quest/Helper.java", "package quest;\npublic class Helper {\n}\n");
	EXPECT_NO_ERRORS(notAHandler.errors);
	EXPECT_TRUE(notAHandler.classes.empty());
}

TEST(JavaScannerTest, Commands) {
	JavaHandlerScan scan = scanOne("admincommands/Add.java", R"java(
package admincommands;
public class Add extends AdminCommand {
	public Add() { super("add", "Adds an item."); }
	private static class Nested extends AdminCommand {}
}
)java");
	EXPECT_NO_ERRORS(scan.errors);
	ASSERT_EQ(scan.classes.size(), 1u);
	EXPECT_EQ(scan.classes[0].javaClass, "admincommands.Add");
	EXPECT_EQ(scan.classes[0].command, CommandKind::ADMIN);

	EXPECT_EQ(scanOne("playercommands/Id.java", "package playercommands;\npublic class Id extends PlayerCommand {}").classes.at(0).command, CommandKind::PLAYER);
	EXPECT_EQ(scanOne("consolecommands/A.java", "package consolecommands;\npublic final class A extends ConsoleCommand {}").classes.at(0).command,
		CommandKind::CONSOLE);
	EXPECT_TRUE(scanOne("admincommands/S.java", "package admincommands;\npublic class S implements StatFunction {}").classes.empty());

	JavaHandlerScan all;
	scanJavaHandlerFile("admincommands/S.java", "admincommands/S.java", "package admincommands;\npublic class S implements StatFunction {}\nclass T {}\n", all);
	ASSERT_EQ(all.classes.size(), 2u); // unregistered classes are kept for the cross-checks
	EXPECT_FALSE(all.classes[0].registered);
	EXPECT_EQ(all.classes[1].javaClass, "admincommands.T");
}

TEST(JavaScannerTest, QuestHandlersAndCommandsRegisterThroughTheWholeSuperclassChain) {
	// Java: AbstractQuestHandler.class.isAssignableFrom(c) / ChatCommand.class.isAssignableFrom(c), so indirect subclasses register too
	JavaHandlerScan scan = scanFiles({
		{"quest/shared/SharedQuestBase.java", R"java(
package quest.shared;
import com.aionemu.gameserver.questEngine.handlers.AbstractQuestHandler;
public abstract class SharedQuestBase extends AbstractQuestHandler {
	protected SharedQuestBase(int questId) { super(questId); }
}
)java"},
		{"quest/heiron/_1234X.java", R"java(
package quest.heiron;
import quest.shared.SharedQuestBase;
public class _1234X extends SharedQuestBase {
	public _1234X() {
		super(1234);
	}
}
)java"},
		{"quest/heiron/_1235Y.java", R"java(
package quest.heiron;
public class _1235Y extends _1234X { // same package, two levels down
	public _1235Y() { super(1235); }
}
)java"},
		{"quest/heiron/_1236Z.java", R"java(
package quest.heiron;
import com.aionemu.gameserver.questEngine.handlers.template.*;
public class _1236Z extends MentorMonsterHunt { // core template chain: MentorMonsterHunt -> MonsterHunt -> AbstractTemplateQuestHandler
	private static final int QUEST = 1236;
	public _1236Z() { super(QUEST); }
}
)java"},
		{"admincommands/base/ToggleCommand.java", R"java(
package admincommands.base;
import com.aionemu.gameserver.utils.chathandlers.AdminCommand;
public abstract class ToggleCommand extends AdminCommand {
}
)java"},
		{"admincommands/Gm.java", R"java(
package admincommands;
import admincommands.base.*;
public class Gm extends ToggleCommand {
	private static final Logger log = LoggerFactory.getLogger(Gm.class); // not a class declaration
	public Gm() { super("gm"); }
	public static class GmOn extends admincommands.base.ToggleCommand {}  // public static nested classes register (qualified name)
	public class Inner extends ToggleCommand {}                            // not static: no no-argument constructor for the loader
	static class Hidden extends ToggleCommand {}                           // not public
	void run() { new ToggleCommand() { }; class Local extends ToggleCommand {} }
}
)java"},
		{"playercommands/Outer.java", R"java(
package playercommands;
public class Outer {
	public abstract static class Base extends com.aionemu.gameserver.utils.chathandlers.PlayerCommand {}
	public static class Cmd extends Base {} // member type of the enclosing class
}
)java"},
	});
	EXPECT_NO_ERRORS(scan.errors);
	std::vector<std::string> registered;
	for (const JavaHandlerClass& cls : scan.classes)
		registered.push_back(cls.javaClass);
	EXPECT_EQ(registered, (std::vector<std::string>{"quest.heiron._1234X", "quest.heiron._1235Y", "quest.heiron._1236Z", "admincommands.Gm",
		"admincommands.Gm.GmOn", "playercommands.Outer.Cmd"}));
	EXPECT_EQ(findClass(scan, "quest.heiron._1234X")->questId, 1234);
	EXPECT_EQ(findClass(scan, "quest.heiron._1235Y")->questId, 1235);
	EXPECT_EQ(findClass(scan, "quest.heiron._1236Z")->questId, 1236);
	EXPECT_EQ(findClass(scan, "admincommands.Gm")->command, CommandKind::ADMIN);
	EXPECT_EQ(findClass(scan, "admincommands.Gm.GmOn")->command, CommandKind::ADMIN);
	ASSERT_NE(findClass(scan, "playercommands.Outer.Cmd"), nullptr);
	EXPECT_EQ(findClass(scan, "playercommands.Outer.Cmd")->command, CommandKind::PLAYER);
	EXPECT_EQ(findClass(scan, "admincommands.Gm")->line, 4u);
}

TEST(JavaScannerTest, UnresolvableHandlerChainsAreErrors) {
	JavaHandlerScan unknownCore = scanOne("quest/X.java", R"java(
package quest;
import com.aionemu.gameserver.questEngine.handlers.template.NewTemplate;
public class X extends NewTemplate {
	public X() { super(1); }
}
)java");
	EXPECT_ERROR(unknownCore.errors, 4, "cannot resolve the superclass chain of quest.X: com.aionemu.gameserver.questEngine.handlers.template.NewTemplate");

	JavaHandlerScan wildcard = scanOne("admincommands/Y.java",
		"package admincommands;\nimport com.aionemu.gameserver.utils.chathandlers.*;\npublic class Y extends FancyCommand {}\n");
	EXPECT_ERROR(wildcard.errors, 3, "com.aionemu.gameserver.utils.chathandlers.FancyCommand is not a core handler base class");

	JavaHandlerScan chat = scanOne("admincommands/Z.java", "package admincommands;\npublic class Z extends ChatCommand {}\n");
	EXPECT_ERROR(chat.errors, 2, "extends ChatCommand without AdminCommand");

	JavaHandlerScan cycle = scanFiles({{"quest/A.java", "package quest;\npublic class A extends B {}\n"}, {"quest/B.java", "package quest;\npublic class B extends A {}\n"}});
	EXPECT_ERROR(cycle.errors, 2, "cyclic superclass chain of quest.A");

	JavaHandlerScan unknownId = scanFiles({{"quest/Base.java", "package quest;\npublic abstract class Base extends AbstractQuestHandler {}\n"},
		{"quest/_7C.java", "package quest;\npublic class _7C extends Base {\n\tpublic _7C() { super(7, 8); }\n}\n"}});
	EXPECT_ERROR(unknownId.errors, 2, "cannot determine the quest id of _7C");

	JavaHandlerScan external = scanOne("admincommands/Util.java",
		"package admincommands;\nimport com.aionemu.gameserver.model.gameobjects.HouseObject;\npublic class Util extends HouseObject {}\npublic class T2 extends Thread {}\n");
	EXPECT_NO_ERRORS(external.errors);
	EXPECT_TRUE(external.classes.empty()) << "classes outside the handler hierarchies are not handlers";

	JavaHandlerScan notQuestDir = scanOne("admincommands/Q.java", "package admincommands;\npublic class Q extends AbstractQuestHandler {}\n");
	EXPECT_NO_ERRORS(notQuestDir.errors);
	EXPECT_TRUE(notQuestDir.classes.empty()) << "QuestHandlerLoader only loads data/handlers/quest";

	JavaHandlerScan twice;
	resolveJavaHandlerRegistrations(twice);
	EXPECT_THROW(resolveJavaHandlerRegistrations(twice), std::logic_error);
	EXPECT_THROW(scanJavaHandlerFile("quest/A.java", "quest/A.java", "", twice), std::logic_error);
}

TEST(JavaScannerTest, NpcIdsOnlyForAiInstanceAndQuest) {
	EXPECT_EQ(scanOne("zone/Z.java", "spawn(215074);").npcIds.size(), 0u);
	EXPECT_EQ(scanOne("admincommands/Z.java", "spawn(215074);").npcIds.size(), 0u);
	EXPECT_EQ(scanOne("instance/Z.java", "spawn(215074);").npcIds.size(), 1u);
	EXPECT_EQ(scanOne("quest/a/Z.java", "sp(215074);").npcIds.size(), 1u);
}

TEST(JavaScannerTest, ClientPacketFactory) {
	std::vector<Diagnostic> errors;
	std::vector<JavaClientPacket> packets = scanJavaClientPacketFactory("Factory.java", R"java(
public class AionClientPacketFactory {
	private static final PacketInfo<? extends AionClientPacket>[] packets = new PacketInfo<?>[250];
	static {
		packets[0] = new PacketInfo<>(CM_VERSION_CHECK.class, State.CONNECTED); // [C_VERSION]
		// packets[1] = [C_QUERY_PASSPORT (QueryPassportPacket)]
		/* packets[2] = new PacketInfo<>(CM_BLOCKED.class, State.IN_GAME); */
		packets[244] = new PacketInfo<>(CM_BIND_POINT_TELEPORT.class, State.IN_GAME);// [C_HOTSPOT]
	}
}
)java", errors);
	EXPECT_NO_ERRORS(errors);
	ASSERT_EQ(packets.size(), 2u);
	EXPECT_EQ(packets[0].name, "CM_VERSION_CHECK");
	EXPECT_EQ(packets[0].opcode, 0);
	EXPECT_EQ(packets[1].name, "CM_BIND_POINT_TELEPORT");
	EXPECT_EQ(packets[1].opcode, 244);

	scanJavaClientPacketFactory("Factory.java", "packets[1] = createInfo(CM_X.class);\n", errors);
	EXPECT_ERROR(errors, 1, "unexpected packet table entry");
}
