// The teleport scrolls (m5f-plan.md X-01, X-03; P5-07 MultiReturnAction, P5-04 ReturnPointEffect, behind P5-08's useTeleportScroll):
// - a multi-return scroll (`<multireturn id>`, MultiReturnAction.java:30-81): CM_USE_ITEM type 6 hands the client's list index to act, which
//   plays the item's cast bar, then takes the scroll and teleports to the list entry's portal scroll (its alias upper-cased);
// - a return scroll (`<skilluse skillid="8198"/>`, whose skill's one effect is `<returnpoint>`, ReturnPointEffect.java:21-48): the item's
//   return_world and return_alias go to TeleportService.useTeleportScroll; a resting player stands up first.
//
// The fixture is TravelTestSupport.h (a connected Elyos in Poeta with a watcher 1 m away) on its DeterministicExecutor. The rows are verbatim:
// items/multi_return_item.xml:3-9 (return_item 1), items/item_templates.xml:832843-832849 (164000085 Sanctum Scroll, return_world/alias) and
// :836365-836370 (164020000 Test_MultiReturnScroll_01, casting_delay 10000), portals/portal_template2.xml:2006-2008, :2012-2014, portal_loc.xml:5
// and :41, skills/skill_templates.xml:79888-79902 (8198 Move); plus one synthetic scroll of casting_delay 0 for the immediate arm.

#include "TravelTestSupport.h"

#include <any>
#include <chrono>
#include <cstdint>
#include <string_view>
#include <vector>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/MultiReturnItemData.bind.h"
#include "aion/gameserver/dataholders/MultiReturnItemData.h"
#include "aion/gameserver/dataholders/Portal2Data.bind.h"
#include "aion/gameserver/dataholders/Portal2Data.h"
#include "aion/gameserver/dataholders/PortalLocData.bind.h"
#include "aion/gameserver/dataholders/PortalLocData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/item/actions/MultiReturnAction.h"
#include "aion/gameserver/network/aion/clientpackets/CM_USE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/skillengine/effect/EffectTemplate.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::teleport::test {
namespace {

using model::templates::item::actions::AbstractItemAction;
using network::test::PacketWriter;

constexpr int32_t MULTI_RETURN_SCROLL = 164020000;
constexpr int32_t QUICK_MULTI_RETURN_SCROLL = 164029999; // synthetic: casting_delay 0
/** synthetic: the Sanctum Scroll with Verteron's return point (the fixture's World has no Sanctum) */
constexpr int32_t VERTERON_SCROLL = 164009999;
constexpr int32_t RETURN_POINT_SKILL = 8198;
/** ServerPacketsOpcodes.java:201 (SM_ITEM_USAGE_ANIMATION), :43 (SM_SYSTEM_MESSAGE), :15 (SM_PLAYER_SPAWN) */
constexpr int32_t SM_ITEM_USAGE_ANIMATION_OPCODE = 183;
constexpr int32_t SM_SYSTEM_MESSAGE_OPCODE = 25;
constexpr int32_t SM_PLAYER_SPAWN_OPCODE = 15;
/** SM_SYSTEM_MESSAGE.java:10762-10763, :10790-10791 */
constexpr int32_t STR_USE_ITEM = 1300423;
constexpr int32_t STR_ITEM_CANCELED = 1300427;

constexpr std::string_view SCROLL_ITEMS_XML = R"xml(<item_templates>
	<item_template id="164000085" name="Sanctum Scroll" level="1" cName="scroll_return_lc1" casting_delay="10000" mask="12414" max_stack_count="1000" quality="COMMON" price="1500" race="ELYOS" restrict="10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10" desc="717713" return_world="110010000" return_alias="LC1_RETURN_AREA_1" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="8198"/>
		</actions>
		<uselimits usedelay="15000" usedelayid="56"/>
	</item_template>
	<item_template id="164009999" name="Verteron Scroll" level="1" cName="scroll_return_lc1" casting_delay="10000" mask="12414" max_stack_count="1000" quality="COMMON" price="1500" race="ELYOS" restrict="10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10" desc="717713" return_world="210030000" return_alias="LF1A_RETURN_AREA_1" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="8198"/>
		</actions>
	</item_template>
	<item_template id="164020000" name="Test_MultiReturnScroll_01" level="1" cName="test_scroll_multi_return_1" casting_delay="10000" mask="12414" max_stack_count="1000" quality="COMMON" price="10000" restrict="10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10" desc="832200" activate_target="STANDALONE" activate_count="1">
		<actions>
			<multireturn id="1"/>
		</actions>
		<uselimits usedelay="15000" usedelayid="56"/>
	</item_template>
	<item_template id="164029999" name="Quick_MultiReturnScroll" level="1" cName="test_scroll_multi_return_1" mask="12414" max_stack_count="1000" quality="COMMON" price="10000" desc="832200" activate_target="STANDALONE" activate_count="1">
		<actions>
			<multireturn id="1"/>
		</actions>
	</item_template>
</item_templates>)xml";

constexpr std::string_view MULTI_RETURN_XML = R"xml(<multi_return_item>
	<return_item id="1">
		<return_loc index="0" worldid="110010000" desc="Sanctum" alias="LC1_Return_Area_1"/>
		<return_loc index="1" worldid="210030000" desc="Verteron" alias="LF1A_Return_Area_1"/>
		<return_loc index="2" worldid="210020000" desc="Eltnen" alias="LF2_Return_Area"/>
		<return_loc index="3" worldid="210040000" desc="Heiron" alias="LF3_Return_Area_1"/>
		<return_loc index="4" worldid="210060000" desc="Aetheric Field Observatory Village" alias="LF2A_Return_Area"/>
	</return_item>
</multi_return_item>)xml";

constexpr std::string_view SCROLL_PORTALS_XML = R"xml(<portal_templates2>
	<portal_scroll name="LC1_RETURN_AREA_1">
		<portal_path loc_id="1100100" race="ELYOS" />
	</portal_scroll>
	<portal_scroll name="LF1A_RETURN_AREA_1">
		<portal_path loc_id="2100300" race="ELYOS" />
	</portal_scroll>
</portal_templates2>)xml";

constexpr std::string_view SCROLL_PORTAL_LOCS_XML = R"xml(<portal_locs>
	<portal_loc world_id="110010000" loc_id="1100100" x="1476.3" y="1595.5" z="572.9"/>
	<portal_loc world_id="210030000" loc_id="2100300" x="1690.4" y="1481.4" z="121.4"/>
</portal_locs>)xml";

constexpr std::string_view RETURN_POINT_SKILL_XML = R"xml(<skill_data>
	<skill_template skill_id="8198" name="Move" nameId="280400" stack="ITEM_RETURNPOINT" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="NONE" activation="ACTIVE" cooldown="0" duration="5000" ground="true" apply_magical_skill_boost_bonus="true" apply_magical_critical="true">
		<properties first_target="ME" />
		<startconditions>
			<weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" />
			<targetflying restriction="GROUND" />
			<selfflying restriction="GROUND" />
		</startconditions>
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
		<effects>
			<returnpoint e="1" />
		</effects>
		<motion name="normalfire" />
	</skill_template>
</skill_data>)xml";

class TeleportScrollTest : public TravelTest {
protected:
	void SetUp() override {
		TravelTest::SetUp();
		if (!prepared)
			return;
		dataholders::DataManager::ITEM_DATA.resetForTests();
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, SCROLL_ITEMS_XML));
		dataholders::DataManager::MULTIRETURN_DATA.publish(xml::bindString<dataholders::MultiReturnItemData>(context, MULTI_RETURN_XML));
		dataholders::DataManager::PORTAL2_DATA.publish(xml::bindString<dataholders::Portal2Data>(context, SCROLL_PORTALS_XML));
		dataholders::DataManager::PORTAL_LOC_DATA.publish(xml::bindString<dataholders::PortalLocData>(context, SCROLL_PORTAL_LOCS_XML));
		dataholders::DataManager::SKILL_DATA.resetForTests();
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(context, RETURN_POINT_SKILL_XML));
	}

	void TearDown() override {
		if (prepared) {
			dataholders::DataManager::MULTIRETURN_DATA.resetForTests();
			dataholders::DataManager::PORTAL2_DATA.resetForTests();
			dataholders::DataManager::PORTAL_LOC_DATA.resetForTests();
		}
		TravelTest::TearDown();
	}

	static void advance(int64_t millis) {
		dynamic_cast<runtime::DeterministicExecutor&>(*utils::ThreadPoolManager::installedBackend()).advance(std::chrono::milliseconds(millis));
	}

	/** `count` of the scroll `itemId` in the actor's cube, loaded as the DAO does */
	runtime::Ref<model::gameobjects::Item> giveScroll(int32_t itemId, int64_t count) {
		runtime::Ref<model::gameobjects::Item> item = model::gameobjects::Item::create(420150, itemId, count, std::nullopt, 0, "", 0, 0, false, false, 0,
			model::items::storage::getId(model::items::storage::StorageType::CUBE), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, 0);
		player().getInventory().onLoadHandler(*item);
		items.push_back(item);
		return item;
	}

	/** The item's <multireturn> action */
	static const AbstractItemAction& multiReturn(int32_t itemId) {
		const model::templates::item::ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
		for (const std::unique_ptr<AbstractItemAction>& action : itemTemplate->getActions()->getItemActions())
			if (dynamic_cast<const model::templates::item::actions::MultiReturnAction*>(action.get()))
				return *action;
		throw std::runtime_error("no multireturn action");
	}

	/** SM_ITEM_USAGE_ANIMATION.writeImpl of the 6-argument constructor: D player, D player (target), D itemObj, D itemId, D time, C end, C unk 0, C unk1 0, C unk2 1 (its initializer, SM_ITEM_USAGE_ANIMATION.java:19), D unk3 */
	std::vector<uint8_t> usageAnimation(const model::gameobjects::Item& item, int32_t time, int32_t end) {
		return javaPacket(SM_ITEM_USAGE_ANIMATION_OPCODE, PacketWriter()
			.D(player().getObjectId()).D(player().getObjectId()).D(item.getObjectId()).D(item.getItemTemplate()->getTemplateId()).D(time).C(end).C(0).C(0).C(1).D(0));
	}

	static std::vector<uint8_t> systemMessage(int32_t id, std::initializer_list<std::string_view> params = {}) {
		PacketWriter body;
		body.C(25).C(0).D(0).D(id).C(static_cast<int32_t>(params.size()));
		for (std::string_view param : params)
			body.S(param);
		body.C(0);
		return javaPacket(SM_SYSTEM_MESSAGE_OPCODE, body);
	}

	/** Java ItemTemplate.getL10n() of the template's desc id: "$" + two UTF-16 units of `desc << 1 | 1` (ChatUtil.l10n) - read off the item */
	static std::string l10n(model::gameobjects::Item& item) { return item.getL10n(); }
};

// ---- MultiReturnAction (MultiReturnAction.java:30-81) ---------------------------------------------------------------------------------------

TEST_F(TeleportScrollTest, AMultiReturnScrollCanActForAnEntryOfItsList) {
	spawnActor(0);
	runtime::Ref<model::gameobjects::Item> scroll = giveScroll(MULTI_RETURN_SCROLL, 2);
	EXPECT_TRUE(multiReturn(MULTI_RETURN_SCROLL).canAct(player(), scroll, nullptr, {std::any(int32_t{0})}));
	EXPECT_TRUE(multiReturn(MULTI_RETURN_SCROLL).canAct(player(), scroll, nullptr, {std::any(int32_t{4})})) << "the last of the five entries";
}

// the owner's correction of 2026-10-05 (docs/deviations/P5-07.md, "MultiReturnAction: correction of the Java code"): Java answered true and
// let the task throw
TEST_F(TeleportScrollTest, AnIndexOutsideTheListCannotAct) {
	spawnActor(0);
	runtime::Ref<model::gameobjects::Item> scroll = giveScroll(MULTI_RETURN_SCROLL, 2);
	for (int32_t index : {5, -1, 1000})
		EXPECT_FALSE(multiReturn(MULTI_RETURN_SCROLL).canAct(player(), scroll, nullptr, {std::any(index)})) << index;
}

TEST_F(TeleportScrollTest, TheCastBarEndsInTheChosenEntrysWorldWithTheAliasUpperCased) {
	spawnActor(0);
	runtime::Ref<model::gameobjects::Item> scroll = giveScroll(MULTI_RETURN_SCROLL, 2);
	clearSent();
	multiReturn(MULTI_RETURN_SCROLL).act(player(), scroll, nullptr, {std::any(int32_t{1})});
	EXPECT_EQ(sent(), cptest::exactly({usageAnimation(*scroll, 10000, 0)})) << "the bar, broadcast to himself";
	EXPECT_EQ(watcherSent(), cptest::exactly({usageAnimation(*scroll, 10000, 0)})) << "and to the ones around";
	EXPECT_TRUE(player().getController().hasTask(model::TaskId::ITEM_USE));

	clearSent();
	advance(9999);
	EXPECT_EQ(player().getWorldId(), POETA) << "nothing before casting_delay";
	EXPECT_EQ(scroll->getItemCount(), 2);
	advance(1);
	// "LF1A_Return_Area_1".toUpperCase() is portal_template2.xml's LF1A_RETURN_AREA_1 -> loc 2100300 in the entry's world, Verteron
	EXPECT_EQ(player().getWorldId(), VERTERON);
	EXPECT_FLOAT_EQ(player().getX(), 1690.4f);
	EXPECT_FLOAT_EQ(player().getY(), 1481.4f);
	EXPECT_FLOAT_EQ(player().getZ(), 121.4f);
	EXPECT_EQ(scroll->getItemCount(), 1) << "decreaseByObjectId(item, 1)";
	EXPECT_EQ(ofOpcode(sent(), SM_PLAYER_SPAWN_OPCODE).size(), 1u) << "a map change: SpawnTask's cross-map arm";
	const std::vector<std::vector<uint8_t>> messages = ofOpcode(sent(), SM_SYSTEM_MESSAGE_OPCODE);
	ASSERT_FALSE(messages.empty());
	EXPECT_EQ(messages.back(), systemMessage(STR_USE_ITEM, {l10n(*scroll)})) << "STR_USE_ITEM after the teleport";
}

TEST_F(TeleportScrollTest, MovingDuringTheBarCancelsItAndKeepsTheScroll) {
	spawnActor(0);
	runtime::Ref<model::gameobjects::Item> scroll = giveScroll(MULTI_RETURN_SCROLL, 2);
	multiReturn(MULTI_RETURN_SCROLL).act(player(), scroll, nullptr, {std::any(int32_t{1})});
	clearSent();
	player().getController().onMove(); // the ItemUseObserver's moved() -> abort()
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::ITEM_USE)) << "abort cancels ITEM_USE";
	EXPECT_EQ(sent(), cptest::exactly({systemMessage(STR_ITEM_CANCELED), usageAnimation(*scroll, 0, 2)}));
	advance(10000);
	EXPECT_EQ(player().getWorldId(), POETA);
	EXPECT_EQ(scroll->getItemCount(), 2);
}

TEST_F(TeleportScrollTest, WithoutACastingDelayTheTeleportIsImmediate) {
	spawnActor(0);
	runtime::Ref<model::gameobjects::Item> scroll = giveScroll(QUICK_MULTI_RETURN_SCROLL, 1);
	multiReturn(QUICK_MULTI_RETURN_SCROLL).act(player(), scroll, nullptr, {std::any(int32_t{1})});
	EXPECT_EQ(player().getWorldId(), VERTERON) << "index 1: Verteron's LF1A_RETURN_AREA_1, loc 2100300";
	EXPECT_FLOAT_EQ(player().getX(), 1690.4f);
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::ITEM_USE));
	EXPECT_EQ(scroll->getItemCount(), 0);
}

/** The owner's correction of 2026-10-05: a client index outside the list is refused by canAct, CM_USE_ITEM drops the use, the scroll stays */
TEST_F(TeleportScrollTest, CmUseItemWithAnIndexOutsideTheListKeepsTheScroll) {
	spawnActor(0);
	runtime::Ref<model::gameobjects::Item> scroll = giveScroll(QUICK_MULTI_RETURN_SCROLL, 1);
	clearSent();
	cptest::Driver<network::aion::clientpackets::CM_USE_ITEM> packet(37); // AionClientPacketFactory packets[37]
	EXPECT_NO_THROW(packet.readAndRun(PacketWriter().D(scroll->getObjectId()).C(6).D(5).data, actorClient->get()))
		<< "Java: List.get(5) of the five entries of return_item 1 threw (MultiReturnAction.java:70)";
	EXPECT_EQ(player().getWorldId(), POETA);
	EXPECT_EQ(scroll->getItemCount(), 1);
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::ITEM_USE)) << "no cast bar";
	EXPECT_TRUE(watcherSent().empty()) << "no SM_ITEM_USAGE_ANIMATION";
}

/**
 * The whole client path: CM_USE_ITEM type 6 (CM_USE_ITEM.java:47-49, readD indexReturn) passes the index to MultiReturnAction's canAct and act
 * (:104, :120). The synthetic scroll has no casting delay and no level restriction, so the teleport happens inside runImpl.
 */
TEST_F(TeleportScrollTest, CmUseItemHandsTheClientsIndexToTheScroll) {
	spawnActor(0);
	runtime::Ref<model::gameobjects::Item> scroll = giveScroll(QUICK_MULTI_RETURN_SCROLL, 3);
	cptest::Driver<network::aion::clientpackets::CM_USE_ITEM> packet(37); // AionClientPacketFactory packets[37]
	packet.readAndRun(PacketWriter().D(scroll->getObjectId()).C(6).D(1).data, actorClient->get());
	EXPECT_EQ(player().getWorldId(), VERTERON) << "index 1 of return_item 1";
	EXPECT_EQ(scroll->getItemCount(), 2);
}

// ---- ReturnPointEffect (ReturnPointEffect.java:21-48) --------------------------------------------------------------------------------------

TEST_F(TeleportScrollTest, AReturnScrollsEffectTeleportsToTheItemsReturnPoint) {
	spawnActor(0);
	const model::templates::item::ItemTemplate* sanctumScroll = dataholders::DataManager::ITEM_DATA->getItemTemplate(VERTERON_SCROLL);
	runtime::Ref<skillengine::model::Skill> skill = skillengine::model::Skill::create(
		dataholders::DataManager::SKILL_DATA->getSkillTemplate(RETURN_POINT_SKILL), player(), 1, runtime::Ptr<model::gameobjects::Creature>(player()),
		sanctumScroll);
	runtime::Ref<skillengine::model::Effect> effect = skillengine::model::Effect::create(*skill, runtime::Ptr<model::gameobjects::Creature>(player()));
	const skillengine::effect::EffectTemplate& returnPoint = *effect->getEffectTemplates()[0];
	returnPoint.calculate(*effect);
	ASSERT_TRUE(effect->isInSuccessEffects(1)) << "calculate: an effect with an item template succeeds";
	returnPoint.applyEffect(*effect);
	EXPECT_EQ(player().getWorldId(), VERTERON) << "return_world 210030000, return_alias LF1A_RETURN_AREA_1 -> loc 2100300";
	EXPECT_FLOAT_EQ(player().getX(), 1690.4f);
	EXPECT_FLOAT_EQ(player().getY(), 1481.4f);
	EXPECT_FLOAT_EQ(player().getZ(), 121.4f);
}

TEST_F(TeleportScrollTest, AReturnPointWithoutAnItemDoesNotSucceed) {
	spawnActor(0);
	runtime::Ref<skillengine::model::Effect> effect = skillengine::model::Effect::create(player(), runtime::Ptr<model::gameobjects::Creature>(player()),
		dataholders::DataManager::SKILL_DATA->getSkillTemplate(RETURN_POINT_SKILL), 1);
	effect->getEffectTemplates()[0]->calculate(*effect);
	EXPECT_FALSE(effect->isInSuccessEffects(1)) << "calculate: effect.getItemTemplate() is null";
}

TEST_F(TeleportScrollTest, AResterStandsUpBeforeTheReturn) {
	spawnActor(0);
	player().setState(model::gameobjects::state::CreatureState::RESTING);
	player().setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*watcher.player));
	const model::templates::item::ItemTemplate* sanctumScroll = dataholders::DataManager::ITEM_DATA->getItemTemplate(VERTERON_SCROLL);
	runtime::Ref<skillengine::model::Skill> skill = skillengine::model::Skill::create(
		dataholders::DataManager::SKILL_DATA->getSkillTemplate(RETURN_POINT_SKILL), player(), 1, runtime::Ptr<model::gameobjects::Creature>(player()),
		sanctumScroll);
	runtime::Ref<skillengine::model::Effect> effect = skillengine::model::Effect::create(*skill, runtime::Ptr<model::gameobjects::Creature>(player()));
	// the STAND emotion as the server serializes it once RESTING is unset (SM_EMOTION writes the state), with the target's id
	player().unsetState(model::gameobjects::state::CreatureState::RESTING);
	const std::vector<uint8_t> stand = forActor(network::aion::serverpackets::SM_EMOTION(player(), model::EmotionType::STAND, 0, player().getX(),
		player().getY(), player().getZ(), player().getHeading(), watcher.player->getObjectId()));
	player().setState(model::gameobjects::state::CreatureState::RESTING);
	clearSent();
	effect->getEffectTemplates()[0]->applyEffect(*effect);
	EXPECT_FALSE(player().isInState(model::gameobjects::state::CreatureState::RESTING)) << "unsetState(RESTING)";
	ASSERT_FALSE(watcherSent().empty());
	EXPECT_EQ(watcherSent().front(), stand) << "SM_EMOTION(STAND, ..., target) broadcast before the teleport";
	EXPECT_EQ(player().getWorldId(), VERTERON);
	player().setTarget(nullptr);
}

} // namespace
} // namespace aion::gameserver::services::teleport::test
