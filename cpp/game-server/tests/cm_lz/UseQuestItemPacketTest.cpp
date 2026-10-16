// M5d E-10 (m5d-plan.md §7, D19, §17.3 N1; P5-16): CM_USE_ITEM with a <queststart> item, the start path of 39 XML quests. CM_USE_ITEM skips
// QuestEngine.onItemUseEvent for an item with a QuestStartAction (CM_USE_ITEM.java:84-88, "QuestStartAction opens the quest dialog itself"),
// so the handler of a start item registered with registerQuestItem is asked exactly once: by QuestStartAction.finishUse, after the cast
// (QuestStartAction.java:82-87). The actions themselves are tests/itemsvc/QuestItemActionsTest.cpp's.
//
// The run cases drive runImpl against the item packet fixture (ItemPacketTestSupport.h: a spawned level-1 warrior in Poeta and a real
// AionConnection whose send queue the cases read), with a probe handler for quest 1107 in the QuestEngine singleton, cleared again after the
// case (tests/cm_lz/AscensionPacketsTest.cpp's pattern). The rows are the shipped data's, verbatim: items/item_templates.xml:877018 (Broken Axe
// Handle, <queststart questid="1107"/>, no casting delay) and quest_data/quest_data.xml:937-942 (1107, minlevel 3).

#include "../cm_ak/ItemPacketTestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/network/aion/clientpackets/CM_USE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/AbstractQuestHandler.h"
#include "aion/gameserver/questEngine/handlers/HandlerResult.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/utils/cron/ThreadPoolManagerRunnableRunner.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::items {
namespace {

using questEngine::QuestEngine;
using questEngine::handlers::HandlerResult;
using serverpackets::SM_SYSTEM_MESSAGE;
namespace DialogAction = model::DialogAction;

/** the decoded opcode of ClientPacketInfo.gen.inc:48 (Java AionClientPacketFactory.java:65, State.IN_GAME) */
constexpr int32_t CM_USE_ITEM_OPCODE = 37;

constexpr int32_t THE_LOST_AXE = 1107;
constexpr int32_t BROKEN_AXE_HANDLE = 182200501;
constexpr int32_t AXE = 750101; // the axe's object id

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

/** quest_data/quest_data.xml:937-942, verbatim */
constexpr std::string_view QUEST_DATA_XML = R"xml(<quests>
	<quest id="1107" name="The Lost Axe" nameId="1102207" quest_zone="Poeta" minlevel_permitted="3" max_repeat_count="1" can_report="true" cannot_share="true" race_permitted="ELYOS" category="QUEST">
		<rewards gold="1560" exp="462"/>
		<quest_work_items>
			<quest_work_item item_id="182200501"/>
		</quest_work_items>
	</quest>
</quests>)xml";

/**
 * SM_ITEM_USAGE_ANIMATION(playerObjId, itemObjId, itemId) (SM_ITEM_USAGE_ANIMATION.java:22-30: time 0, end 1, unk3 1): D player, D target (the
 * player), D item object, D item id, D time, C end, C unk 0, C unk1 0, C unk2 (the field initializer's 1), D unk3
 */
std::vector<uint8_t> finishAnimation(int32_t playerObjId, int32_t itemObjId, int32_t itemId) {
	return javaPacket(SM_ITEM_USAGE_ANIMATION_OPCODE, PacketWriter().D(playerObjId).D(playerObjId).D(itemObjId).D(itemId).D(0).C(1).C(0).C(0).C(1).D(1));
}

/** What a quest handler was asked: the hook, the QuestEnv's quest and dialog action, and the item used (0 for the dialog) */
struct HandlerCall {
	std::string hook;
	int32_t questId = 0;
	int32_t dialogActionId = 0;
	int32_t itemObjId = 0;

	bool operator==(const HandlerCall&) const = default;
};

std::ostream& operator<<(std::ostream& os, const HandlerCall& call) {
	return os << call.hook << "(quest " << call.questId << ", action " << call.dialogActionId << ", item " << call.itemObjId << ")";
}

/** A quest handler that registers the axe as its quest item and records what it is asked (Immortal: the record is static) */
class AxeProbe final : public questEngine::handlers::AbstractQuestHandler {
public:
	AxeProbe() : AbstractQuestHandler(THE_LOST_AXE) {}

	static inline std::vector<HandlerCall> calls;
	static inline HandlerResult itemUseAnswer = HandlerResult::SUCCESS;

	void register_() override { QuestEngine::getInstance().registerQuestItem(BROKEN_AXE_HANDLE, getQuestId()); }

	HandlerResult onItemUseEvent(questEngine::model::QuestEnv& env, model::gameobjects::Item& item) override {
		calls.push_back({"onItemUseEvent", env.getQuestId(), env.getDialogActionId(), item.getObjectId()});
		return itemUseAnswer;
	}

	bool onDialogEvent(questEngine::model::QuestEnv& env) override {
		calls.push_back({"onDialogEvent", env.getQuestId(), env.getDialogActionId(), 0});
		return true;
	}
};

class UseQuestItemPacketTest : public ItemPacketTest {
protected:
	void SetUp() override {
		ItemPacketTest::SetUp();
		xml::LoadContext context;
		std::string itemRows(ITEM_TEMPLATES_XML);
		itemRows.insert(itemRows.rfind("</item_templates>"), BROKEN_AXE_HANDLE_ROW);
		dataholders::DataManager::ITEM_DATA.resetForTests(); // the base rows, republished with the axe
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, itemRows));
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(context, QUEST_DATA_XML));
		// Java PlayerService.loadPlayer: the quest states (PlayerQuestListDAO.load: none for this character)
		player().setQuestStateList(model::gameobjects::player::QuestStateList::create());
		// the probe, and the CronService QuestEngine.clear cancels the daily message through (QuestEngineTest's fixture)
		services::cron::CronService::resetForTests();
		services::cron::CronService::initSingleton(std::make_unique<utils::cron::ThreadPoolManagerRunnableRunner>(), std::chrono::locate_zone("UTC"),
			services::cron::CronService::Driver::EXECUTOR);
		AxeProbe::calls.clear();
		AxeProbe::itemUseAnswer = HandlerResult::SUCCESS;
		QuestEngine::getInstance().addQuestHandler(std::make_unique<AxeProbe>());
		ASSERT_TRUE(QuestEngine::getInstance().isRegisteredQuestItem(BROKEN_AXE_HANDLE));
	}

	void TearDown() override {
		QuestEngine::getInstance().clear(); // the handlers themselves stay (Immortal), unreachable
		services::cron::CronService::resetForTests();
		ItemPacketTest::TearDown();
		dataholders::DataManager::QUEST_DATA.resetForTests();
	}

	/** C_USE_ITEM: D item, C type 0 (CM_USE_ITEM.java:38-52) */
	void use(int32_t itemObjId) {
		Driver<CM_USE_ITEM> packet(CM_USE_ITEM_OPCODE);
		packet.readAndRun(PacketWriter().D(itemObjId).C(0).data, client->get());
	}

	std::vector<uint8_t> message(SM_SYSTEM_MESSAGE&& packet) { return serializedFor(std::move(packet)); }
};

TEST_F(UseQuestItemPacketTest, TheHandlerOfAStartItemIsAskedOnceByTheActionNotByThePacket) {
	f.commonData->setLevel(3); // 1107's minlevel_permitted
	Item& axe = stored(AXE, BROKEN_AXE_HANDLE, 1);

	use(AXE);

	// CM_USE_ITEM.java:84-88 skips onItemUseEvent (a QuestStartAction is among the actions); canAct is true, act has no casting delay, and
	// finishUse asks the handler with ASK_QUEST_ACCEPT (QuestStartAction.java:84-85). Its SUCCESS ends the use: no onDialog
	EXPECT_EQ(AxeProbe::calls, (std::vector<HandlerCall>{{"onItemUseEvent", THE_LOST_AXE, DialogAction::ASK_QUEST_ACCEPT, AXE}}));
	// the use itself: the finish animation (SM_ITEM_USAGE_ANIMATION(player, item, itemId): time 0, end 1, unk3 1) and STR_USE_ITEM
	EXPECT_EQ(sent(), exactly({finishAnimation(player().getObjectId(), AXE, BROKEN_AXE_HANDLE), message(SM_SYSTEM_MESSAGE::STR_USE_ITEM(axe.getL10n()))}));
	EXPECT_EQ(axe.getItemCount(), 1) << "a quest start item is not used up";
}

TEST_F(UseQuestItemPacketTest, AStartItemTheHandlerDoesNotTakeOpensTheQuestDialog) {
	f.commonData->setLevel(3);
	stored(AXE, BROKEN_AXE_HANDLE, 1);
	AxeProbe::itemUseAnswer = HandlerResult::UNKNOWN;

	use(AXE);

	// QuestStartAction.java:86-87: the generic dialog routing, QuestEngine.onDialog(QuestEnv(null, player, 1107, ASK_QUEST_ACCEPT))
	EXPECT_EQ(AxeProbe::calls, (std::vector<HandlerCall>{{"onItemUseEvent", THE_LOST_AXE, DialogAction::ASK_QUEST_ACCEPT, AXE},
								   {"onDialogEvent", THE_LOST_AXE, DialogAction::ASK_QUEST_ACCEPT, 0}}));
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items
