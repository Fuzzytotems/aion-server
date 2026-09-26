// M5c D-03 / D-04 (m5c-plan.md §5, P5-16): CM_SHOW_DIALOG (C_START_DIALOG), which the 2026-09-24 real-client session sent unported
// (m5b3-client-session.md S-2), and CM_QUESTION_RESPONSE (C_ANSWER), without which the soul-bind question M5b-3 asks could not be answered
// (m5c-plan.md W-11).
//
// Java: CM_SHOW_DIALOG.java:23-42, CM_QUESTION_RESPONSE.java:27-44. The read cases lay each body out field by field from the Java readImpl; the
// run cases drive runImpl against the item packet fixture (tests/cm_ak/ItemPacketTestSupport.h: a spawned warrior in Poeta and a real
// AionConnection whose send queue the cases read).
// - CM_SHOW_DIALOG talks to the merchant 798007 minalinerk (npc_templates.xml:461604-461610, verbatim), spawned beside the player and made known
//   to him. The npc's row names ai="general", whose handler lives in the handler library this executable does not link; the npc gets a leaf
//   AI whose dialog hooks are GeneralNpcAI's (GeneralNpcAI.java:49-57: TalkEventHandler.onTalk / onFinishTalk), so the dialog start runs the
//   real TalkEventHandler, QuestEngine.onDialog, DialogPage.getStartPageId and DialogService.isInteractionAllowed.
// - CM_QUESTION_RESPONSE answers the soul-bind question of Equipment.soulBindItem (the Manastone Slot Test Superior Sword,
//   item_templates.xml:2062, as tests/player/SoulBindTest.cpp asks it).
//
// NOT COVERED, and named: the `isTrading()` bail-outs of both packets and CM_QUESTION_RESPONSE's cancel-on-yes (nothing can put a player into
// an exchange before ExchangeService.registerExchange is ported, M5c T-02; m5c-plan.md W-28 gives that test to T-04), and CM_SHOW_DIALOG's
// removeHideEffects arm (no hide effect can be applied without a skill of M5b-2's subset that hides; the call itself is ported).

#include "../cm_ak/ItemPacketTestSupport.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/handler/TalkEventHandler.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/clientpackets/CM_QUESTION_RESPONSE.h"
#include "aion/gameserver/network/aion/clientpackets/CM_SHOW_DIALOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

std::unique_ptr<AionClientPacket> CM_SHOW_DIALOG_clientPacketFactory(int32_t opcode, const StateSet& validStates);
std::unique_ptr<AionClientPacket> CM_QUESTION_RESPONSE_clientPacketFactory(int32_t opcode, const StateSet& validStates);

/** The friends the two headers declare: the fields readImpl decoded, which Java keeps private */
struct CM_SHOW_DIALOGTestAccess {
	static int32_t targetObjectId(const CM_SHOW_DIALOG& p) { return p.targetObjectId; }
};

struct CM_QUESTION_RESPONSETestAccess {
	static int32_t questionid(const CM_QUESTION_RESPONSE& p) { return p.questionid; }
	static int32_t response(const CM_QUESTION_RESPONSE& p) { return p.response; }
	static int32_t senderid(const CM_QUESTION_RESPONSE& p) { return p.senderid; }
};

namespace testing::items {
namespace {

using model::gameobjects::Npc;
using network::test::LogCapture;
using serverpackets::SM_QUESTION_WINDOW;
using serverpackets::SM_SYSTEM_MESSAGE;

const char* BASE_CLIENT_PACKET_LOGGER = "com.aionemu.commons.network.packet.BaseClientPacket";

/** the decoded opcodes of ClientPacketInfo.gen.inc (Java AionClientPacketFactory.java:78, :80, State.IN_GAME) */
constexpr int32_t CM_QUESTION_RESPONSE_OPCODE = 50;
constexpr int32_t CM_SHOW_DIALOG_OPCODE = 52;

// ServerPacketsOpcodes.java:78
constexpr int32_t SM_DIALOG_WINDOW_OPCODE = 60;

constexpr int32_t MINALINERK = 798007;
constexpr int64_t MAIN_HAND = 1; // ItemSlot.MAIN_HAND.getSlotIdMask()
constexpr int32_t SWORD = 792001;

/** npc_templates.xml:461604-461610, verbatim: the Poeta merchant minalinerk (BUY and SELL, talk distance 5, can_talk_invisible false) */
constexpr std::string_view MINALINERK_XML =
	R"(<npc_template npc_id="798007" level="9" name="minalinerk" name_id="351126" height="1.16875" title_id="350377" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2100" hpgauge="3">
		<stats maxHp="2568">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
		<talk_info distance="5" is_dialog="true" func_dialogs="2 3" can_talk_invisible="false" />
	</npc_template>)";

const model::templates::npc::NpcTemplate* minalinerkTemplate() {
	static const dataholders::NpcData* holder = [] {
		static xml::LoadContext context;
		return xml::bindString<dataholders::NpcData>(context, "<npc_templates>" + std::string(MINALINERK_XML) + "</npc_templates>").release();
	}();
	return holder->getNpcTemplate(MINALINERK);
}

/** C_START_DIALOG: D target object id (CM_SHOW_DIALOG.java:23-25) */
std::vector<uint8_t> showDialogBody(int32_t targetObjectId) {
	return PacketWriter().D(targetObjectId).data;
}

/** C_ANSWER: D question id, C response, C unk, H unk, D sender id, D unk, H unk (CM_QUESTION_RESPONSE.java:27-36) */
std::vector<uint8_t> answerBody(int32_t questionId, int32_t response, int32_t senderId = 0) {
	return PacketWriter().D(questionId).C(response).C(1).H(0x2222).D(senderId).D(0x33333333).H(0x4444).data;
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

/** How many entries of the generated table name `className`, each checked against `opcode` and IN_GAME */
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

TEST(DialogPacketsReadTest, ShowDialogReadsTheTarget) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	int32_t unread = -1;
	std::unique_ptr<CM_SHOW_DIALOG> p = readPacket<CM_SHOW_DIALOG>(showDialogBody(0x01020304), CM_SHOW_DIALOG_OPCODE, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(CM_SHOW_DIALOGTestAccess::targetObjectId(*p), 0x01020304);
	EXPECT_EQ(unread, 0);
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(DialogPacketsReadTest, QuestionResponseReadsTheQuestionTheUnsignedAnswerAndTheSender) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	int32_t unread = -1;
	std::unique_ptr<CM_QUESTION_RESPONSE> p =
		readPacket<CM_QUESTION_RESPONSE>(answerBody(0x0A0B0C0D, 0xFE, 0x11223344), CM_QUESTION_RESPONSE_OPCODE, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(CM_QUESTION_RESPONSETestAccess::questionid(*p), 0x0A0B0C0D);
	EXPECT_EQ(CM_QUESTION_RESPONSETestAccess::response(*p), 0xFE) << "readUC: the answer byte is unsigned";
	EXPECT_EQ(CM_QUESTION_RESPONSETestAccess::senderid(*p), 0x11223344) << "after a skipped byte and a skipped short";
	EXPECT_EQ(unread, 0) << "the trailing D and H are read and dropped";
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(DialogPacketsReadTest, TheMarkersRegisterTheClassesUnderTheirJavaOpcodes) {
	EXPECT_NE(dynamic_cast<CM_SHOW_DIALOG*>(CM_SHOW_DIALOG_clientPacketFactory(CM_SHOW_DIALOG_OPCODE, StateSet{AionConnection_State::IN_GAME}).get()),
		nullptr);
	EXPECT_NE(dynamic_cast<CM_QUESTION_RESPONSE*>(
				  CM_QUESTION_RESPONSE_clientPacketFactory(CM_QUESTION_RESPONSE_OPCODE, StateSet{AionConnection_State::IN_GAME}).get()),
		nullptr);
	EXPECT_EQ(checkTableEntry("CM_SHOW_DIALOG", CM_SHOW_DIALOG_OPCODE), 1);
	EXPECT_EQ(checkTableEntry("CM_QUESTION_RESPONSE", CM_QUESTION_RESPONSE_OPCODE), 1);
}

/** GeneralNpcAI's dialog hooks (GeneralNpcAI.java:49-57) on the plain NpcAI of this executable (the handler library is not linked here) */
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

class DialogPacketsRunTest : public ItemPacketTest {
protected:
	void SetUp() override {
		ItemPacketTest::SetUp();
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
	}

	void TearDown() override {
		if (f.player) {
			f.player->getController().cancelTask(model::TaskId::ITEM_USE);
			f.player->setTarget(nullptr);
		}
		npcs.clear(); // before the map instance their positions name
		spawnGroups.clear();
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
		ItemPacketTest::TearDown();
	}

	/** minalinerk spawned `x` on the x axis at (x, 100, 50) with the talking AI in its spawned state, and known to the player */
	Npc& merchantAt(float x) {
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, MINALINERK, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<DialogSpawnTemplate>(*group, x, 100.0f, 50.0f));
		runtime::Ref<Npc> npc =
			model::gameobjects::VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), spawn, minalinerkTemplate());
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

	void showDialog(int32_t targetObjectId) {
		Driver<CM_SHOW_DIALOG> packet(CM_SHOW_DIALOG_OPCODE);
		packet.readAndRun(showDialogBody(targetObjectId), client->get());
	}

	void answer(int32_t questionId, int32_t response) {
		Driver<CM_QUESTION_RESPONSE> packet(CM_QUESTION_RESPONSE_OPCODE);
		packet.readAndRun(answerBody(questionId, response), client->get());
	}

	/** SM_DIALOG_WINDOW.writeImpl (SM_DIALOG_WINDOW.java:29-41) of a page that is neither MAIL nor TOWN_CHALLENGE_TASK */
	static std::vector<uint8_t> dialogWindow(int32_t npcObjectId, int32_t page) {
		return javaPacket(SM_DIALOG_WINDOW_OPCODE, PacketWriter().D(npcObjectId).H(page).D(0).H(0).H(0));
	}

	std::shared_ptr<const std::string> savedMissingAiHandlers;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<runtime::Ref<Npc>> npcs;
};

TEST_F(DialogPacketsRunTest, ShowDialogAtAMerchantInTalkRangeOpensItsFunctionPage) {
	Npc& merchant = merchantAt(103.0f); // 3 m: inside talk distance 5 + 1 (PositionUtil.isInTalkRange)

	showDialog(merchant.getObjectId());

	// CM_SHOW_DIALOG.java:36-40 -> NpcController.onDialogRequest -> DIALOG_START -> TalkEventHandler.onTalk: no quest takes it
	// (QuestEngine.onDialog), so SM_DIALOG_WINDOW with DialogPage.getStartPageId: talk_info with func_dialogs, isInteractionAllowed (no summon
	// owner, no subdialog type) -> HTML_PAGE_SELECT_QUEST 10
	EXPECT_EQ(sent(), exactly({dialogWindow(merchant.getObjectId(), 10)}));
	// TalkEventHandler.onSimpleTalk: an is_dialog npc turns to the player
	EXPECT_TRUE(merchant.isTargeting(player().getObjectId()));
}

TEST_F(DialogPacketsRunTest, ShowDialogOutOfTalkRangeSaysTheNpcIsTooFar) {
	Npc& merchant = merchantAt(120.0f); // 20 m

	showDialog(merchant.getObjectId());

	// NpcController.java:252-256: an is_dialog npc answers STR_DIALOG_TOO_FAR_TO_TALK, and the AI is never asked
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_DIALOG_TOO_FAR_TO_TALK())}));
	EXPECT_FALSE(merchant.isTargeting(player().getObjectId()));
}

TEST_F(DialogPacketsRunTest, ShowDialogWithAnUnknownOrNonNpcTargetDoesNothing) {
	Npc& merchant = merchantAt(103.0f);

	showDialog(merchant.getObjectId() + 1000); // not in the known list: getObject answers null, and `null instanceof Npc` is false
	showDialog(player().getObjectId());       // the player himself is no known object of his own list
	showDialog(0);

	EXPECT_TRUE(sent().empty());
	EXPECT_FALSE(merchant.isTargeting(player().getObjectId()));
}

TEST_F(DialogPacketsRunTest, ShowDialogEndsTheSpawnProtectionFirst) {
	Npc& merchant = merchantAt(120.0f); // out of range: the protection ends whatever the npc answers
	player().setVisualState(model::gameobjects::state::CreatureVisualState::BLINKING);
	ASSERT_TRUE(player().isProtectionActive());

	showDialog(merchant.getObjectId());

	// CM_SHOW_DIALOG.java:30-31: PlayerController.stopProtectionActiveTask clears BLINKING (and updates the player's visual state)
	EXPECT_FALSE(player().isProtectionActive());
	EXPECT_EQ(packetsOf(sent(), SM_SYSTEM_MESSAGE_OPCODE).back(), serializedFor(SM_SYSTEM_MESSAGE::STR_DIALOG_TOO_FAR_TO_TALK()));
}

TEST_F(DialogPacketsRunTest, AnsweringYesToTheSoulBindQuestionStartsTheBinding) {
	stored(SWORD, SOUL_BOUND_TEST_SWORD, 1);
	// Equipment.equipItem of an unbound soul-bound item asks first (Equipment.java:138-141, :772-779; tests/player/SoulBindTest.cpp)
	EXPECT_FALSE(player().getEquipment().equipItem(SWORD, MAIN_HAND));
	ASSERT_EQ(opcodesOf(sent()), (std::vector<int32_t>{SM_QUESTION_WINDOW_OPCODE}));
	clearSent();

	answer(SM_QUESTION_WINDOW::STR_SOUL_BOUND_ITEM_DO_YOU_WANT_SOUL_BOUND, 1);

	// CM_QUESTION_RESPONSE.java:43 -> ResponseRequester.respond -> Equipment$1.acceptRequest: the 5 s item use starts (m5c-plan.md W-11)
	EXPECT_EQ(opcodesOf(sent()), (std::vector<int32_t>{SM_ITEM_USAGE_ANIMATION_OPCODE}));
	EXPECT_TRUE(player().getController().hasScheduledTask(model::TaskId::ITEM_USE));

	// the request was removed by the answer: a second "yes" finds nothing to answer
	clearSent();
	answer(SM_QUESTION_WINDOW::STR_SOUL_BOUND_ITEM_DO_YOU_WANT_SOUL_BOUND, 1);
	EXPECT_TRUE(sent().empty());
}

TEST_F(DialogPacketsRunTest, AnsweringNoToTheSoulBindQuestionCancelsIt) {
	Item& sword = stored(SWORD, SOUL_BOUND_TEST_SWORD, 1);
	EXPECT_FALSE(player().getEquipment().equipItem(SWORD, MAIN_HAND));
	clearSent();

	answer(SM_QUESTION_WINDOW::STR_SOUL_BOUND_ITEM_DO_YOU_WANT_SOUL_BOUND, 0);

	// RequestResponseHandler.handle: response 0 -> denyRequest (Equipment.java:766-769)
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_SOUL_BOUND_ITEM_CANCELED(sword.getL10n()))}));
	EXPECT_FALSE(player().getController().hasScheduledTask(model::TaskId::ITEM_USE));
	// and the question is gone: equipping asks again instead of "close the other message box first"
	clearSent();
	EXPECT_FALSE(player().getEquipment().equipItem(SWORD, MAIN_HAND));
	EXPECT_EQ(opcodesOf(sent()), (std::vector<int32_t>{SM_QUESTION_WINDOW_OPCODE}));
}

TEST_F(DialogPacketsRunTest, AnyNonZeroAnswerIsAYes) {
	stored(SWORD, SOUL_BOUND_TEST_SWORD, 1);
	EXPECT_FALSE(player().getEquipment().equipItem(SWORD, MAIN_HAND));
	clearSent();

	answer(SM_QUESTION_WINDOW::STR_SOUL_BOUND_ITEM_DO_YOU_WANT_SOUL_BOUND, 0xFF); // readUC: 255, not -1

	EXPECT_EQ(opcodesOf(sent()), (std::vector<int32_t>{SM_ITEM_USAGE_ANIMATION_OPCODE}));
}

TEST_F(DialogPacketsRunTest, AnAnswerToAQuestionNobodyAskedDoesNothing) {
	answer(SM_QUESTION_WINDOW::STR_SOUL_BOUND_ITEM_DO_YOU_WANT_SOUL_BOUND, 1);
	answer(SM_QUESTION_WINDOW::STR_ASK_RECOVER_EXPERIENCE, 0);

	EXPECT_TRUE(sent().empty());
}

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
