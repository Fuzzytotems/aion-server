// M5c C-04 (m5c-plan.md §5, §20.3; P5-15): CM_CRAFT (C_COMBINE), what the client sends when the player starts a craft at a crafting station or
// morphs substances. Java: CM_CRAFT.java:32-61.
//
// The read cases lay each body out from the Java readImpl. The run cases drive runImpl on CraftPacketTestSupport.h's fixture into the craft
// lane's CraftService.startCrafting (C-01) and the craft-task lane's CraftingTask (C-02). runImpl's own checks decide whether startCrafting runs
// at all: an active player who is spawned, no shutdown within 30 s, and for any first byte but 129 (the morph) a known object closer than 10 m
// centre to centre whose template id is the one the client sent. What startCrafting then sends shows which recipe, target, craft type and
// materials it got: the craft's start packets (CraftingTask.onInteractionStart) name the recipe's skill and product and the station, the
// consumed items the materials and the enhancement stone, and a refusal's cancel pair (CraftService.sendCancelCraft) the target object id.
// The distances are the oracle's (`oracle.py m5c-craft --no-profile --set gameserver.event.service.disabled_events=* --recipe 155001381`,
// craft.station: the packet's 10 m centre to centre, checkCraft's 5.25 m, both strict), and the gate's C19 spots are 3, 7 and 12 m.

#include "CraftPacketTestSupport.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "aion/commons/utils/ExitCode.h"
#include "aion/gameserver/GameServer.h"
#include "aion/gameserver/ShutdownHook.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/PunishmentConfig.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/network/aion/clientpackets/CM_CRAFT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

std::unique_ptr<AionClientPacket> CM_CRAFT_clientPacketFactory(int32_t opcode, const StateSet& validStates);

/** The friend CM_CRAFT.h declares: the fields readImpl decoded, which Java keeps private */
struct CM_CRAFTTestAccess {
	static int32_t unk(const CM_CRAFT& p) { return p.unk; }
	static int32_t targetTemplateId(const CM_CRAFT& p) { return p.targetTemplateId; }
	static int32_t recipeId(const CM_CRAFT& p) { return p.recipeId; }
	static int32_t targetObjId(const CM_CRAFT& p) { return p.targetObjId; }
	static int32_t craftType(const CM_CRAFT& p) { return p.craftType; }
	static const std::unordered_map<int32_t, int64_t>& materialsData(const CM_CRAFT& p) { return p.materialsData; }
};

namespace testing::items {
namespace {

using Access = CM_CRAFTTestAccess;
using network::test::LogCapture;
using serverpackets::SM_SYSTEM_MESSAGE;

void noExit(int32_t) {
}

/**
 * GameServer.isShuttingDownSoon() for the scope: a shutdown scheduled with 30 s left (GameServer.initShutdown -> ShutdownHook.initShutdown), its
 * hook thread held in the first worldHasPlayers until the scope ends, every other operation and the exit a no-op (the pattern of
 * tests/itemsvc/ItemMoveSplitServiceTest.cpp and tests/app/GameServerTest.cpp). The destructor releases the thread, waits for it and resets
 * the hook.
 */
class ShutdownSoonScope {
public:
	ShutdownSoonScope() {
		ShutdownHook::setExitFunctionForTests(&noExit);
		ShutdownHook::Operations ops;
		ops.worldHasPlayers = [this] {
			while (!release.load())
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			return false;
		};
		ops.announceShutdown = [](int32_t) {};
		ops.shutdownNetwork = [] {};
		ops.dumpStats = [] {};
		ops.saveData = [] {};
		ops.shutdownRuntime = [] {};
		ops.sleep = [](std::chrono::milliseconds) {};
		ShutdownHook::setOperationsForTests(ops);
		GameServer::initShutdown(commons::utils::ExitCode::NORMAL, 30);
	}

	~ShutdownSoonScope() {
		release.store(true);
		static_cast<void>(ShutdownHook::getInstance().awaitCompletion(std::chrono::seconds(10)));
		ShutdownHook::resetForTests();
	}

	ShutdownSoonScope(const ShutdownSoonScope&) = delete;
	ShutdownSoonScope& operator=(const ShutdownSoonScope&) = delete;

private:
	std::atomic<bool> release{false};
};

// ------------------------------------------------------------------------------------------------------------------------------------ readImpl

// CM_CRAFT.java:32-41: readUC unk, readD targetTemplateId, readD recipeId, readD targetObjId, readUH materialsCount, readUC craftType, then per
// material readD itemId and readQ count, all little-endian
TEST(CraftReadTest, TheFieldsAreReadInJavasOrder) {
	const std::vector<uint8_t> body{0x00, 0x04, 0x03, 0x02, 0x01, 0x08, 0x07, 0x06, 0x05, 0x0C, 0x0B, 0x0A, 0x09, 0x01, 0x00, 0x02,
		0x14, 0x13, 0x12, 0x11, 0x28, 0x27, 0x26, 0x25, 0x24, 0x23, 0x22, 0x21};
	int32_t unread = -1;
	auto p = readAlone<CM_CRAFT>(CM_CRAFT_OPCODE, body, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(Access::unk(*p), 0);
	EXPECT_EQ(Access::targetTemplateId(*p), 0x01020304);
	EXPECT_EQ(Access::recipeId(*p), 0x05060708);
	EXPECT_EQ(Access::targetObjId(*p), 0x090A0B0C);
	EXPECT_EQ(Access::craftType(*p), 2);
	EXPECT_EQ(Access::materialsData(*p), (CraftMaterials{{0x11121314, 0x2122232425262728LL}}));
	EXPECT_EQ(unread, 0);
}

// The gate's C19 body: `CM_CRAFT(0, 150000009, 155001381, oven, {152001001: 1, 169400096: 2}, 0)`, and the morph's first byte 129
TEST(CraftReadTest, TheGatesBodyAndTheMorphByte) {
	int32_t unread = -1;
	auto p = readAlone<CM_CRAFT>(CM_CRAFT_OPCODE, craftBody(129, OVEN, ROAST_ININA_RECIPE, 0x0A0B0C0D, {{ININA, 1}, {SALT, 2}}, 1), unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(Access::unk(*p), 129);
	EXPECT_EQ(Access::targetTemplateId(*p), OVEN);
	EXPECT_EQ(Access::recipeId(*p), ROAST_ININA_RECIPE);
	EXPECT_EQ(Access::targetObjId(*p), 0x0A0B0C0D);
	EXPECT_EQ(Access::craftType(*p), 1);
	EXPECT_EQ(Access::materialsData(*p), (CraftMaterials{{ININA, 1}, {SALT, 2}}));
	EXPECT_EQ(unread, 0);
}

// readUC: the first byte and the craft type are unsigned (0xFF is 255, not -1)
TEST(CraftReadTest, TheFirstByteAndTheCraftTypeAreUnsigned) {
	int32_t unread = -1;
	auto p = readAlone<CM_CRAFT>(CM_CRAFT_OPCODE, craftBody(0xFF, OVEN, ROAST_ININA_RECIPE, 1, {}, 0xFF), unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(Access::unk(*p), 255);
	EXPECT_EQ(Access::craftType(*p), 255);
	EXPECT_TRUE(Access::materialsData(*p).empty());
	EXPECT_EQ(unread, 0);
}

// readUH: a count of 0x8000 is 32768 materials, not a negative count that reads none
TEST(CraftReadTest, TheMaterialsCountIsAnUnsignedShort) {
	PacketWriter body;
	body.C(0).D(OVEN).D(ROAST_ININA_RECIPE).D(1).H(0x8000).C(0);
	for (int32_t i = 0; i < 0x8000; i++)
		body.D(i + 1).Q(i);
	int32_t unread = -1;
	auto p = readAlone<CM_CRAFT>(CM_CRAFT_OPCODE, body.data, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(Access::materialsData(*p).size(), 0x8000u);
	EXPECT_EQ(Access::materialsData(*p).at(0x8000), 0x7FFF);
	EXPECT_EQ(unread, 0);
}

// CM_CRAFT.java:40: HashMap.put - a repeated item id keeps the count read last
TEST(CraftReadTest, ARepeatedItemIdKeepsTheCountReadLast) {
	int32_t unread = -1;
	auto p = readAlone<CM_CRAFT>(CM_CRAFT_OPCODE, craftBody(0, OVEN, ROAST_ININA_RECIPE, 1, {{SALT, 1}, {ININA, 5}, {SALT, 7}}, 0), unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(Access::materialsData(*p), (CraftMaterials{{SALT, 7}, {ININA, 5}}));
	EXPECT_EQ(unread, 0);
}

// A body shorter than its count: Java's readD and readQ log the underflow and answer 0 (BaseClientPacket), so the missing material is item 0
// with count 0, and the body still reads
TEST(CraftReadTest, AShortBodyReadsZeroesAsJavaDoes) {
	PacketWriter body;
	body.C(0).D(OVEN).D(ROAST_ININA_RECIPE).D(1).H(2).C(0).D(ININA).Q(1);
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	int32_t unread = -1;
	auto p = readAlone<CM_CRAFT>(CM_CRAFT_OPCODE, body.data, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(Access::materialsData(*p), (CraftMaterials{{ININA, 1}, {0, 0}}));
	EXPECT_TRUE(capture.contains("Missing D")) << capture.dump();
	EXPECT_TRUE(capture.contains("Missing Q")) << capture.dump();
	EXPECT_EQ(unread, 0);
}

TEST(CraftReadTest, TheMarkerRegistersTheClassUnderItsJavaOpcode) {
	EXPECT_NE(dynamic_cast<CM_CRAFT*>(CM_CRAFT_clientPacketFactory(CM_CRAFT_OPCODE, StateSet{AionConnection_State::IN_GAME}).get()), nullptr);
	EXPECT_EQ(economyTableEntries("CM_CRAFT", CM_CRAFT_OPCODE), 1);
}

// ------------------------------------------------------------------------------------------------------------------------------------- runImpl

class CraftPacketRunTest : public CraftPacketTest {
protected:
	void SetUp() override {
		CraftPacketTest::SetUp();
		setRecipes({ROAST_ININA_RECIPE});
		give(830001, ININA, 1);
		give(830002, SALT, 2);
	}

	/** Reads and runs CM_CRAFT on the player's connection */
	void craft(int32_t unk, int32_t targetTemplateId, int32_t recipeId, int32_t targetObjId,
		std::initializer_list<std::pair<int32_t, int64_t>> materials, int32_t craftType) {
		readAndRun<CM_CRAFT>(CM_CRAFT_OPCODE, craftBody(unk, targetTemplateId, recipeId, targetObjId, materials, craftType));
	}

	/** The gate's Roast Inina body at `targetObjId` */
	void roastInina(int32_t targetObjId, int32_t unk = 0) { craft(unk, OVEN, ROAST_ININA_RECIPE, targetObjId, {{ININA, 1}, {SALT, 2}}, 0); }

	/** Nothing was sent, nothing consumed and no craft started: runImpl returned before CraftService */
	void expectNothing(const char* why) {
		EXPECT_TRUE(sent().empty()) << why;
		EXPECT_EQ(countOf(ININA), 1) << why;
		EXPECT_EQ(countOf(SALT), 2) << why;
		EXPECT_FALSE(craftInProgress()) << why;
	}

	/** checkCraft's refusal of a station out of its 5.25 m: STR_COMBINE_TOO_FAR_FROM_TOOL(station) and the cancel pair; nothing consumed */
	void expectTooFar(model::gameobjects::StaticObject& oven, const char* why) {
		std::vector<std::vector<uint8_t>> expected{
			serializedFor(SM_SYSTEM_MESSAGE::STR_COMBINE_TOO_FAR_FROM_TOOL(oven.getObjectTemplate()->getL10n()))};
		for (std::vector<uint8_t>& packet : craftRefused(COOKING, ROAST_ININA, oven.getObjectId()))
			expected.push_back(std::move(packet));
		EXPECT_EQ(sent(), expected) << why;
		EXPECT_EQ(countOf(ININA), 1) << why;
		EXPECT_FALSE(craftInProgress()) << why;
	}

	std::vector<uint8_t> message(SM_SYSTEM_MESSAGE&& packet) { return serializedFor(std::move(packet)); }

	EconomyConfigScope<bool> logAudit{configs::main::LoggingConfig::LOG_AUDIT, true};
	EconomyConfigScope<bool> noPunishment{configs::main::PunishmentConfig::PUNISHMENT_ENABLE, false};
};

// CM_CRAFT.java:60 -> CraftService.startCrafting: the recipe's skill and product, the station as the craft's responder, the materials consumed
// (checkCraft, CraftService.java:222-230), the CraftingTask in progress
TEST_F(CraftPacketRunTest, AStationWithinReachStartsTheCraft) {
	model::gameobjects::StaticObject& oven = station(3.0f);

	roastInina(oven.getObjectId());

	EXPECT_EQ(craftPackets(), craftStart(COOKING, ROAST_ININA, oven.getObjectId()));
	EXPECT_EQ(countOf(ININA), 0);
	EXPECT_EQ(countOf(SALT), 0);
	EXPECT_TRUE(craftInProgress());
}

// The gate's C19 spots (m5c-plan.md §10.2): from 7 m the packet passes (7 < 10) and checkCraft refuses (7 >= 5.25); from 12 m the packet
// itself returns
TEST_F(CraftPacketRunTest, FromSevenMetresCheckCraftRefusesAndFromTwelveThePacketReturns) {
	model::gameobjects::StaticObject& near = station(7.0f);
	roastInina(near.getObjectId());
	expectTooFar(near, "7 m");

	clearSent();
	model::gameobjects::StaticObject& far = station(12.0f);
	roastInina(far.getObjectId());
	expectNothing("12 m");
}

// CM_CRAFT.java:55: PositionUtil.isInRange(player, staticObject, 10) - centre to centre (no bound radius), in three dimensions, strictly below;
// 9.999 m east (float: 109.999 - 100, squared 99.98) passes the packet, which pins the range to 10 within a millimetre
TEST_F(CraftPacketRunTest, TenMetresCentreToCentreIsOutOfReach) {
	model::gameobjects::StaticObject& east = station(10.0f);
	roastInina(east.getObjectId());
	expectNothing("10 m east: 100 < 100 is false");

	model::gameobjects::StaticObject& above = station(6.0f, 8.0f);
	roastInina(above.getObjectId());
	expectNothing("6 m east and 8 m up: 10 m in three dimensions");

	model::gameobjects::StaticObject& justBelow = station(6.0f, 7.9f);
	roastInina(justBelow.getObjectId());
	expectTooFar(justBelow, "6 m east and 7.9 m up: 9.92 m passes the packet");

	model::gameobjects::StaticObject& edge = station(9.999f);
	roastInina(edge.getObjectId());
	expectTooFar(edge, "9.999 m east passes the packet");
}

// CM_CRAFT.java:56: the object's template id must be the one the client sent
TEST_F(CraftPacketRunTest, TheTemplateIdMustBeTheTargets) {
	model::gameobjects::StaticObject& oven = station(3.0f);

	craft(0, OVEN + 1, ROAST_ININA_RECIPE, oven.getObjectId(), {{ININA, 1}, {SALT, 2}}, 0);

	expectNothing("template 150000010 sent for an oven");
}

// CM_CRAFT.java:54-55: getKnownList().getObject(targetObjId) - an object the player does not know is no target
TEST_F(CraftPacketRunTest, AnObjectThePlayerDoesNotKnowIsNoTarget) {
	model::gameobjects::StaticObject& oven = station(3.0f);

	roastInina(oven.getObjectId() + 1);

	expectNothing("an unknown object id");
}

// CM_CRAFT.java:53: only the first byte 129 skips the target check. With an object id the player does not know, every other byte returns in
// the packet; 129 reaches startCrafting, whose checkCraft refuses a cooking recipe without a StaticObject target (an audit line and the cancel
// pair, CraftService.java:149-153) - the cancel animation names the object id the client sent
TEST_F(CraftPacketRunTest, OnlyTheMorphByteSkipsTheTargetCheck) {
	LogCapture audit({AUDIT_LOGGER});
	for (int32_t unk : {0, 1, 128, 130, 255}) {
		roastInina(777, unk);
		expectNothing("an unknown target and a first byte other than 129");
	}
	EXPECT_EQ(audit.dump(), "");

	roastInina(777, 129);

	EXPECT_EQ(sent(), craftRefused(COOKING, ROAST_ININA, 777));
	EXPECT_TRUE(audit.contains(player().toString() + " tried to craft with incorrect target")) << audit.dump();
	EXPECT_EQ(countOf(ININA), 1);
}

// The morph as the client sends it (first byte 129, no station): CraftService starts it without a target, so the task's responder is the
// player (AbstractInteractionTask.java:27-33); the recipe's 200 DP are spent and the powder consumed. The same body with the first byte 0
// returns in the packet. A starting class has no DP (PlayerCommonData.setDp), so the morpher is a Gladiator.
TEST_F(CraftPacketRunTest, AMorphStartsWithoutAStation) {
	player().getCommonData()->setPlayerClass(model::PlayerClass::GLADIATOR);
	player().getCommonData()->setDp(200);
	ASSERT_EQ(player().getCommonData()->getDp(), 200);
	setRecipes({ININA_MORPH_RECIPE});
	setSkills({{MORPH, 1}});
	give(830003, AETHER_POWDER, 1);
	clearSent();

	craft(0, 0, ININA_MORPH_RECIPE, 0, {{AETHER_POWDER, 1}}, 0);
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(countOf(AETHER_POWDER), 1);

	craft(129, 0, ININA_MORPH_RECIPE, 0, {{AETHER_POWDER, 1}}, 0);

	EXPECT_EQ(craftPackets(), craftStart(MORPH, ININA, player().getObjectId()));
	EXPECT_EQ(countOf(AETHER_POWDER), 0);
	EXPECT_EQ(player().getCommonData()->getDp(), 0);
	EXPECT_TRUE(craftInProgress());
}

// CM_CRAFT.java:60: the craft type reaches startCrafting - type 1 needs the Cooking Enhancement Stone (checkCraft, CraftService.java:216-220:
// its message and the cancel pair without one) and uses it up with the materials
TEST_F(CraftPacketRunTest, TheCraftTypeReachesCraftService) {
	model::gameobjects::StaticObject& oven = station(3.0f);

	craft(0, OVEN, ROAST_ININA_RECIPE, oven.getObjectId(), {{ININA, 1}, {SALT, 2}}, 1);
	std::vector<std::vector<uint8_t>> refused{
		message(SM_SYSTEM_MESSAGE::STR_COMBINE_NO_COMPONENT_ITEM_SINGLE(itemTemplate(COOKING_STONE)->getL10n()))};
	for (std::vector<uint8_t>& packet : craftRefused(COOKING, ROAST_ININA, oven.getObjectId()))
		refused.push_back(std::move(packet));
	EXPECT_EQ(sent(), refused);
	EXPECT_EQ(countOf(ININA), 1);

	clearSent();
	give(830004, COOKING_STONE, 1);
	craft(0, OVEN, ROAST_ININA_RECIPE, oven.getObjectId(), {{ININA, 1}, {SALT, 2}}, 1);

	EXPECT_EQ(craftPackets(), craftStart(COOKING, ROAST_ININA, oven.getObjectId()));
	EXPECT_EQ(countOf(COOKING_STONE), 0);
	EXPECT_EQ(countOf(ININA), 0);
	EXPECT_TRUE(craftInProgress());
}

// CM_CRAFT.java:60 hands the craft type byte on as read, and only type 1 is the bonus craft (CraftService.java:123, :216): types 2 and 255 start
// the craft with the Cooking Enhancement Stone held and leave it. Between the two the first craft is aborted (CraftingTask.onInteractionAbort)
// and the materials given again, since checkCraft refuses a second craft while one is in progress (CraftService.java:145)
TEST_F(CraftPacketRunTest, OnlyCraftTypeOneUsesTheEnhancementStone) {
	model::gameobjects::StaticObject& oven = station(3.0f);
	give(830004, COOKING_STONE, 1);

	for (int32_t craftType : {2, 255}) {
		if (runtime::Ptr<skillengine::task::CraftingTask> started = craftInProgress()) {
			started->abort();
			give(830005, ININA, 1);
			give(830006, SALT, 2);
		}
		clearSent();

		craft(0, OVEN, ROAST_ININA_RECIPE, oven.getObjectId(), {{ININA, 1}, {SALT, 2}}, craftType);

		EXPECT_EQ(craftPackets(), craftStart(COOKING, ROAST_ININA, oven.getObjectId())) << "craft type " << craftType;
		EXPECT_EQ(countOf(COOKING_STONE), 1) << "craft type " << craftType;
		EXPECT_EQ(countOf(ININA), 0) << "craft type " << craftType;
		EXPECT_EQ(countOf(SALT), 0) << "craft type " << craftType;
		EXPECT_TRUE(craftInProgress()) << "craft type " << craftType;
	}
}

// CM_CRAFT.java:49-50: GameServer.isShuttingDownSoon() - a shutdown with at most 30 s left crafts nothing, so no material is lost
TEST_F(CraftPacketRunTest, NothingIsCraftedWhileTheServerShutsDownWithinThirtySeconds) {
	model::gameobjects::StaticObject& oven = station(3.0f);
	{
		ShutdownSoonScope shutdown;
		ASSERT_TRUE(GameServer::isShuttingDownSoon());
		roastInina(oven.getObjectId());
		expectNothing("a shutdown in 30 s");
	}

	roastInina(oven.getObjectId());
	EXPECT_TRUE(craftInProgress()) << "after the scope the same body crafts";
}

// CM_CRAFT.java:47-48: an unspawned player crafts nothing
TEST_F(CraftPacketRunTest, AnUnspawnedPlayerCraftsNothing) {
	model::gameobjects::StaticObject& oven = station(3.0f);
	player().getPosition()->setIsSpawned(false);

	roastInina(oven.getObjectId());

	expectNothing("not spawned");
}

// CM_CRAFT.java:47-48: a connection without an active player crafts nothing, and nothing is thrown
TEST_F(CraftPacketRunTest, WithoutAPlayerNothingIsCrafted) {
	model::gameobjects::StaticObject& oven = station(3.0f);
	TestClient loggedOut;
	EconomyDriver<CM_CRAFT> packet(CM_CRAFT_OPCODE);
	ASSERT_TRUE(packet.readOn(craftBody(0, OVEN, ROAST_ININA_RECIPE, oven.getObjectId(), {{ININA, 1}, {SALT, 2}}, 0), loggedOut.get()));

	EXPECT_NO_THROW(packet.runNow());

	expectNothing("no active player");
	EXPECT_TRUE(loggedOut->sentBytes().empty());
}

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
