// M5d D-03 (m5d-plan.md §7, §18.3, P5-15): CM_DELETE_QUEST (C_GIVE_UP_QUEST), the client's abandon of a quest in the journal.
//
// Java: CM_DELETE_QUEST.java:22-37 - D quest id; a quest whose template has timer="true" gets its QUEST_TIMER task cancelled and the client's
// timer cleared (SM_QUEST_ACTION(questId, 0)) first, then QuestService.abandonQuest (QuestService.java:849-885, ported by M5d's engine
// overlay, E-02): nothing for an unknown quest, a quest that cannot be given up, a quest not held, COMPLETE or LOCKED; a quest completed
// before goes back to COMPLETE with its variables and flags cleared, any other is deleted from the list; the quest's work items leave the
// cube (the amount the quest needs, deleted with the delete type of the quest's status at that moment, ItemDeleteType.fromQuestStatus); a
// running quest timer ends (questTimerEnd: the same SM_QUEST_ACTION(questId, 0)); then SM_QUEST_ACTION(ABANDON) and the nearby quests.
// The run cases drive runImpl against the item packet fixture (ItemPacketTestSupport.h: a spawned level-1 warrior in Poeta and a real
// AionConnection whose send queue the cases read).
//
// Every quest and item row is the shipped data's, verbatim (file:line beside each), except one marked fixture row: no shipped quest sets
// timer="true" (quest_data/quest_data.xml has no such attribute), so the arm of CM_DELETE_QUEST.java:32-35 is driven with 1107's own row
// carrying the attribute, published only in the case that needs it. Expected packets are Java's bytes (writeOP + the writeImpl fields;
// SM_QUEST_ACTION.java, SM_DELETE_ITEM.java, SM_NEARBY_QUESTS.java; ServerPacketsOpcodes.java:46, :142, :145).

#include "ItemPacketTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/network/aion/clientpackets/CM_DELETE_QUEST.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/services/QuestService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

std::unique_ptr<AionClientPacket> CM_DELETE_QUEST_clientPacketFactory(int32_t opcode, const StateSet& validStates);

/** The friend CM_DELETE_QUEST.h declares: the field readImpl decoded, which Java keeps private */
struct CM_DELETE_QUESTTestAccess {
	static int32_t questId(const CM_DELETE_QUEST& p) { return p.questId; }
};

namespace testing::items {
namespace {

using network::test::LogCapture;
using questEngine::model::QuestState;
using questEngine::model::QuestStatus;

const char* BASE_CLIENT_PACKET_LOGGER = "com.aionemu.commons.network.packet.BaseClientPacket";

/** the decoded opcode of ClientPacketInfo.gen.inc:87 (Java AionClientPacketFactory: C_GIVE_UP_QUEST, State.IN_GAME) */
constexpr int32_t CM_DELETE_QUEST_OPCODE = 80;

// ServerPacketsOpcodes.java:142, :145
constexpr int32_t SM_QUEST_ACTION_OPCODE = 124;
constexpr int32_t SM_NEARBY_QUESTS_OPCODE = 127;

constexpr int32_t PROLOGUE = 1000; // cannot_giveup
constexpr int32_t THE_LOST_AXE = 1107; // a work item: the Broken Axe Handle
constexpr int32_t BROKEN_AXE_HANDLE = 182200501;
constexpr int32_t AXE = 790001; // the axe's object id

// ItemPacketService.ItemDeleteType (ItemPacketService.java:121-122): fromQuestStatus START / COMPLETE
constexpr int32_t QUEST_START_DELETE = 0x34;
constexpr int32_t QUEST_COMPLETE_DELETE = 0x31;

/** quest_data/quest_data.xml, verbatim rows */
constexpr std::string_view QUEST_DATA_XML = R"xml(<quests>
	<!-- :6-8 -->
	<quest id="1000" name="Prologue" nameId="1102000" quest_zone="Poeta" minlevel_permitted="1" max_repeat_count="1" cannot_share="true" cannot_giveup="true" race_permitted="ELYOS" category="QUEST">
		<rewards exp="1"/>
	</quest>
	<!-- :937-942 -->
	<quest id="1107" name="The Lost Axe" nameId="1102207" quest_zone="Poeta" minlevel_permitted="3" max_repeat_count="1" can_report="true" cannot_share="true" race_permitted="ELYOS" category="QUEST">
		<rewards gold="1560" exp="462"/>
		<quest_work_items>
			<quest_work_item item_id="182200501"/>
		</quest_work_items>
	</quest>
</quests>)xml";

/** A FIXTURE ROW, not shipped data: quest_data.xml:937's 1107 with timer="true" added (no shipped quest has the attribute) */
constexpr std::string_view TIMER_QUEST_FIXTURE_XML = R"xml(<quests>
	<quest id="1107" name="The Lost Axe" nameId="1102207" quest_zone="Poeta" minlevel_permitted="3" max_repeat_count="1" can_report="true" cannot_share="true" race_permitted="ELYOS" category="QUEST" timer="true">
		<rewards gold="1560" exp="462"/>
		<quest_work_items>
			<quest_work_item item_id="182200501"/>
		</quest_work_items>
	</quest>
</quests>)xml";

/** items/item_templates.xml:877018, verbatim */
constexpr std::string_view BROKEN_AXE_HANDLE_ROW = R"xml(
	<item_template id="182200501" name="Broken Axe Handle" level="2" cName="quest_1107a" mask="20545" item_group="QUEST" quality="COMMON" price="1" desc="1105001" activate_target="STANDALONE" activate_count="1000">
		<actions>
			<queststart questid="1107"/>
		</actions>
		<uselimits usedelay="15000" usedelayid="61"/>
		<inventory id="2"/>
	</item_template>
)xml";

/** C_GIVE_UP_QUEST: D quest (CM_DELETE_QUEST.java:22-25) */
std::vector<uint8_t> deleteBody(int32_t questId) {
	return PacketWriter().D(questId).data;
}

/** SM_QUEST_ACTION(ABANDON, qs).writeImpl: C ActionType.ABANDON 3, D quest, D 0 */
std::vector<uint8_t> questAbandoned(int32_t questId) {
	return javaPacket(SM_QUEST_ACTION_OPCODE, PacketWriter().C(3).D(questId).D(0));
}

/** SM_QUEST_ACTION(questId, timer).writeImpl: C ActionType.TIMER 4, D quest, D timer, C timer > 0 */
std::vector<uint8_t> questTimer(int32_t questId, int32_t timer) {
	return javaPacket(SM_QUEST_ACTION_OPCODE, PacketWriter().C(4).D(questId).D(timer).C(timer > 0 ? 1 : 0));
}

/** SM_NEARBY_QUESTS with no quest (SM_NEARBY_QUESTS.java:22-30): the map instance of the fixture spawns no npc that starts one */
std::vector<uint8_t> noNearbyQuests() {
	return javaPacket(SM_NEARBY_QUESTS_OPCODE, PacketWriter().C(0).H(0));
}

std::unique_ptr<CM_DELETE_QUEST> readPacket(const std::vector<uint8_t>& data, int32_t& unread) {
	std::vector<uint8_t> copy = data;
	auto packet = std::make_unique<CM_DELETE_QUEST>(CM_DELETE_QUEST_OPCODE, StateSet{AionConnection_State::IN_GAME});
	packet->setBuffer(commons::utils::ByteBuffer::wrap(copy));
	if (!packet->read())
		return nullptr;
	unread = packet->getRemainingBytes();
	return packet;
}

TEST(DeleteQuestReadTest, TheBodyIsTheQuestId) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	int32_t unread = -1;
	std::unique_ptr<CM_DELETE_QUEST> p = readPacket(deleteBody(0x01020304), unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(CM_DELETE_QUESTTestAccess::questId(*p), 0x01020304);
	EXPECT_EQ(unread, 0);
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();

	ASSERT_NE(readPacket(PacketWriter().H(7).data, unread), nullptr);
	EXPECT_EQ(capture.count("Missing D"), 1) << capture.dump();
}

TEST(DeleteQuestReadTest, TheMarkerRegistersTheClassUnderItsJavaOpcode) {
	EXPECT_NE(dynamic_cast<CM_DELETE_QUEST*>(
				  CM_DELETE_QUEST_clientPacketFactory(CM_DELETE_QUEST_OPCODE, StateSet{AionConnection_State::IN_GAME}).get()),
		nullptr);
	int32_t found = 0;
#define AION_CLIENT_PACKET_INFO(opcode, wireOpcode, Class, clientName, ...)                                                                             \
	if (std::string_view(#Class) == "CM_DELETE_QUEST") {                                                                                                \
		EXPECT_EQ(opcode, CM_DELETE_QUEST_OPCODE);                                                                                                      \
		EXPECT_EQ((StateSet{__VA_ARGS__}), (StateSet{AionConnection_State::IN_GAME}));                                                                \
		++found;                                                                                                                                        \
	}
	using enum AionConnection_State;
#include "aion/gameserver/network/aion/ClientPacketInfo.gen.inc"
#undef AION_CLIENT_PACKET_INFO
	EXPECT_EQ(found, 1);
}

class DeleteQuestRunTest : public ItemPacketTest {
protected:
	void SetUp() override {
		ItemPacketTest::SetUp();
		xml::LoadContext context;
		std::string itemRows(ITEM_TEMPLATES_XML);
		itemRows.insert(itemRows.rfind("</item_templates>"), BROKEN_AXE_HANDLE_ROW);
		dataholders::DataManager::ITEM_DATA.resetForTests(); // the base rows, republished with the work item
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, itemRows));
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(context, QUEST_DATA_XML));
		// Java PlayerService.loadPlayer: the quest states (PlayerQuestListDAO.load: none for this character)
		player().setQuestStateList(model::gameobjects::player::QuestStateList::create());
	}

	void TearDown() override {
		if (f.player)
			f.player->getController().cancelTask(model::TaskId::QUEST_TIMER);
		ItemPacketTest::TearDown();
		dataholders::DataManager::QUEST_DATA.resetForTests();
	}

	void deleteQuest(int32_t questId) {
		Driver<CM_DELETE_QUEST> packet(CM_DELETE_QUEST_OPCODE);
		packet.readAndRun(deleteBody(questId), client->get());
	}

	/** A quest state as PlayerQuestListDAO loads it (stored), in the player's quest list */
	runtime::Ref<QuestState> hold(int32_t questId, QuestStatus status, int32_t questVars, int32_t flags, int32_t completeCount) {
		runtime::Ref<QuestState> qs = QuestState::create(questId, status, questVars, flags, completeCount, std::nullopt, std::nullopt, std::nullopt);
		qs->setPersistentState(model::gameobjects::Persistable::PersistentState::UPDATED);
		player().getQuestStateList()->addQuest(questId, *qs);
		return qs;
	}

	bool holds(int32_t questId) { return static_cast<bool>(player().getQuestStateList()->getQuestState(questId)); }

	bool timerRuns() { return player().getController().hasTask(model::TaskId::QUEST_TIMER); }
};

TEST_F(DeleteQuestRunTest, AQuestNeverCompletedIsDroppedWithItsWorkItem) {
	stored(AXE, BROKEN_AXE_HANDLE, 1);
	runtime::Ref<QuestState> qs = hold(THE_LOST_AXE, QuestStatus::START, 1, 0, 0);

	deleteQuest(THE_LOST_AXE);

	// abandonQuest: completeCount 0 -> QuestStateList.deleteQuest (DELETED for the DAO); the work item (count 1) deleted while the quest is
	// START (ItemDeleteType.QUEST_START); no timer; then ABANDON and the nearby quests
	EXPECT_FALSE(holds(THE_LOST_AXE));
	EXPECT_EQ(qs->getPersistentState(), model::gameobjects::Persistable::PersistentState::DELETED);
	EXPECT_EQ(player().getInventory().getItemCountByItemId(BROKEN_AXE_HANDLE), 0);
	// (ItemPacketService.sendItemDeletePacket, ItemPacketService.java:178-185: SM_DELETE_ITEM, then the cube's size)
	EXPECT_EQ(sent(), exactly({deleteItem(AXE, QUEST_START_DELETE), cubeSize(StorageType::CUBE, 0), questAbandoned(THE_LOST_AXE), noNearbyQuests()}));
}

TEST_F(DeleteQuestRunTest, AQuestCompletedBeforeGoesBackToCompleteWithoutItsProgress) {
	stored(AXE, BROKEN_AXE_HANDLE, 1);
	runtime::Ref<QuestState> qs = hold(THE_LOST_AXE, QuestStatus::START, 5, 2, 1);

	deleteQuest(THE_LOST_AXE);

	// QuestService.java:861-864: COMPLETE (setStatus(COMPLETE, false): the complete count and time stay), variables and flags 0; the work
	// item leaves with the status of that moment (QUEST_COMPLETE)
	EXPECT_TRUE(holds(THE_LOST_AXE));
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(qs->getCompleteCount(), 1);
	EXPECT_EQ(qs->getQuestVars()->getQuestVars(), 0);
	EXPECT_EQ(qs->getFlags(), 0);
	EXPECT_EQ(sent(), exactly({deleteItem(AXE, QUEST_COMPLETE_DELETE), cubeSize(StorageType::CUBE, 0), questAbandoned(THE_LOST_AXE), noNearbyQuests()}));
}

TEST_F(DeleteQuestRunTest, AQuestThatCannotBeGivenUpOrIsNotInProgressStays) {
	stored(AXE, BROKEN_AXE_HANDLE, 1);
	runtime::Ref<QuestState> prologue = hold(PROLOGUE, QuestStatus::START, 1, 0, 0);

	deleteQuest(PROLOGUE); // cannot_giveup (QuestService.java:854-855)
	deleteQuest(THE_LOST_AXE); // not held (:858)
	deleteQuest(9999); // no template (:851-852; CM_DELETE_QUEST.java:32's qt != null)

	runtime::Ref<QuestState> done = hold(THE_LOST_AXE, QuestStatus::COMPLETE, 0, 0, 1);
	deleteQuest(THE_LOST_AXE); // COMPLETE (:858)

	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(prologue->getStatus(), QuestStatus::START);
	EXPECT_TRUE(holds(PROLOGUE));
	EXPECT_EQ(done->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(player().getInventory().getItemCountByItemId(BROKEN_AXE_HANDLE), 1);
}

TEST_F(DeleteQuestRunTest, ARunningQuestTimerEndsWithTheAbandon) {
	hold(THE_LOST_AXE, QuestStatus::START, 1, 0, 0);
	services::QuestService::questTimerStart(*questEngine::model::QuestEnv::create(nullptr, player(), THE_LOST_AXE), 60);
	ASSERT_TRUE(timerRuns());
	clearSent();

	deleteQuest(THE_LOST_AXE);

	// 1107 has no timer="true", so CM_DELETE_QUEST leaves the timer to abandonQuest: hasTask(QUEST_TIMER) -> questTimerEnd (:875-876), the
	// timer cleared once, before ABANDON
	EXPECT_FALSE(timerRuns());
	EXPECT_EQ(sent(), exactly({questTimer(THE_LOST_AXE, 0), questAbandoned(THE_LOST_AXE), noNearbyQuests()}));
}

TEST_F(DeleteQuestRunTest, ATimerQuestHasItsTimerClearedByThePacketFirst) {
	dataholders::DataManager::QUEST_DATA.resetForTests();
	xml::LoadContext context;
	dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(context, TIMER_QUEST_FIXTURE_XML));
	hold(THE_LOST_AXE, QuestStatus::START, 1, 0, 0);

	// CM_DELETE_QUEST.java:32-35 without a running timer: the client's timer is cleared anyway
	deleteQuest(THE_LOST_AXE);
	EXPECT_EQ(sent(), exactly({questTimer(THE_LOST_AXE, 0), questAbandoned(THE_LOST_AXE), noNearbyQuests()}));

	// with one: the packet cancels it before abandonQuest, which then finds no QUEST_TIMER task - the timer is cleared once
	hold(THE_LOST_AXE, QuestStatus::START, 1, 0, 0);
	services::QuestService::questTimerStart(*questEngine::model::QuestEnv::create(nullptr, player(), THE_LOST_AXE), 60);
	clearSent();
	deleteQuest(THE_LOST_AXE);
	EXPECT_FALSE(timerRuns());
	EXPECT_EQ(sent(), exactly({questTimer(THE_LOST_AXE, 0), questAbandoned(THE_LOST_AXE), noNearbyQuests()}));
}

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
