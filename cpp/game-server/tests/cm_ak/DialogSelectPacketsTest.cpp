// M5c D-03 (m5c-plan.md §5, P5-15): CM_DIALOG_SELECT (C_HACTION), the client's pick of a dialog action, and CM_CLOSE_DIALOG (C_END_DIALOG).
//
// Java: CM_DIALOG_SELECT.java:47-124, CM_CLOSE_DIALOG.java:24-36. The read cases lay each body out field by field from the Java readImpl; the
// run cases drive runImpl against the item packet fixture (ItemPacketTestSupport.h: a spawned level-1 warrior in Poeta and a real
// AionConnection whose send queue the cases read) with npcs of real rows spawned beside him and made known to him:
// - CM_DIALOG_SELECT's own checks in Java order: the unknown action, the two audits (a function the npc does not offer; an action below
//   SELECT1 or a function at an npc whose isInteractionAllowed refuses), the quest-report arm of target 0, the talk range of
//   NpcController.onDialogSelect, then DialogService.onDialogSelect (its own arms are tests/playersvc/DialogServiceTest.cpp's).
// - CM_CLOSE_DIALOG: DialogService.onCloseDialog (DIALOG_FINISH to the AI, the mailbox closed) and then SM_LOOKATOBJECT built from the npc -
//   after the finish, so the npc's cleared target is what the packet carries.
// The npc rows are npc_templates.xml's, the trade rows npc_trade_list.xml's, the goods rows goodslists.xml's and the quest row
// quest_data.xml's, verbatim (file:line beside each). The npcs' rows name ai="general" and ai="aggressive", whose handlers live in the handler
// library this executable does not link: the cases that talk give the npc a leaf AI with GeneralNpcAI's dialog hooks (GeneralNpcAI.java:49-57).
//
// NOT COVERED, and named: the `isTrading()` bail-out (no exchange can be registered before ExchangeService.registerExchange is ported, M5c
// T-02; m5c-plan.md W-28 gives it to T-04) and a player target (PlayerController.onDialogSelect's private store arm, M5c T-03).

#include "ItemPacketTestSupport.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/handler/TalkEventHandler.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/PricesConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/GoodsListData.bind.h"
#include "aion/gameserver/dataholders/GoodsListData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/TradeListData.bind.h"
#include "aion/gameserver/dataholders/TradeListData.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/clientpackets/CM_CLOSE_DIALOG.h"
#include "aion/gameserver/network/aion/clientpackets/CM_DIALOG_SELECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/player/PlayerMailboxState.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

std::unique_ptr<AionClientPacket> CM_DIALOG_SELECT_clientPacketFactory(int32_t opcode, const StateSet& validStates);
std::unique_ptr<AionClientPacket> CM_CLOSE_DIALOG_clientPacketFactory(int32_t opcode, const StateSet& validStates);

/** The friends the two headers declare: the fields readImpl decoded, which Java keeps private */
struct CM_DIALOG_SELECTTestAccess {
	static int32_t targetObjectId(const CM_DIALOG_SELECT& p) { return p.targetObjectId; }
	static int32_t dialogActionId(const CM_DIALOG_SELECT& p) { return p.dialogActionId; }
	static int32_t extendedRewardIndex(const CM_DIALOG_SELECT& p) { return p.extendedRewardIndex; }
	static int32_t lastPage(const CM_DIALOG_SELECT& p) { return p.lastPage; }
	static int32_t questId(const CM_DIALOG_SELECT& p) { return p.questId; }
	static int32_t unk(const CM_DIALOG_SELECT& p) { return p.unk; }
};

struct CM_CLOSE_DIALOGTestAccess {
	static int32_t targetObjectId(const CM_CLOSE_DIALOG& p) { return p.targetObjectId; }
};

namespace testing::items {
namespace {

using model::gameobjects::Npc;
using network::test::LogCapture;
using serverpackets::SM_SYSTEM_MESSAGE;
using services::player::PlayerMailboxState;
namespace DialogAction = model::DialogAction;

const char* BASE_CLIENT_PACKET_LOGGER = "com.aionemu.commons.network.packet.BaseClientPacket";
const char* CM_DIALOG_SELECT_LOGGER = "com.aionemu.gameserver.network.aion.clientpackets.CM_DIALOG_SELECT";
const char* AUDIT_LOGGER = "AUDIT_LOG";

/** the decoded opcodes of ClientPacketInfo.gen.inc (Java AionClientPacketFactory.java:81-82, State.IN_GAME) */
constexpr int32_t CM_CLOSE_DIALOG_OPCODE = 53;
constexpr int32_t CM_DIALOG_SELECT_OPCODE = 54;

// ServerPacketsOpcodes.java:42, :58, :78, :271
constexpr int32_t SM_MESSAGE_OPCODE = 24;
constexpr int32_t SM_LOOKATOBJECT_OPCODE = 40;
constexpr int32_t SM_DIALOG_WINDOW_OPCODE = 60;
constexpr int32_t SM_TRADELIST_OPCODE = 253;

constexpr int32_t MINALINERK = 798007;
constexpr int32_t BAEVRUNERK = 798008;
constexpr int32_t LALRINERK = 833548;
constexpr int32_t SLEEPING_ON_THE_JOB = 1101;

/** npc_templates.xml, verbatim rows */
constexpr std::string_view NPC_TEMPLATES_XML = R"xml(<npc_templates>
	<!-- :461604-461610, BUY and SELL -->
	<npc_template npc_id="798007" level="9" name="minalinerk" name_id="351126" height="1.16875" title_id="350377" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2100" hpgauge="3">
		<stats maxHp="2568">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
		<talk_info distance="5" is_dialog="true" func_dialogs="2 3" can_talk_invisible="false" />
	</npc_template>
	<!-- :461611-461617, EXTEND_INVENTORY only -->
	<npc_template npc_id="798008" level="9" name="baevrunerk" name_id="351141" height="1.16875" title_id="350421" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2100" hpgauge="3">
		<stats maxHp="2568">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
		<talk_info distance="5" is_dialog="true" func_dialogs="47" can_talk_invisible="false" />
	</npc_template>
	<!-- :562457-562463, BUY and SELL for level 55 and above (LEVEL_HIGH) -->
	<npc_template npc_id="833548" level="1" name="lalrinerk" name_id="466577" height="1.375" title_id="466578" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" tribe="USEALL" type="GENERAL" ai="aggressive" srange="20" attack_speed="2000" hpgauge="3">
		<stats maxHp="124">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.7" side="0.444" upper="1.375" />
		<talk_info distance="5" is_dialog="true" func_dialogs="2 3" subdialog_type="LEVEL_HIGH" subdialog_value="55" can_talk_invisible="false" />
	</npc_template>
</npc_templates>)xml";

/** npc_trade_list.xml:2186-2189, verbatim */
constexpr std::string_view NPC_TRADE_LIST_XML = R"xml(<npc_trade_list>
	<tradelist_template npc_id="798007" buy_price_rate="200">
		<tradelist id="132" />
		<tradelist id="720" />
	</tradelist_template>
</npc_trade_list>)xml";

/** goodslists/goodslists.xml:13484-13488 and :27984-27987, verbatim */
constexpr std::string_view GOODSLISTS_XML = R"xml(<goodslists>
    <list id="132">
        <item id="169000003"/>
        <item id="165000001"/>
        <item id="169300002"/>
    </list>
    <list id="720">
        <item id="162000052"/>
        <item id="162000057"/>
    </list>
</goodslists>)xml";

/** quest_data/quest_data.xml:895-897, verbatim: a Poeta quest the player can report without an npc (can_report) */
constexpr std::string_view QUEST_DATA_XML = R"xml(<quests>
	<quest id="1101" name="Sleeping on the Job" nameId="1102201" quest_zone="Poeta" minlevel_permitted="1" max_repeat_count="1" can_report="true" race_permitted="ELYOS" category="IMPORTANT">
		<rewards gold="120" exp="130"/>
	</quest>
</quests>)xml";

/** The templates the npcs are created from, kept for the process as DataManager keeps its own (the published NPC_DATA below is reset) */
const dataholders::NpcData& npcTemplates() {
	static const dataholders::NpcData* holder = [] {
		static xml::LoadContext context;
		return xml::bindString<dataholders::NpcData>(context, NPC_TEMPLATES_XML).release();
	}();
	return *holder;
}

/** C_HACTION: D target, H action, H extended reward index, H last page, D quest, H unk (CM_DIALOG_SELECT.java:47-54) */
std::vector<uint8_t> selectBody(int32_t target, int32_t action, int32_t questId = 0, int32_t lastPage = 0, int32_t rewardIndex = 0) {
	return PacketWriter().D(target).H(action).H(rewardIndex).H(lastPage).D(questId).H(0x4747).data;
}

/** C_END_DIALOG: D target (CM_CLOSE_DIALOG.java:24-26) */
std::vector<uint8_t> closeBody(int32_t target) {
	return PacketWriter().D(target).data;
}

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

int32_t checkTableEntry(std::string_view className, int32_t expectedOpcode) {
	using enum AionConnection_State;
	int32_t found = 0;
#define AION_CLIENT_PACKET_INFO(opcode, wireOpcode, Class, clientName, ...)                                                                             \
	if (std::string_view(#Class) == className) {                                                                                                        \
		EXPECT_EQ(opcode, expectedOpcode) << className;                                                                                                 \
		EXPECT_EQ((StateSet{__VA_ARGS__}), (StateSet{IN_GAME})) << className;                                                                          \
		++found;                                                                                                                                        \
	}
#include "aion/gameserver/network/aion/ClientPacketInfo.gen.inc"
#undef AION_CLIENT_PACKET_INFO
	return found;
}

TEST(DialogSelectReadTest, DialogSelectReadsItsSixFieldsWithUnsignedShorts) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	int32_t unread = -1;
	std::unique_ptr<CM_DIALOG_SELECT> p =
		readPacket<CM_DIALOG_SELECT>(PacketWriter().D(0x01020304).H(0xFFFF).H(0x8001).H(0x7FFE).D(0x0A0B0C0D).H(0xFFFE).data, CM_DIALOG_SELECT_OPCODE,
			unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(CM_DIALOG_SELECTTestAccess::targetObjectId(*p), 0x01020304);
	EXPECT_EQ(CM_DIALOG_SELECTTestAccess::dialogActionId(*p), 0xFFFF) << "readUH";
	EXPECT_EQ(CM_DIALOG_SELECTTestAccess::extendedRewardIndex(*p), 0x8001) << "readUH";
	EXPECT_EQ(CM_DIALOG_SELECTTestAccess::lastPage(*p), 0x7FFE);
	EXPECT_EQ(CM_DIALOG_SELECTTestAccess::questId(*p), 0x0A0B0C0D);
	EXPECT_EQ(CM_DIALOG_SELECTTestAccess::unk(*p), 0xFFFE) << "readUH, unk 4.7";
	EXPECT_EQ(unread, 0);
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(DialogSelectReadTest, CloseDialogReadsTheTarget) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	int32_t unread = -1;
	std::unique_ptr<CM_CLOSE_DIALOG> p = readPacket<CM_CLOSE_DIALOG>(closeBody(0x11223344), CM_CLOSE_DIALOG_OPCODE, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(CM_CLOSE_DIALOGTestAccess::targetObjectId(*p), 0x11223344);
	EXPECT_EQ(unread, 0);
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(DialogSelectReadTest, TheMarkersRegisterTheClassesUnderTheirJavaOpcodes) {
	EXPECT_NE(dynamic_cast<CM_DIALOG_SELECT*>(
				  CM_DIALOG_SELECT_clientPacketFactory(CM_DIALOG_SELECT_OPCODE, StateSet{AionConnection_State::IN_GAME}).get()),
		nullptr);
	EXPECT_NE(dynamic_cast<CM_CLOSE_DIALOG*>(CM_CLOSE_DIALOG_clientPacketFactory(CM_CLOSE_DIALOG_OPCODE, StateSet{AionConnection_State::IN_GAME}).get()),
		nullptr);
	EXPECT_EQ(checkTableEntry("CM_DIALOG_SELECT", CM_DIALOG_SELECT_OPCODE), 1);
	EXPECT_EQ(checkTableEntry("CM_CLOSE_DIALOG", CM_CLOSE_DIALOG_OPCODE), 1);
}

/** GeneralNpcAI's dialog hooks (GeneralNpcAI.java:49-57) on the plain NpcAI of this executable */
class TalkingNpcAI final : public ai::NpcAI {
public:
	explicit TalkingNpcAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDialogStart(model::gameobjects::player::Player& player) override { ai::handler::TalkEventHandler::onTalk(*this, player); }

	void handleDialogFinish(model::gameobjects::player::Player& player) override { ai::handler::TalkEventHandler::onFinishTalk(*this, player); }
};

class DialogSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	DialogSpawnTemplate(model::templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

uint64_t unportedHitsIn(std::string_view file) {
	uint64_t hits = 0;
	for (const runtime::UnportedHit& hit : runtime::unportedHits()) {
		if (hit.file.find(file) != std::string::npos)
			hits += hit.hits;
	}
	return hits;
}

class DialogSelectRunTest : public ItemPacketTest {
protected:
	void SetUp() override {
		ItemPacketTest::SetUp();
		runtime::resetUnportedHitsForTests();
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
		// the Java defaults of the keys the packet and the BUY arm read (AdminConfig.java:59-60, LoggingConfig.java:10-11, PricesConfig.java:28)
		savedDialogInfo = configs::administration::AdminConfig::DIALOG_INFO.exchange(9);
		savedLogAudit = configs::main::LoggingConfig::LOG_AUDIT.exchange(true);
		savedBuyModifier = configs::main::PricesConfig::VENDOR_BUY_MODIFIER.exchange(100);
		xml::LoadContext context;
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, NPC_TEMPLATES_XML));
		dataholders::DataManager::TRADE_LIST_DATA.publish(xml::bindString<dataholders::TradeListData>(context, NPC_TRADE_LIST_XML));
		dataholders::DataManager::GOODSLIST_DATA.publish(xml::bindString<dataholders::GoodsListData>(context, GOODSLISTS_XML));
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(context, QUEST_DATA_XML));
		player().setMailbox(std::make_unique<model::gameobjects::player::Mailbox>(player()));
	}

	void TearDown() override {
		if (f.player)
			f.player->setTarget(nullptr);
		npcs.clear(); // before the map instance their positions name
		spawnGroups.clear();
		ItemPacketTest::TearDown();
		dataholders::DataManager::QUEST_DATA.resetForTests();
		dataholders::DataManager::GOODSLIST_DATA.resetForTests();
		dataholders::DataManager::TRADE_LIST_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
		configs::main::PricesConfig::VENDOR_BUY_MODIFIER.store(savedBuyModifier);
		configs::main::LoggingConfig::LOG_AUDIT.store(savedLogAudit);
		configs::administration::AdminConfig::DIALOG_INFO.store(savedDialogInfo);
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
	}

	/** The npc of the row spawned `x` on the x axis at (x, 100, 50) with the talking AI in its spawned state, and known to the player */
	Npc& npcAt(int32_t npcId, float x) {
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, npcId, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<DialogSpawnTemplate>(*group, x, 100.0f, 50.0f));
		runtime::Ref<Npc> npc = model::gameobjects::VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), spawn,
			npcTemplates().getNpcTemplate(npcId));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		npc->setPosition(world::WorldPosition::create(210010000, x, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(x, 100.0f, 50.0f)));
		npc->getPosition()->setIsSpawned(true);
		auto ai = std::make_unique<TalkingNpcAI>(*npc);
		TalkingNpcAI& talking = *ai;
		npc->replaceAi(std::move(ai));
		talking.setStateIfNot(ai::AIState::IDLE); // a new AI is CREATED, which handles only the spawn events (AIState.java)
		f.knownList().addForTest(*npc);
		spawnGroups.push_back(group);
		npcs.push_back(npc);
		clearSent(); // what the player's see notification sent
		return *npc;
	}

	void dialogSelect(const std::vector<uint8_t>& body) {
		Driver<CM_DIALOG_SELECT> packet(CM_DIALOG_SELECT_OPCODE);
		packet.readAndRun(body, client->get());
	}

	void closeDialog(int32_t target) {
		Driver<CM_CLOSE_DIALOG> packet(CM_CLOSE_DIALOG_OPCODE);
		packet.readAndRun(closeBody(target), client->get());
	}

	/** SM_TRADELIST of minalinerk under the fixture's defaults (tests/playersvc/DialogServiceTest.cpp derives it field by field) */
	static std::vector<uint8_t> minalinerkTradeList(int32_t npcObjectId) {
		return javaPacket(SM_TRADELIST_OPCODE, PacketWriter().D(npcObjectId).C(1).D(100).D(100).C(1).C(1).H(2).D(132).D(720).H(0));
	}

	std::shared_ptr<const std::string> savedMissingAiHandlers;
	int8_t savedDialogInfo = 0;
	bool savedLogAudit = false;
	int32_t savedBuyModifier = 0;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<runtime::Ref<Npc>> npcs;
};

TEST_F(DialogSelectRunTest, BuyAtAMerchantInTalkRangeOpensItsTradeList) {
	Npc& merchant = npcAt(MINALINERK, 103.0f);

	dialogSelect(selectBody(merchant.getObjectId(), DialogAction::BUY));

	// CM_DIALOG_SELECT.java:110-120 pass (a function the npc offers, interaction allowed) -> NpcController.onDialogSelect (in talk range, the
	// AI answers false) -> DialogService.onDialogSelect(BUY)
	EXPECT_EQ(sent(), exactly({minalinerkTradeList(merchant.getObjectId())}));
}

TEST_F(DialogSelectRunTest, AnUnknownDialogActionIsLoggedAndDropped) {
	Npc& merchant = npcAt(MINALINERK, 103.0f);
	ASSERT_FALSE(DialogAction::nameOf(0xFFFF).has_value()) << "the premise: 65535 names no DialogAction";
	LogCapture capture({CM_DIALOG_SELECT_LOGGER});

	dialogSelect(selectBody(merchant.getObjectId(), 0xFFFF, 1100));

	// :65-69
	EXPECT_TRUE(capture.contains("Received unknown dialog action id 65535 (quest 1100) from " + player().toString())) << capture.dump();
	EXPECT_TRUE(sent().empty());
}

TEST_F(DialogSelectRunTest, AFunctionTheNpcDoesNotOfferIsAudited) {
	Npc& baevrunerk = npcAt(BAEVRUNERK, 103.0f); // func_dialogs 47 only; BUY (2) is a function dialog of NPC_DATA (minalinerk's)
	LogCapture capture({AUDIT_LOGGER});

	dialogSelect(selectBody(baevrunerk.getObjectId(), DialogAction::BUY));

	// :108-112
	EXPECT_TRUE(capture.contains("tried to use unsupported dialog action BUY on " + baevrunerk.toString())) << capture.dump();
	EXPECT_TRUE(sent().empty());
}

TEST_F(DialogSelectRunTest, AFunctionAtAnNpcThatRefusesThePlayerIsAudited) {
	Npc& lalrinerk = npcAt(LALRINERK, 103.0f); // LEVEL_HIGH 55; the player is level 1
	LogCapture capture({AUDIT_LOGGER});

	dialogSelect(selectBody(lalrinerk.getObjectId(), DialogAction::SELL));

	// :113-116: a function dialog and !isInteractionAllowed
	EXPECT_TRUE(capture.contains("tried to illegally use dialog action SELL on " + lalrinerk.toString())) << capture.dump();
	EXPECT_TRUE(sent().empty());
}

TEST_F(DialogSelectRunTest, AnActionBelowSelect1IsAuditedAtARefusingNpcButAPageFromSelect1OnIsNot) {
	Npc& lalrinerk = npcAt(LALRINERK, 103.0f);
	LogCapture capture({AUDIT_LOGGER});

	dialogSelect(selectBody(lalrinerk.getObjectId(), DialogAction::QUEST_SELECT)); // 31: no function dialog of NPC_DATA, but below SELECT1
	EXPECT_TRUE(capture.contains("tried to illegally use dialog action QUEST_SELECT on " + lalrinerk.toString())) << capture.dump();
	EXPECT_TRUE(sent().empty());

	// :117: `dialogActionId < SELECT1` - SELECT1 (1011) itself is not below it, and neither is 1012: no function dialog either, so the dialog
	// goes on to the npc
	dialogSelect(selectBody(lalrinerk.getObjectId(), DialogAction::SELECT1));
	dialogSelect(selectBody(lalrinerk.getObjectId(), DialogAction::SELECT1 + 1));
	EXPECT_EQ(capture.count("tried to illegally use"), 1) << capture.dump();
	// DialogService's default arm: the next page (DialogService.java:273-274, 289-290)
	EXPECT_EQ(sent(), exactly({javaPacket(SM_DIALOG_WINDOW_OPCODE, PacketWriter().D(lalrinerk.getObjectId()).H(1011).D(0).H(0).H(0)),
						  javaPacket(SM_DIALOG_WINDOW_OPCODE, PacketWriter().D(lalrinerk.getObjectId()).H(1012).D(0).H(0).H(0))}));
}

TEST_F(DialogSelectRunTest, OutOfTalkRangeTheNpcIgnoresTheSelection) {
	Npc& merchant = npcAt(MINALINERK, 120.0f); // 20 m

	dialogSelect(selectBody(merchant.getObjectId(), DialogAction::BUY));

	EXPECT_TRUE(sent().empty()) << "NpcController.java:266-267: not in talk range";
}

TEST_F(DialogSelectRunTest, AnUnknownTargetIsIgnored) {
	npcAt(MINALINERK, 103.0f);

	dialogSelect(selectBody(4242, DialogAction::BUY));

	EXPECT_TRUE(sent().empty()) << ":109: getObject answers null, and `null instanceof Creature` is false";
}

TEST_F(DialogSelectRunTest, TheDialogInfoOfAStaffMemberComesFirst) {
	Npc& merchant = npcAt(MINALINERK, 103.0f);
	f.account->setAccessLevel(9);

	dialogSelect(selectBody(merchant.getObjectId(), DialogAction::BUY));

	// :61-63: hasAccess(DIALOG_INFO 9) -> "Quest ID: 0, Dialog Action: BUY (ID: 2)" as SM_MESSAGE, then the dialog as for anyone
	std::vector<std::vector<uint8_t>> packets = sent();
	EXPECT_EQ(opcodesOf(packets), (std::vector<int32_t>{SM_MESSAGE_OPCODE, SM_TRADELIST_OPCODE}));
	ASSERT_EQ(packets.size(), 2u);
	EXPECT_EQ(packets[0], serializedFor(serverpackets::SM_MESSAGE(0, "", "Quest ID: 0, Dialog Action: BUY (ID: 2)", model::ChatType::GOLDEN_YELLOW)));
}

TEST_F(DialogSelectRunTest, TheSpawnProtectionEndsFirst) {
	Npc& merchant = npcAt(MINALINERK, 120.0f);
	player().setVisualState(model::gameobjects::state::CreatureVisualState::BLINKING);
	ASSERT_TRUE(player().isProtectionActive());

	dialogSelect(selectBody(merchant.getObjectId(), DialogAction::BUY));

	EXPECT_FALSE(player().isProtectionActive()) << ":58-59";
}

TEST_F(DialogSelectRunTest, ReportingAQuestWithoutAnNpcFinishesItThroughTheUnportedQuestService) {
	// :71-104: target 0 (or the player himself) is a quest report; quest 1101 can be reported, and an auto-reward action finishes it:
	// QuestService.finishQuest is M5d's (m5c-plan.md W-12), so it is loud
	EXPECT_THROW(dialogSelect(selectBody(0, DialogAction::SELECTED_QUEST_AUTO_REWARD, SLEEPING_ON_THE_JOB)), runtime::UnportedException);
	EXPECT_THROW(dialogSelect(selectBody(player().getObjectId(), DialogAction::SELECTED_QUEST_AUTO_REWARD15, SLEEPING_ON_THE_JOB)),
		runtime::UnportedException);
	EXPECT_EQ(unportedHitsIn("QuestService.cpp"), 2u);
}

TEST_F(DialogSelectRunTest, AQuestReportWithoutAHandlerOrForAnUnknownQuestDoesNothing) {
	dialogSelect(selectBody(0, DialogAction::SELECT1 + 1, SLEEPING_ON_THE_JOB)); // not an auto reward: QuestEngine.onDialog has no handler
	dialogSelect(selectBody(0, DialogAction::SELECTED_QUEST_AUTO_REWARD, 9999)); // :73-75: no such quest
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "simple 2nd class is off; nothing reaches QuestService or ClassChangeService";
}

TEST_F(DialogSelectRunTest, ClosingTheDialogFinishesTheTalkAndLooksAtTheNpc) {
	Npc& merchant = npcAt(MINALINERK, 103.0f);
	merchant.getController().onDialogRequest(player()); // what CM_SHOW_DIALOG does: TalkEventHandler.onSimpleTalk turns the npc to the player
	ASSERT_TRUE(merchant.isTargeting(player().getObjectId()));
	player().getMailbox()->mailBoxState.set(PlayerMailboxState::REGULAR);
	clearSent();

	closeDialog(merchant.getObjectId());

	// CM_CLOSE_DIALOG.java:33-34: DialogService.onCloseDialog (DIALOG_FINISH -> onFinishTalk clears the npc's target; the mailbox closes), then
	// SM_LOOKATOBJECT(npc).writeImpl: D npc, D its target - 0 by now -, C heading. The talk turned the npc to the player standing 3 m west of
	// it (NpcController.java:89: heading 60 = 180 degrees); clearing the target only schedules the 750 ms think that resets it (:83-87)
	EXPECT_FALSE(merchant.isTargeting(player().getObjectId()));
	EXPECT_EQ(player().getMailbox()->mailBoxState.get(), PlayerMailboxState::CLOSED);
	EXPECT_EQ(sent(), exactly({javaPacket(SM_LOOKATOBJECT_OPCODE, PacketWriter().D(merchant.getObjectId()).D(0).C(60))}));
}

TEST_F(DialogSelectRunTest, ClosingADialogWithAnUnknownTargetDoesNothing) {
	player().getMailbox()->mailBoxState.set(PlayerMailboxState::REGULAR);

	closeDialog(4242);

	EXPECT_TRUE(sent().empty()) << ":31-32";
	EXPECT_EQ(player().getMailbox()->mailBoxState.get(), PlayerMailboxState::REGULAR);
}

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
