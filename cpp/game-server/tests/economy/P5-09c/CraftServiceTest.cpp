// M5c C-01 / C-06 (m5c-plan.md §5, P5-09c): CraftService - startCrafting with checkCraft's refusals in their order and sendCancelCraft's
// packets, the (StaticObject) cast of the target, the materials of the sent alternative, getBonusReqItem's stones; finishCrafting's xp and
// level-up arithmetic, the product, the creator name, the craft log line, the craft cooldown and the limited recipe. The fixture and its rules
// are CraftTestSupport.h's.
//
// Java: CraftService.java:43-258. Expectations: `oracle.py m5c-craft --no-profile --set gameserver.event.service.disabled_events=* --recipe ID
// [--skill-level N --skill-xp X --craft-type T --set KEY=VALUE]` (its craft.checks, craft.materials, craft.timing, skillUp and afterCraft
// blocks) where named, else the Java lines beside each assertion.

#include "CraftTestSupport.h"

#include <any>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/CraftConfig.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Cooldowns.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatAddFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/ride/RideInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_ADD_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEARN_RECIPE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RECIPE_DELETE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/AbstractQuestHandler.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemAddType.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"

namespace aion::gameserver::economy::test::craft {
namespace {

using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using services::craft::CraftService;

/**
 * A stand-in for a crafting quest's handler (data/handlers/quest/crafting, M5d's): it registers QuestEngine.onFailCraft for its items, as
 * _19038MasterCooksPotential.register() does (_19038MasterCooksPotential.java:26-29), and records each onFailCraftEvent as (the env's quest id,
 * the item id). Quest handlers are Immortal (the engine never frees them), so the calls go to a static list.
 */
class FailCraftProbe final : public questEngine::handlers::AbstractQuestHandler {
public:
	static inline std::vector<std::pair<int32_t, int32_t>> calls;

	FailCraftProbe(int32_t questIdValue, std::vector<int32_t> itemIdsValue) : AbstractQuestHandler(questIdValue), itemIds(std::move(itemIdsValue)) {}

	void register_() override {
		for (int32_t itemId : itemIds)
			qe.registerOnFailCraft(itemId, getQuestId());
	}

	bool onFailCraftEvent(questEngine::model::QuestEnv& env, int32_t itemId) override {
		calls.emplace_back(env.getQuestId(), itemId);
		return true;
	}

private:
	const std::vector<int32_t> itemIds;
};

/** quest_data.xml:47763, verbatim: the quest of _19038MasterCooksPotential */
constexpr std::string_view QUEST_19038_XML = R"xml(<quests>
	<quest id="19038" name="[Master] Cook's Potential" nameId="1124538" quest_zone="Sanctum" minlevel_permitted="29" max_repeat_count="1" cannot_share="true" race_permitted="ELYOS" combineskill="40001" combine_skillpoint="499" category="SIGNIFICANT">
		<collect_items>
			<collect_item item_id="182206773" count="1"/>
			<collect_item item_id="182206774" count="1"/>
			<collect_item item_id="182206775" count="1"/>
			<collect_item item_id="182206776" count="1"/>
		</collect_items>
		<rewards exp="291412"/>
		<start_conditions>
			<finished quest_id="1944"/>
			<unfinished>19008</unfinished>
			<noacquired>19008</noacquired>
		</start_conditions>
		<start_conditions>
			<finished quest_id="3952"/>
			<unfinished>19014</unfinished>
			<noacquired>19014</noacquired>
		</start_conditions>
		<start_conditions>
			<unfinished>19020</unfinished>
			<noacquired>19020</noacquired>
		</start_conditions>
		<start_conditions>
			<unfinished>19026</unfinished>
			<noacquired>19026</noacquired>
		</start_conditions>
		<start_conditions>
			<unfinished>19032</unfinished>
			<noacquired>19032</noacquired>
		</start_conditions>
		<quest_work_items>
			<quest_work_item item_id="152202200"/>
			<quest_work_item item_id="152202201"/>
			<quest_work_item item_id="152202202"/>
			<quest_work_item item_id="152202203"/>
		</quest_work_items>
	</quest>
</quests>)xml";

/**
 * The quest engine of FailCraftProbe's case for its scope: QUEST_DATA with the quest's row (AbstractQuestHandler's constructor reads its work
 * items), and at the end the engine cleared of the probe - QuestEngine.clear() cancels the daily message job through CronService, which is
 * started for the scope (QuestEngineTest.cpp and QuestDropTest.cpp do the same)
 */
class FailCraftEngine {
public:
	FailCraftEngine() : quests(dataholders::DataManager::QUEST_DATA, bindXml<dataholders::QuestsData>(QUEST_19038_XML)) {
		FailCraftProbe::calls.clear();
		services::cron::CronService::resetForTests();
		services::cron::CronService::initSingleton(std::make_unique<services::cron::CurrentThreadRunnableRunner>(), std::chrono::locate_zone("UTC"),
			services::cron::CronService::Driver::EXECUTOR);
	}
	~FailCraftEngine() {
		questEngine::QuestEngine::getInstance().clear();
		services::cron::CronService::resetForTests();
		FailCraftProbe::calls.clear();
	}
	FailCraftEngine(const FailCraftEngine&) = delete;
	FailCraftEngine& operator=(const FailCraftEngine&) = delete;

private:
	PublishedHolder<dataholders::QuestsData> quests;
};

class CraftServiceTest : public CraftTest {
protected:
	void ride() {
		static const model::templates::ride::RideInfo RIDE{};
		player().setPlayerMode(model::actions::PlayerMode::RIDE, std::any(static_cast<const model::templates::ride::RideInfo*>(&RIDE)));
	}

	void dismount() { player().setPlayerMode(model::actions::PlayerMode::RIDE, std::any()); }

	/** A class past the starting class (PlayerCommonData.setDp returns at once for a starting class) with `dp` divine points (offline: no packet) */
	void setDaevaDp(int32_t dp) {
		commonData().setPlayerClass(model::PlayerClass::GLADIATOR);
		commonData().setDp(dp);
		ASSERT_EQ(commonData().getDp(), dp);
	}

	/** Adds `value` to one stat of the player as a bonus, through a stat function without an owner (EffectClassTestSupport.h's addStat) */
	void addStat(model::stats::container::StatEnum stat, int32_t value) {
		namespace functions = model::stats::calc::functions;
		player().getGameStats()->addEffect(nullptr,
			{runtime::Ptr<functions::IStatFunction>(functions::RcStatFunction<functions::StatAddFunction>::create(stat, value, true))});
	}

	void finish(int32_t recipeId, int32_t critCount = 0, int32_t bonus = 0) {
		CraftService::finishCrafting(player(), dataholders::DataManager::RECIPE_DATA->getRecipeTemplateById(recipeId), critCount, bonus);
	}

	/** The one item of the template in the cube */
	model::gameobjects::Item& onlyItem(int32_t itemId) {
		std::vector<runtime::Ptr<model::gameobjects::Item>> found = player().getInventory().getItemsByItemId(itemId);
		if (found.size() != 1)
			throw runtime::IllegalStateException(std::to_string(found.size()) + " items of " + std::to_string(itemId));
		return *found[0];
	}

	bool sentContains(const std::vector<uint8_t>& packet) {
		for (const std::vector<uint8_t>& each : sent())
			if (each == packet)
				return true;
		return false;
	}
};

// ---------------------------------------------------------------------------------------------------------------------------------------------
// startCrafting and checkCraft (CraftService.java:97-233)
// ---------------------------------------------------------------------------------------------------------------------------------------------

// checkCraft's refusals in their order (CraftService.java:149-220; oracle craft.checks): every step removes the one failure the step before
// reported and keeps all later ones, so each refusal is the first check that fails. Each ends with sendCancelCraft's pair (:104-105, :235-238)
// and changes nothing (the materials are taken only after the last check, :222-230). The in-progress check (:145-147) needs a CraftingTask,
// whose constructor is C-02's: TheCraftIntervalFollowsTheLevelDifferenceTheQualityAndTheMorph covers it once C-02 merged.
TEST_F(CraftServiceTest, CheckCraftRefusesInJavasOrderAndSendsTheCancelPair) {
	mail::LogCapture audit("AUDIT_LOG");
	Npc& luelas = knownNpc(LUELAS);
	give(820001, SALT, 1);
	fillCube(820100);
	ride();
	player().setVisualState(model::gameobjects::state::CreatureVisualState::HIDE1);
	const Materials inina{{ININA, 1}};

	// :151-153: a known object that is not a StaticObject, and an object id the player does not know (null): an audit line, no message
	craft(ROAST_ININA_RECIPE, luelas.getObjectId(), 0, inina);
	EXPECT_EQ(sent(), cancelPair(COOKING, ROAST_ININA, luelas.getObjectId()));
	clearSent();
	craft(ROAST_ININA_RECIPE, 0, 0, inina);
	EXPECT_EQ(sent(), cancelPair(COOKING, ROAST_ININA, 0));
	int32_t targetLines = 0;
	for (const std::string& line : audit.lines())
		targetLines += line.find("tried to craft with incorrect target") != std::string::npos ? 1 : 0;
	EXPECT_EQ(targetLines, 2) << "one audit line per refusal";
	clearSent();

	// :154-156: isInRange(player, target, 5, false) adds both bound radii (player 0.25, a station 0; oracle craft.station.effectiveRange 5.25)
	// and compares squared distances strictly: 5.3 m is out, 5.2 m in
	StaticObject& far = station(5.3f);
	craft(ROAST_ININA_RECIPE, far.getObjectId(), 0, inina);
	EXPECT_EQ(sent(), then(systemMessage(SM_SYSTEM_MESSAGE::STR_COMBINE_TOO_FAR_FROM_TOOL(javaL10n(OVEN_NAME))),
						  cancelPair(COOKING, ROAST_ININA, far.getObjectId())));
	clearSent();
	StaticObject& oven = station(5.2f);
	const int32_t ovenId = oven.getObjectId();

	// :165-168: riding, then (dismounted) hidden
	const std::vector<std::vector<uint8_t>> stanceRefusal =
		then(systemMessage(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_COMBINE_WHILE_IN_CURRENT_STANCE()), cancelPair(COOKING, ROAST_ININA, ovenId));
	craft(ROAST_ININA_RECIPE, ovenId, 0, inina);
	EXPECT_EQ(sent(), stanceRefusal);
	clearSent();
	dismount();
	craft(ROAST_ININA_RECIPE, ovenId, 0, inina);
	EXPECT_EQ(sent(), stanceRefusal);
	clearSent();
	player().unsetVisualState(model::gameobjects::state::CreatureVisualState::HIDE1);

	// :170-173: the full cube
	craft(ROAST_ININA_RECIPE, ovenId, 0, inina);
	EXPECT_EQ(sent(), then(systemMessage(SM_SYSTEM_MESSAGE::STR_COMBINE_INVENTORY_IS_FULL()), cancelPair(COOKING, ROAST_ININA, ovenId)));
	ASSERT_TRUE(player().getInventory().decreaseByItemId(TRAINING_SWORD, 5)); // room for the items the next steps give
	clearSent();

	// :175-178: the recipe is not known
	craft(ROAST_ININA_RECIPE, ovenId, 0, inina);
	EXPECT_EQ(sent(), then(systemMessage(SM_SYSTEM_MESSAGE::STR_COMBINE_CAN_NOT_FIND_RECIPE()), cancelPair(COOKING, ROAST_ININA, ovenId)));
	clearSent();
	setRecipes({ROAST_ININA_RECIPE});

	// :187-190: no cooking skill; :192-196: cooking below the recipe's skillpoint 1 - both name the skill
	craft(ROAST_ININA_RECIPE, ovenId, 0, inina);
	EXPECT_EQ(sent(), then(systemMessage(SM_SYSTEM_MESSAGE::STR_COMBINE_CANT_USE(javaL10n(COOKING_NAME))), cancelPair(COOKING, ROAST_ININA, ovenId)));
	clearSent();
	setSkills({{COOKING, 0}});
	craft(ROAST_ININA_RECIPE, ovenId, 0, inina);
	EXPECT_EQ(sent(),
		then(systemMessage(SM_SYSTEM_MESSAGE::STR_COMBINE_OUT_OF_SKILL_POINT(javaL10n(COOKING_NAME))), cancelPair(COOKING, ROAST_ININA, ovenId)));
	clearSent();
	setSkills({{COOKING, 1}});

	// :198-214: the components of the sent alternative in document order - no Inina (quantity 1: SINGLE) before too little Salt (2: MULTIPLE)
	craft(ROAST_ININA_RECIPE, ovenId, 0, inina);
	EXPECT_EQ(sent(), then(systemMessage(SM_SYSTEM_MESSAGE::STR_COMBINE_NO_COMPONENT_ITEM_SINGLE(javaL10n(ININA_NAME))),
						  cancelPair(COOKING, ROAST_ININA, ovenId)));
	clearSent();
	give(820002, ININA, 1);
	craft(ROAST_ININA_RECIPE, ovenId, 0, inina);
	EXPECT_EQ(sent(), then(systemMessage(SM_SYSTEM_MESSAGE::STR_COMBINE_NO_COMPONENT_ITEM_MULTIPLE(2, javaL10n(SALT_NAME))),
						  cancelPair(COOKING, ROAST_ININA, ovenId)));
	clearSent();

	// :198-220: the component check comes before the stone check - craft type 1 with the stone held and too little Salt is refused for the Salt,
	// and the stone stays (it is taken, :216, only once every component was found)
	give(820004, COOKING_STONE, 1);
	craft(ROAST_ININA_RECIPE, ovenId, 1, inina);
	EXPECT_EQ(sent(), then(systemMessage(SM_SYSTEM_MESSAGE::STR_COMBINE_NO_COMPONENT_ITEM_MULTIPLE(2, javaL10n(SALT_NAME))),
						  cancelPair(COOKING, ROAST_ININA, ovenId)));
	EXPECT_EQ(countOf(COOKING_STONE), 1);
	ASSERT_TRUE(player().getInventory().decreaseByItemId(COOKING_STONE, 1)); // the next step wants no stone
	clearSent();
	give(820003, SALT, 1);

	// :216-220: craft type 1 without the cooking stone (getBonusReqItem(40001) = 169401081); the materials are still all there
	craft(ROAST_ININA_RECIPE, ovenId, 1, inina);
	EXPECT_EQ(sent(), then(systemMessage(SM_SYSTEM_MESSAGE::STR_COMBINE_NO_COMPONENT_ITEM_SINGLE(javaL10n(COOKING_STONE_NAME))),
						  cancelPair(COOKING, ROAST_ININA, ovenId)));
	EXPECT_EQ(countOf(ININA), 1);
	EXPECT_EQ(countOf(SALT), 2);
	clearSent();
	give(820005, COOKING_STONE, 1);

	// every check passes with exactly the recipe's counts: the stone, then the alternative's components are taken (:216, :222-230), no cancel
	pastCheckCraft(ROAST_ININA_RECIPE, ovenId, 1, inina);
	EXPECT_EQ(countOf(COOKING_STONE), 0);
	EXPECT_EQ(countOf(ININA), 0);
	EXPECT_EQ(countOf(SALT), 0);
	EXPECT_FALSE(sentCancel()) << "no sendCancelCraft";
	EXPECT_EQ(commonData().getDp(), 0);
}

// :180-185: a running craft cooldown of the recipe's craft_delay_id refuses after the recipe check and before the skill check (oracle
// craft.checks); a recipe without a craft_delay_id ignores every cooldown
TEST_F(CraftServiceTest, ARunningCraftCooldownRefusesAfterTheRecipeCheckAndBeforeTheSkillCheck) {
	const int32_t ovenId = station(3.0f).getObjectId();
	ASSERT_EQ(player().getCraftCooldowns()->put(1, commons::utils::currentTimeMillis() + 30000), std::nullopt);

	craft(DELAYED_SUPPLEMENT_RECIPE, ovenId);
	EXPECT_EQ(sent(),
		then(systemMessage(SM_SYSTEM_MESSAGE::STR_COMBINE_CAN_NOT_FIND_RECIPE()), cancelPair(WEAPONSMITHING, SIEGE_WEAPON_SUPPLEMENT, ovenId)));
	clearSent();
	setRecipes({DELAYED_SUPPLEMENT_RECIPE, LIMITED_SUPPLEMENT_RECIPE});
	craft(DELAYED_SUPPLEMENT_RECIPE, ovenId);
	EXPECT_EQ(sent(),
		then(systemMessage(SM_SYSTEM_MESSAGE::STR_ITEM_CANT_USE_UNTIL_DELAY_TIME()), cancelPair(WEAPONSMITHING, SIEGE_WEAPON_SUPPLEMENT, ovenId)));
	clearSent();

	// the same product without a craft_delay_id (155090012) is not held back by cooldown 1
	setSkills({{WEAPONSMITHING, 1}});
	pastCheckCraft(LIMITED_SUPPLEMENT_RECIPE, ovenId, 0, {});
	endCraft();

	// the cooldown over (Cooldowns.put of a past time removes it): the next check, the skill level, decides
	setSkills({});
	player().getCraftCooldowns()->put(1, commons::utils::currentTimeMillis() - 1);
	craft(DELAYED_SUPPLEMENT_RECIPE, ovenId);
	EXPECT_EQ(sent(), then(systemMessage(SM_SYSTEM_MESSAGE::STR_COMBINE_CANT_USE(javaL10n(WEAPONSMITHING_NAME))),
						  cancelPair(WEAPONSMITHING, SIEGE_WEAPON_SUPPLEMENT, ovenId)));
}

// :149-163: the morph skill (40009) needs no target, and the DP check comes before the stance check. A starting class has 0 DP (setDp returns
// at once), so it is refused every dp > 0 recipe (oracle dp.refusedForStartingClass) with an audit line and no message; SM_CRAFT_UPDATE's
// delay is 1000 for the morph skill. Passed, startCrafting spends the recipe's DP (:109-110) and consumes the powder.
TEST_F(CraftServiceTest, TheMorphNeedsNoTargetAndItsDpIsCheckedBeforeTheStance) {
	mail::LogCapture audit("AUDIT_LOG");
	setRecipes({ARIA_MORPH_RECIPE});
	setSkills({{MORPH, 1}});
	give(820011, AETHER_POWDER, 1);
	ride();
	const Materials powder{{AETHER_POWDER, 1}};

	craft(ARIA_MORPH_RECIPE, 0, 0, powder);
	EXPECT_EQ(sent(), cancelPair(MORPH, ARIA, 0)) << "the starting class's 0 DP, before the stance";
	setDaevaDp(199);
	clearSent();
	craft(ARIA_MORPH_RECIPE, 0, 0, powder);
	EXPECT_EQ(sent(), cancelPair(MORPH, ARIA, 0)) << "199 < 200";
	int32_t dpLines = 0;
	for (const std::string& line : audit.lines()) {
		dpLines += line.find("tried to craft without required DP count") != std::string::npos ? 1 : 0;
		EXPECT_EQ(line.find("incorrect target"), std::string::npos) << "the morph skips the target check: " << line;
	}
	EXPECT_EQ(dpLines, 2);
	clearSent();

	commonData().setDp(200);
	craft(ARIA_MORPH_RECIPE, 0, 0, powder);
	EXPECT_EQ(sent(), then(systemMessage(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_COMBINE_WHILE_IN_CURRENT_STANCE()), cancelPair(MORPH, ARIA, 0)));
	EXPECT_EQ(commonData().getDp(), 200);
	EXPECT_EQ(countOf(AETHER_POWDER), 1);
	clearSent();

	dismount();
	pastCheckCraft(ARIA_MORPH_RECIPE, 0, 0, powder);
	EXPECT_EQ(commonData().getDp(), 0) << "addDp(-200)";
	EXPECT_EQ(countOf(AETHER_POWDER), 0);
}

// :123 and m5c-plan.md C-01: Java's unconditional (StaticObject) cast of the target. checkCraft skips the target check for the morph skill, so a
// known object that is not a StaticObject (an npc the morphing player targets) reaches the cast and throws ClassCastException - after the
// powder was consumed and the DP spent (:109-110, :222-230), which Java does not give back. A null target (an object id the player does not
// know) and a StaticObject reach the task's constructor.
TEST_F(CraftServiceTest, TheMorphsTargetIsCastToAStaticObject) {
	setRecipes({ARIA_MORPH_RECIPE});
	setSkills({{MORPH, 1}});
	setDaevaDp(600);
	give(820021, AETHER_POWDER, 3);
	const Materials powder{{AETHER_POWDER, 1}};
	Npc& luelas = knownNpc(LUELAS);

	runtime::resetUnportedHitsForTests();
	EXPECT_THROW(craft(ARIA_MORPH_RECIPE, luelas.getObjectId(), 0, powder), runtime::ClassCastException);
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "the cast throws before the task is constructed";
	EXPECT_EQ(commonData().getDp(), 400);
	EXPECT_EQ(countOf(AETHER_POWDER), 2);
	EXPECT_TRUE(craftUpdateActions(sent()).empty()) << "no cancel pair: the exception leaves startCrafting";
	EXPECT_FALSE(player().getInteractionTask());
	clearSent();

	pastCheckCraft(ARIA_MORPH_RECIPE, 0, 0, powder); // runtime::cast of a null Ptr is a null Ptr: CraftingTask's nullable responder
	EXPECT_EQ(commonData().getDp(), 200);
	EXPECT_EQ(countOf(AETHER_POWDER), 1);
	endCraft();

	const int32_t ovenId = station(3.0f).getObjectId();
	pastCheckCraft(ARIA_MORPH_RECIPE, ovenId, 0, powder);
	EXPECT_EQ(commonData().getDp(), 0);
	EXPECT_EQ(countOf(AETHER_POWDER), 0);
}

// :198-230 (oracle craft.materials.rule): only the FIRST <components_data> whose first component's item id is a key of the sent map is
// checked and consumed (the counts sent are ignored); a map with no alternative's first item checks and consumes nothing
TEST_F(CraftServiceTest, TheMaterialsAreTheFirstAlternativeWhoseFirstItemWasSent) {
	const int32_t ovenId = station(3.0f).getObjectId();
	setRecipes({LIMITED_SUPPLEMENT_RECIPE});
	setSkills({{WEAPONSMITHING, 1}});
	give(820031, SIEGE_WEAPON_FUEL_A, 1);
	give(820032, PURE_KATALIUM, 1);
	give(820033, SIEGE_WEAPON_FUEL_B, 1);
	give(820034, PURE_ANCIENT_AETHER, 1);

	pastCheckCraft(LIMITED_SUPPLEMENT_RECIPE, ovenId, 0, {{SIEGE_WEAPON_FUEL_B, 99}});
	EXPECT_EQ(countOf(SIEGE_WEAPON_FUEL_B), 0) << "the second alternative";
	EXPECT_EQ(countOf(PURE_ANCIENT_AETHER), 0);
	EXPECT_EQ(countOf(SIEGE_WEAPON_FUEL_A), 1);
	EXPECT_EQ(countOf(PURE_KATALIUM), 1);
	endCraft();

	pastCheckCraft(LIMITED_SUPPLEMENT_RECIPE, ovenId, 0, {{PURE_KATALIUM, 1}}); // a second component is no alternative's first
	EXPECT_EQ(countOf(SIEGE_WEAPON_FUEL_A), 1);
	EXPECT_EQ(countOf(PURE_KATALIUM), 1);
	endCraft();

	// both first items sent, both alternatives held: the first alternative in document order is checked and taken, and only it (:213, :229)
	give(820035, SIEGE_WEAPON_FUEL_B, 1);
	give(820036, PURE_ANCIENT_AETHER, 1);
	pastCheckCraft(LIMITED_SUPPLEMENT_RECIPE, ovenId, 0, {{SIEGE_WEAPON_FUEL_B, 1}, {SIEGE_WEAPON_FUEL_A, 1}});
	EXPECT_EQ(countOf(SIEGE_WEAPON_FUEL_A), 0);
	EXPECT_EQ(countOf(PURE_KATALIUM), 0);
	EXPECT_EQ(countOf(SIEGE_WEAPON_FUEL_B), 1);
	EXPECT_EQ(countOf(PURE_ANCIENT_AETHER), 1);
	endCraft();

	// the sent alternative short of its second item: SINGLE for its quantity 1, and nothing is taken
	ASSERT_TRUE(player().getInventory().decreaseByItemId(PURE_ANCIENT_AETHER, 1));
	clearSent();
	craft(LIMITED_SUPPLEMENT_RECIPE, ovenId, 0, {{SIEGE_WEAPON_FUEL_B, 1}});
	EXPECT_EQ(sent(), then(systemMessage(SM_SYSTEM_MESSAGE::STR_COMBINE_NO_COMPONENT_ITEM_SINGLE(javaL10n(PURE_ANCIENT_AETHER_NAME))),
						  cancelPair(WEAPONSMITHING, SIEGE_WEAPON_SUPPLEMENT, ovenId)));
	EXPECT_EQ(countOf(SIEGE_WEAPON_FUEL_B), 1);

	// both first items sent, the first alternative held in full and the second still short: the check loop stops after the first alternative
	// it checked (:213), so the short second one is never looked at - the craft passes and takes the first alternative only
	give(820037, SIEGE_WEAPON_FUEL_A, 1);
	give(820038, PURE_KATALIUM, 1);
	pastCheckCraft(LIMITED_SUPPLEMENT_RECIPE, ovenId, 0, {{SIEGE_WEAPON_FUEL_A, 1}, {SIEGE_WEAPON_FUEL_B, 1}});
	EXPECT_EQ(countOf(SIEGE_WEAPON_FUEL_A), 0);
	EXPECT_EQ(countOf(PURE_KATALIUM), 0);
	EXPECT_EQ(countOf(SIEGE_WEAPON_FUEL_B), 1);
}

// getBonusReqItem (:240-258): craft type 1 wants the profession's enhancement stone (oracle craft.bonusItem); without it the refusal names
// the stone. The morph skill has none (0): decreaseByItemId(0, 1) fails and ITEM_DATA.getItemTemplate(0).getL10n() is Java's
// NullPointerException, before anything is taken.
TEST_F(CraftServiceTest, CraftType1WantsTheEnhancementStoneOfTheProfession) {
	const int32_t ovenId = station(3.0f).getObjectId();
	struct Row {
		int32_t skillId;
		int32_t recipeId;
		int32_t productId;
		int32_t stoneName;
	};
	const Row rows[] = {
		{COOKING, ROAST_ININA_RECIPE, ROAST_ININA, COOKING_STONE_NAME},
		{WEAPONSMITHING, PISTOL_RECIPE, NOBLE_PREMIUM_OPHIDAN_PISTOL, WEAPONSMITHING_STONE_NAME},
		{ARMORSMITHING, METAL_PLATE_RECIPE, 182290358, ARMORSMITHING_STONE_NAME},
		{TAILORING, THIN_LEATHER_RECIPE, 152020033, TAILORING_STONE_NAME},
		{ALCHEMY, STONE_POWDER_RECIPE, 152020111, ALCHEMY_STONE_NAME},
		{HANDICRAFTING, BETUA_WOOD_RECIPE, 152020087, HANDICRAFTING_STONE_NAME},
		{CONSTRUCTION, WOODEN_SHELF_RECIPE, 182290609, CONSTRUCTION_STONE_NAME},
	};
	for (const Row& row : rows) {
		SCOPED_TRACE(row.skillId);
		setSkills({{row.skillId, 1}});
		setRecipes({row.recipeId});
		clearSent();
		craft(row.recipeId, ovenId, 1, {}); // no alternative's first item: no component check (:200-201)
		EXPECT_EQ(sent(), then(systemMessage(SM_SYSTEM_MESSAGE::STR_COMBINE_NO_COMPONENT_ITEM_SINGLE(javaL10n(row.stoneName))),
							  cancelPair(row.skillId, row.productId, ovenId)));
	}

	give(820041, WEAPONSMITHING_STONE, 1);
	setSkills({{WEAPONSMITHING, 1}});
	setRecipes({PISTOL_RECIPE});
	pastCheckCraft(PISTOL_RECIPE, ovenId, 1, {});
	EXPECT_EQ(countOf(WEAPONSMITHING_STONE), 0);
	endCraft();

	setSkills({{MORPH, 1}});
	setRecipes({ARIA_MORPH_RECIPE});
	setDaevaDp(200);
	give(820042, AETHER_POWDER, 1);
	clearSent();
	EXPECT_THROW(craft(ARIA_MORPH_RECIPE, 0, 1, {{AETHER_POWDER, 1}}), runtime::NullPointerException);
	EXPECT_EQ(countOf(AETHER_POWDER), 1);
	EXPECT_EQ(commonData().getDp(), 200);
	EXPECT_TRUE(sent().empty());
}

// :99-107: a recipe id without a template is Java's NullPointerException at recipeTemplate.getSkillId() (checkCraft's null check is never
// reached), and a recipe whose product has no item template fails checkCraft (:141-143) and then throws in sendCancelCraft's SM_CRAFT_UPDATE
// (item.getTemplateId()); oracle craft.checksNote. Nothing is sent either way. The player has the recipe's 200 DP and every other check would
// pass, so it is the product check that refuses: nothing is spent.
TEST_F(CraftServiceTest, AMissingRecipeOrProductTemplateIsJavasNullPointerException) {
	const int32_t ovenId = station(3.0f).getObjectId();
	EXPECT_THROW(craft(155999999, ovenId), runtime::NullPointerException);
	setRecipes({NOT_AUTOLEARN_MORPH_RECIPE});
	setSkills({{MORPH, 1}});
	setDaevaDp(200);
	EXPECT_THROW(craft(NOT_AUTOLEARN_MORPH_RECIPE, 0), runtime::NullPointerException);
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(commonData().getDp(), 200);
}

// :112-131 (oracle craft.timing): the task's interval is 2500 - 60 x (skill level - skillpoint), at least 1200 (1500 for an UNIQUE or EPIC
// product, 1700 for a MYTHIC one, 1200 for any other quality - LEGEND included), and 200 for the morph skill; the first tick comes after AbstractInteractionTask's delay of 1000. A second
// craft while the task runs is refused (:145-147) with nothing taken. Needs C-02 (CraftingTask's constructor): skipped until then.
TEST_F(CraftServiceTest, TheCraftIntervalFollowsTheLevelDifferenceTheQualityAndTheMorph) {
	const int32_t ovenId = station(3.0f).getObjectId();
	struct Row {
		int32_t recipeId;
		int32_t skillId;
		int32_t skillLevel;
		std::chrono::milliseconds interval;
	};
	const Row rows[] = {
		// --recipe ID --skill-level N: craft.timing.interval
		{ROAST_ININA_RECIPE, COOKING, 1, std::chrono::milliseconds(2500)},               // difference 0
		{ROAST_ININA_RECIPE, COOKING, 20, std::chrono::milliseconds(1360)},              // 2500 - 19 x 60
		{ROAST_ININA_RECIPE, COOKING, 30, std::chrono::milliseconds(1200)},              // 760 -> the COMMON cap
		{PISTOL_RECIPE, WEAPONSMITHING, 11, std::chrono::milliseconds(1900)},            // 2500 - 10 x 60
		{PISTOL_RECIPE, WEAPONSMITHING, 31, std::chrono::milliseconds(1500)},            // 700 -> the EPIC cap
		{LIMITED_SUPPLEMENT_RECIPE, WEAPONSMITHING, 31, std::chrono::milliseconds(1200)}, // 700 -> a LEGEND product has the default cap
		{WEAPONSMITH_450_RECIPE, WEAPONSMITHING, 450, std::chrono::milliseconds(2500)},  // difference 0
		{WEAPONSMITH_450_RECIPE, WEAPONSMITHING, 470, std::chrono::milliseconds(1500)},  // 1300 -> the UNIQUE cap
		{KATALIUM_SHIELD_RECIPE, ARMORSMITHING, 520, std::chrono::milliseconds(1700)},   // 1300 -> the MYTHIC cap
		{ARIA_MORPH_RECIPE, MORPH, 1, std::chrono::milliseconds(200)},                   // the morph skill (as --recipe 155004288)
	};
	setDaevaDp(1000);
	for (const Row& row : rows) {
		SCOPED_TRACE(std::to_string(row.recipeId) + " at " + std::to_string(row.skillLevel));
		setSkills({{row.skillId, row.skillLevel}});
		setRecipes({row.recipeId});
		if (pastCheckCraft(row.recipeId, row.skillId == MORPH ? 0 : ovenId, 0, {}) == PastCheckCraft::TASK_CONSTRUCTOR_UNPORTED)
			GTEST_SKIP() << "CraftingTask's constructor is C-02's (m5c-plan.md): pending until the craft-task lane merges";
		const std::chrono::steady_clock::time_point started = clock.now();
		ASSERT_EQ(executor->nextDueTime(), started + std::chrono::milliseconds(1000)) << "AbstractInteractionTask.delay";
		executor->advance(std::chrono::milliseconds(1000));
		ASSERT_EQ(executor->nextDueTime(), clock.now() + row.interval);

		clearSent();
		give(820051, SALT, 1);
		craft(row.recipeId, row.skillId == MORPH ? 0 : ovenId, 0, {{SALT, 1}});
		std::vector<int32_t> actions = craftUpdateActions(sent());
		ASSERT_FALSE(actions.empty());
		EXPECT_EQ(actions.back(), 4) << "the running craft refuses a second one";
		endCraft();
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// finishCrafting (CraftService.java:43-95)
// ---------------------------------------------------------------------------------------------------------------------------------------------

// --recipe 155001381 (skillUp): xpReward (int) (0.008 x 101 x 101 + 60) = 141, gained skill xp 141 x rate 1.0 x BOOST_COOKING_XP_RATE 100 /
// 100 = 141 >= requiredExp 76, so cooking 1 -> 2 with 0 xp (SM_SKILL_LIST 40001 level 2, message 1330064); the character gets the 141 as
// exp (offline: no rate, PlayerCommonData.addExp); the product: 2 Roast Inina without a creator name (not a weapon or armor, :77-81)
TEST_F(CraftServiceTest, FinishingRoastIninaGivesJavasXpAndLevelsCookingUp) {
	setSkills({{COOKING, 1}});
	setRecipes({ROAST_ININA_RECIPE});
	const int64_t exp = commonData().getExp();

	finish(ROAST_ININA_RECIPE);

	EXPECT_EQ(skill(COOKING).getSkillLevel(), 2);
	EXPECT_EQ(skill(COOKING).getCurrentXp(), 0);
	EXPECT_EQ(commonData().getExp(), exp + 141);
	EXPECT_EQ(countOf(ROAST_ININA), 2);
	EXPECT_EQ(countOf(TASTY_ROAST_ININA), 0);
	EXPECT_EQ(onlyItem(ROAST_ININA).getItemCreator(), "");
	EXPECT_TRUE(sentContains(serializedFor(network::aion::serverpackets::SM_SKILL_LIST(skill(COOKING), 1330064))));
	EXPECT_TRUE(knowsRecipe(ROAST_ININA_RECIPE)) << "not a limited recipe";
	EXPECT_EQ(player().getCraftCooldowns()->get(1), std::nullopt) << "no craft_delay_id";
}

// :72-83: the item predicate's two packet types. A new product enters the cube as ItemAddType.CRAFTED_ITEM (SM_INVENTORY_ADD_ITEM's mask
// 0x2D, ItemPacketService.java:92), a product that stacks onto one the cube holds updates that stack as ItemUpdateType.INC_ITEM_COLLECT
// (SM_INVENTORY_UPDATE_ITEM's closing mask 0x19, :39; ItemService.addStackableItem, predicate.getUpdateType)
TEST_F(CraftServiceTest, TheProductEntersTheCubeAsACraftedItemAndStacksAsACollectedOne) {
	namespace sp = network::aion::serverpackets;
	using ItemAddType = services::item::ItemPacketService_ItemAddType;
	using ItemUpdateType = services::item::ItemPacketService_ItemUpdateType;
	setSkills({{COOKING, 30}});
	finish(ROAST_ININA_RECIPE);
	model::gameobjects::Item& roast = onlyItem(ROAST_ININA);
	const std::vector<std::vector<uint8_t>> added = itemtest::packetsOf(sent(), itemtest::SM_INVENTORY_ADD_ITEM_OPCODE);
	EXPECT_EQ(added, cp::exactly({serializedFor(sp::SM_INVENTORY_ADD_ITEM({runtime::Ptr<model::gameobjects::Item>(roast)}, player(),
						 ItemAddType::CRAFTED_ITEM))}));
	// SM_INVENTORY_ADD_ITEM.writeImpl writes the add type's mask first
	EXPECT_EQ(added.empty() ? -1 : PacketReader(cp::bodyOf(added.front())).H(), 0x2D);
	clearSent();

	finish(ROAST_ININA_RECIPE);
	EXPECT_EQ(roast.getItemCount(), 4);
	const std::vector<std::vector<uint8_t>> updated = itemtest::packetsOf(sent(), itemtest::SM_INVENTORY_UPDATE_ITEM_OPCODE);
	EXPECT_EQ(updated, cp::exactly({serializedFor(sp::SM_INVENTORY_UPDATE_ITEM(player(), roast, ItemUpdateType::INC_ITEM_COLLECT))}));
	// SM_INVENTORY_UPDATE_ITEM.writeImpl writes the update type's mask last
	EXPECT_EQ(updated.empty() ? -1 : closingH(updated.front()), 0x19);
}

// addSkillXp's threshold, with the gained xp of finishCrafting: --skill-level 30 --skill-xp 370 ends at 511 (requiredExp 512), --skill-xp 371
// levels 30 -> 31 with 0 xp
TEST_F(CraftServiceTest, TheGainedSkillXpMeetsTheLevelUpThreshold) {
	setSkills({{COOKING, 30}});
	skill(COOKING).setCurrentXp(370);
	finish(ROAST_ININA_RECIPE);
	EXPECT_EQ(skill(COOKING).getSkillLevel(), 30);
	EXPECT_EQ(skill(COOKING).getCurrentXp(), 511);

	setSkills({{COOKING, 30}});
	skill(COOKING).setCurrentXp(371);
	finish(ROAST_ININA_RECIPE);
	EXPECT_EQ(skill(COOKING).getSkillLevel(), 31);
	EXPECT_EQ(skill(COOKING).getCurrentXp(), 0);
}

// :56: the bonus (CraftingTask's 15 for craft type 1) raises the xp reward itself, int arithmetic: 141 + 141 x 15 / 100 = 162 (--craft-type 1:
// xpRewardWithBonus 162), both as skill xp and as the character's exp
TEST_F(CraftServiceTest, TheBonusRaisesTheXpReward) {
	setSkills({{COOKING, 30}});
	const int64_t exp = commonData().getExp();
	finish(ROAST_ININA_RECIPE, 0, 15);
	EXPECT_EQ(skill(COOKING).getCurrentXp(), 162);
	EXPECT_EQ(commonData().getExp(), exp + 162);
}

// :57-61: the skill xp rate (gameserver.rates.skill_xp.crafting, the membership's entry) and at least 1; the character's exp stays the xp
// reward. --set gameserver.rates.skill_xp.crafting=0.0,0.0: gainedSkillXp 1; =1.5,2.0: 211
TEST_F(CraftServiceTest, TheSkillXpRateScalesOnlyTheSkillXpAndLeavesAtLeastOne) {
	const int64_t exp = commonData().getExp();
	setRates(configs::main::RatesConfig::SKILL_XP_CRAFTING_RATES, {0.0f, 0.0f});
	setSkills({{COOKING, 30}});
	finish(ROAST_ININA_RECIPE);
	EXPECT_EQ(skill(COOKING).getCurrentXp(), 1);
	EXPECT_EQ(commonData().getExp(), exp + 141);

	setRates(configs::main::RatesConfig::SKILL_XP_CRAFTING_RATES, {1.5f, 2.0f});
	setSkills({{COOKING, 30}});
	finish(ROAST_ININA_RECIPE);
	EXPECT_EQ(skill(COOKING).getCurrentXp(), 211);
	EXPECT_EQ(commonData().getExp(), exp + 282);
}

// :58-60: the profession's boost stat (StatEnum.getModifier(40001) = BOOST_COOKING_XP_RATE): gainedCraftXp *= current / 100f, narrowed back to
// int - (int) (141 x (150 / 100f)) = 211 with +50; a stat of another profession changes nothing (not modelled by the oracle, which assumes the
// base 100: Java arithmetic)
TEST_F(CraftServiceTest, TheProfessionsBoostStatScalesTheSkillXp) {
	addStat(model::stats::container::StatEnum::BOOST_WEAPONSMITHING_XP_RATE, 50);
	setSkills({{COOKING, 30}});
	finish(ROAST_ININA_RECIPE);
	EXPECT_EQ(skill(COOKING).getCurrentXp(), 141);

	addStat(model::stats::container::StatEnum::BOOST_COOKING_XP_RATE, 50);
	setSkills({{COOKING, 30}});
	const int64_t exp = commonData().getExp();
	finish(ROAST_ININA_RECIPE);
	EXPECT_EQ(skill(COOKING).getCurrentXp(), 211);
	EXPECT_EQ(commonData().getExp(), exp + 141) << "the character's exp is the xp reward, not the boosted skill xp";
}

// :63-68: no skill xp when the skill is more than 40 above the recipe (--skill-level 42: granted false; 41 still gains): STR_MSG_DONT_GET_
// PRODUCTION_EXP with the skill's name and no exp for the character; the product is made all the same
TEST_F(CraftServiceTest, ASkillFarAboveTheRecipeGetsNoXpButTheProduct) {
	setSkills({{COOKING, 41}});
	const int64_t exp = commonData().getExp();
	finish(ROAST_ININA_RECIPE);
	EXPECT_EQ(skill(COOKING).getCurrentXp(), 141);
	EXPECT_EQ(commonData().getExp(), exp + 141);

	setSkills({{COOKING, 42}});
	clearSent();
	finish(ROAST_ININA_RECIPE);
	EXPECT_EQ(skill(COOKING).getCurrentXp(), 0);
	EXPECT_EQ(commonData().getExp(), exp + 141);
	EXPECT_EQ(countOf(ROAST_ININA), 4);
	EXPECT_EQ(systemMessagesOf(sent()),
		cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_MSG_DONT_GET_PRODUCTION_EXP(javaL10n(COOKING_NAME)))}));
}

// :64: the character's exp goes through Rates.XP_CRAFTING (an online character: PlayerCommonData.addExp applies the rate only with a player,
// --set gameserver.rates.xp.crafting=2.0,2.0: playerExp.reward 282) and STR_GET_EXP2 tells him
TEST_F(CraftServiceTest, TheCharactersExpGoesThroughTheCraftingXpRate) {
	setRates(configs::main::RatesConfig::XP_CRAFTING_RATES, {2.0f, 2.0f});
	goOnline();
	setSkills({{COOKING, 30}});
	const int64_t exp = commonData().getExp();
	clearSent();
	finish(ROAST_ININA_RECIPE);
	EXPECT_EQ(commonData().getExp(), exp + 282);
	EXPECT_TRUE(sentContains(systemMessage(SM_SYSTEM_MESSAGE::STR_GET_EXP2(282))));
	EXPECT_EQ(skill(COOKING).getCurrentXp(), 141) << "the skill xp rate is another";
}

// :70-72: a critical craft makes the combo product of its crit count (getComboProduct(1) = Tasty Roast Inina, 2 of them); a crit count past the
// recipe's combo products is RecipeTemplate.getComboProduct's IndexOutOfBoundsException, after the xp was given
TEST_F(CraftServiceTest, ACriticalCraftMakesTheComboProduct) {
	setSkills({{COOKING, 30}});
	finish(ROAST_ININA_RECIPE, 1);
	EXPECT_EQ(countOf(TASTY_ROAST_ININA), 2);
	EXPECT_EQ(countOf(ROAST_ININA), 0);

	EXPECT_THROW(finish(ROAST_ININA_RECIPE, 2), runtime::IndexOutOfBoundsException);
	EXPECT_EQ(skill(COOKING).getCurrentXp(), 282);
	EXPECT_EQ(countOf(TASTY_ROAST_ININA), 2);
}

// :73-83 (CraftService$1.changeItem): a crafted weapon or armor carries the crafter's name - the gun (item_group GUN, a weapon) and the
// shield (item_group SHIELD: ItemSubType.SHIELD is an armor type, ItemTemplate.isArmor); the food of FinishingRoastInina... does not
TEST_F(CraftServiceTest, ACraftedWeaponOrArmorCarriesTheCraftersName) {
	setSkills({{WEAPONSMITHING, 1}, {ARMORSMITHING, 500}});
	finish(PISTOL_RECIPE);
	EXPECT_EQ(onlyItem(NOBLE_PREMIUM_OPHIDAN_PISTOL).getItemCreator(), "Holder");
	finish(KATALIUM_SHIELD_RECIPE);
	EXPECT_EQ(onlyItem(EXALTED_KATALIUM_SHIELD).getItemCreator(), "Holder");
}

// :85-89: gameserver.log.craft (off in the shipped logging.properties:16) writes one CRAFT_LOG line per craft, " - critical" for a crit
TEST_F(CraftServiceTest, TheCraftLogLineNamesThePlayerTheProductAndTheCount) {
	mail::LogCapture craftLog("CRAFT_LOG");
	setSkills({{COOKING, 30}});
	finish(ROAST_ININA_RECIPE);
	EXPECT_TRUE(craftLog.lines().empty());

	configs::main::LoggingConfig::LOG_CRAFT.store(true);
	finish(ROAST_ININA_RECIPE);
	finish(ROAST_ININA_RECIPE, 1);
	EXPECT_EQ(craftLog.lines(), (std::vector<std::string>{"Player Holder crafted item 160001001 [Roast Inina] (count: 2)",
									"Player Holder crafted item 160001051 [Tasty Roast Inina] (count: 2) - critical"}));
}

// :91-94 (--recipe 155090013: afterCraft.cooldownId 1, cooldownMillis 30000): the craft cooldown of the recipe's craft_delay_id ends
// craft_delay_time seconds after the craft, which checkCraft then refuses
TEST_F(CraftServiceTest, ARecipeWithACraftDelayStartsItsCooldown) {
	setSkills({{WEAPONSMITHING, 1}});
	const int64_t before = commons::utils::currentTimeMillis();
	finish(DELAYED_SUPPLEMENT_RECIPE);
	const int64_t after = commons::utils::currentTimeMillis();
	std::optional<int64_t> reuseTime = player().getCraftCooldowns()->get(1);
	ASSERT_TRUE(reuseTime);
	EXPECT_GE(*reuseTime, before + 30000);
	EXPECT_LE(*reuseTime, after + 30000);
	EXPECT_EQ(countOf(SIEGE_WEAPON_SUPPLEMENT), 1);

	setRecipes({DELAYED_SUPPLEMENT_RECIPE});
	const int32_t ovenId = station(3.0f).getObjectId();
	craft(DELAYED_SUPPLEMENT_RECIPE, ovenId);
	EXPECT_EQ(sent(),
		then(systemMessage(SM_SYSTEM_MESSAGE::STR_ITEM_CANT_USE_UNTIL_DELAY_TIME()), cancelPair(WEAPONSMITHING, SIEGE_WEAPON_SUPPLEMENT, ovenId)));
}

// :45-51: a recipe with a max_production_count is forgotten once crafted (RecipeList.deleteRecipe: the player_recipes row, SM_RECIPE_DELETE);
// a recipe without one stays. Weaponsmithing 1 -> 2 on the first craft learns the level-1 autolearn recipe 155000075 on the way
// (PlayerSkillList.addSkillXp -> SkillLearnService.onLearnSkill -> RecipeService.autoLearnRecipes; --skill 40002 --level 1)
TEST_F(CraftServiceTest, ALimitedRecipeIsForgottenOnceCrafted) {
	CRAFT_REQUIRE_DATABASE();
	execute("INSERT INTO player_recipes (player_id, recipe_id) VALUES (710101, 155090012), (710101, 155090009)");
	setRecipes({LIMITED_SUPPLEMENT_RECIPE, PISTOL_RECIPE});
	setSkills({{WEAPONSMITHING, 1}});

	finish(PISTOL_RECIPE);
	EXPECT_TRUE(knowsRecipe(PISTOL_RECIPE));
	EXPECT_TRUE(itemtest::packetsOf(sent(), SM_RECIPE_DELETE_OPCODE).empty());
	EXPECT_EQ(itemtest::packetsOf(sent(), SM_LEARN_RECIPE_OPCODE),
		cp::exactly({serializedFor(network::aion::serverpackets::SM_LEARN_RECIPE(STEEL_INGOT_RECIPE))}));
	clearSent();

	finish(LIMITED_SUPPLEMENT_RECIPE);
	EXPECT_FALSE(knowsRecipe(LIMITED_SUPPLEMENT_RECIPE));
	EXPECT_EQ(itemtest::packetsOf(sent(), SM_RECIPE_DELETE_OPCODE),
		cp::exactly({serializedFor(network::aion::serverpackets::SM_RECIPE_DELETE(LIMITED_SUPPLEMENT_RECIPE))}));
	EXPECT_EQ(storedRecipes(), (std::vector<int32_t>{STEEL_INGOT_RECIPE, PISTOL_RECIPE}));
	EXPECT_EQ(countOf(SIEGE_WEAPON_SUPPLEMENT), 1) << "the product all the same";
}

// :45-46: the limited recipe is forgotten whatever the crit count - only the quest engine's call (:47-50) waits for a craft without a crit.
// --recipe 155002239 --skill-level 500: afterCraft.recipeDeleted true, outcomes critCount 1 = 182206773 (Eremitia's Tasty Vegetable Dish),
// gainedSkillXp 2930
TEST_F(CraftServiceTest, ALimitedRecipeIsForgottenAfterACriticalCraftToo) {
	CRAFT_REQUIRE_DATABASE();
	execute("INSERT INTO player_recipes (player_id, recipe_id) VALUES (710101, 155002239)");
	setRecipes({VEGETABLE_DISH_RECIPE});
	setSkills({{COOKING, 500}});

	finish(VEGETABLE_DISH_RECIPE, 1);
	EXPECT_FALSE(knowsRecipe(VEGETABLE_DISH_RECIPE));
	EXPECT_EQ(itemtest::packetsOf(sent(), SM_RECIPE_DELETE_OPCODE),
		cp::exactly({serializedFor(network::aion::serverpackets::SM_RECIPE_DELETE(VEGETABLE_DISH_RECIPE))}));
	EXPECT_TRUE(storedRecipes().empty());
	EXPECT_EQ(countOf(TASTY_VEGETABLE_DISH), 1) << "the combo product";
	EXPECT_EQ(countOf(VEGETABLE_DISH), 0);
	EXPECT_EQ(skill(COOKING).getCurrentXp(), 2930);
}

// :47-50: a limited recipe crafted without a crit tells QuestEngine.onFailCraft of its first combo product, 0 for a recipe without one
// (getComboProduct(1) == null), with QuestEnv(null, player, 0); a crit tells it nothing. No quest handler is ported yet (the crafting quests are
// M5d's), so FailCraftProbe stands in for _19038MasterCooksPotential, which waits for Eremitia's Tasty Vegetable Dish (:26); it also waits for
// item 0 to see the null arm. The engine calls a handler only while the player holds none of the item (QuestEngine.onFailCraft).
TEST_F(CraftServiceTest, ALimitedRecipeCraftedWithoutACritTellsTheQuestEngineOfItsComboProduct) {
	FailCraftEngine engine;
	questEngine::QuestEngine::getInstance().addQuestHandler(std::make_unique<FailCraftProbe>(19038, std::vector<int32_t>{TASTY_VEGETABLE_DISH, 0}));
	setRecipes({VEGETABLE_DISH_RECIPE, LIMITED_SUPPLEMENT_RECIPE}); // without the database deleteRecipe forgets nothing (CraftTestSupport.h)
	setSkills({{COOKING, 500}, {WEAPONSMITHING, 30}});

	finish(VEGETABLE_DISH_RECIPE);
	EXPECT_EQ(FailCraftProbe::calls, (std::vector<std::pair<int32_t, int32_t>>{{19038, TASTY_VEGETABLE_DISH}}));
	EXPECT_EQ(countOf(VEGETABLE_DISH), 1);
	FailCraftProbe::calls.clear();

	finish(VEGETABLE_DISH_RECIPE, 1);
	EXPECT_TRUE(FailCraftProbe::calls.empty()) << "a crit";
	EXPECT_EQ(countOf(TASTY_VEGETABLE_DISH), 1);

	finish(LIMITED_SUPPLEMENT_RECIPE);
	EXPECT_EQ(FailCraftProbe::calls, (std::vector<std::pair<int32_t, int32_t>>{{19038, 0}})) << "a limited recipe without a combo product";
}

// :123 (m5c-plan.md C-01; oracle --recipe 155090009 --skill-level 20 --craft-type 0 / 1: gainedSkillXp and playerExp 141 / 162):
// startCrafting hands CraftingTask the bonus 15 for craft type 1 and 0 otherwise, which the finished task passes on to
// finishCrafting (CraftingTask.java:55). The task runs to its end on the fixture's executor with gameserver.craft.fail.chance 0 (every tick
// succeeds, CraftingTask.java:133); the pistol recipe has no combo product, so calculateCrit answers false (:61-62) before it asks for the
// player's house (:73). Needs C-02 (CraftingTask's bodies): skipped until then.
TEST_F(CraftServiceTest, CraftType1HandsTheTaskTheStonesFifteenPercent) {
	mail::ConfigScope<int32_t> noFailure(configs::main::CraftConfig::MAX_CRAFT_FAILURE_CHANCE, 0);
	const int32_t ovenId = station(3.0f).getObjectId();
	setRecipes({PISTOL_RECIPE});
	give(820061, WEAPONSMITHING_STONE, 1);
	struct Row {
		int32_t craftType;
		int32_t xp;
	};
	for (const Row& row : {Row{0, 141}, Row{1, 162}}) {
		SCOPED_TRACE(row.craftType);
		setSkills({{WEAPONSMITHING, 20}});
		const int64_t exp = commonData().getExp();
		if (pastCheckCraft(PISTOL_RECIPE, ovenId, row.craftType, {}) == PastCheckCraft::TASK_CONSTRUCTOR_UNPORTED)
			GTEST_SKIP() << "CraftingTask's constructor is C-02's (m5c-plan.md): pending until the craft-task lane merges";
		runCraftToItsEnd();
		EXPECT_EQ(skill(WEAPONSMITHING).getCurrentXp(), row.xp);
		EXPECT_EQ(commonData().getExp(), exp + row.xp);
	}
}

} // namespace
} // namespace aion::gameserver::economy::test::craft
