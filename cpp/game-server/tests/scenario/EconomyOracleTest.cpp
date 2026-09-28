// The binding of oracle.py m5c-economy (EconomyOracle.h; m5c-plan.md G-03's accessor, §18.4): the command line spelled as tools/oracle/oracle.py's
// argparse reads it, and the parse of an answer. The answer below is the real oracle's (oracle.py m5c-economy under the gate profile for 798007,
// 700000, 798008 and 203336, soul healing for 1,000 exp, a letter of five potions and 200 kinah, the Plainsman's Tunic and Sword with manastone
// 167000226 for a level-4 Mage, the level-10 Gladiator seed from old level 2 and recipe 155001381 at the Sanctum ovens 150000009), trimmed to
// the fields the binding reads, with a few lists shortened and the equip checks' `notModelled` text cut to its first clause. Every field the
// binding reads is asserted; where the real answer gives two sibling fields the same value, EachFieldIsReadFromItsOwnKey edits them apart, and
// NullsStayNull swaps in the refusal shapes of a second real answer (--recover-exp 0, --npc-expands 50, 167000290 and 167000226 on the Tunic, the
// [Event] Extraction Greatsword 100901051 and the Guardian's Belt 123000005). No server, no database and no Python interpreter - except
// OracleRunTest's case at the end, which runs the whole binding against the real oracle.py (labelled realdata by ScenarioTests.cmake's
// `^OracleRunTest\.` rule).

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "EconomyOracle.h"

namespace aion::gameserver::scenario {
namespace {

// the real oracle's answer, trimmed (see the header comment)
constexpr std::string_view GATE_ANSWER = R"json({"format":"aion-m5c-economy","version":1,"map":210010000,"prices":{"ELYOS":{"smPrices":[125,
100,113]},"ASMODIANS":{"smPrices":[125,100,113]}},"talk":[{"npcId":798007,"name":"minalinerk","canInteract":true,"talkDistance":5,
"limit":6.845000267028809,"limitWithoutPlusOne":5.845000267028809,"limitCenterToCenter":6.0,"chosenSpot":{"x":851.6710205078125,
"y":1252.6700439453125,"z":118.83300018310547,"distanceFromReference":0.0},"bandSpot":{"x":858.093505859375,"y":1252.6700439453125,
"z":118.83300018310547,"distance":6.4225,"inTalkRange":true,"inRangeWithoutPlusOne":false,"inRangeCenterToCenter":false,
"otherNpcsInTalkRange":[203336]},"nearSpot":{"x":853.6710205078125,"y":1252.6700439453125,"z":118.83300018310547,"distance":2.0,
"inTalkRange":true,"inRangeWithoutPlusOne":true,"inRangeCenterToCenter":true,"otherNpcsInTalkRange":[]},"farSpot":{"x":861.6710205078125,
"y":1252.6700439453125,"z":118.83300018310547,"distance":10.0,"inTalkRange":false,"inRangeWithoutPlusOne":false,
"inRangeCenterToCenter":false,"otherNpcsInTalkRange":[203336]},"outOfRange":{"message":"STR_DIALOG_TOO_FAR_TO_TALK","messageId":1300346,
"window":null},"startWindow":{"ai":"GeneralNpcAI","page":10,"questId":0,"pageValue":0},"functions":[{"action":2,"name":"BUY"},{"action":3,
"name":"SELL"}]},{"npcId":700000,"name":"mailbox","canInteract":true,"talkDistance":5,"limit":6.599999904632568,
"limitWithoutPlusOne":5.599999904632568,"limitCenterToCenter":6.0,"chosenSpot":{"x":827.2310180664062,"y":1243.2099609375,
"z":118.8759994506836,"distanceFromReference":26.207},"bandSpot":{"x":833.531005859375,"y":1243.2099609375,"z":118.8759994506836,
"distance":6.3,"inTalkRange":true,"inRangeWithoutPlusOne":false,"inRangeCenterToCenter":false,"otherNpcsInTalkRange":[]},
"nearSpot":{"x":829.2310180664062,"y":1243.2099609375,"z":118.8759994506836,"distance":2.0,"inTalkRange":true,"inRangeWithoutPlusOne":true,
"inRangeCenterToCenter":true,"otherNpcsInTalkRange":[]},"farSpot":{"x":837.2310180664062,"y":1243.2099609375,"z":118.8759994506836,
"distance":10.0,"inTalkRange":false,"inRangeWithoutPlusOne":false,"inRangeCenterToCenter":false,"otherNpcsInTalkRange":[]},
"outOfRange":{"message":"STR_WAREHOUSE_TOO_FAR_FROM_NPC","messageId":1300419,"window":null},"startWindow":{"ai":"PostboxAI","page":18,
"questId":0,"pageValue":1},"functions":[]}],"recovery":{"recoverableExp":1000,"price":249,"question":{"id":160011,"params":["249","",""],
"senderId":0,"range":0},"yes":{"kinahDelta":-249,"expDelta":1000,"recoverableExpAfter":0,"messages":[{"name":"STR_GET_EXP2","id":1370002,
"value":1000},{"name":"STR_SUCCESS_RECOVER_EXPERIENCE","id":1300674}]},"notEnoughKinah":{"name":"STR_MSG_NOT_ENOUGH_KINA","id":901285,
"value":249}},"cube":[{"npcId":798008,"answer":"SM_QUESTION_WINDOW","price":1000,"question":{"id":900686,"params":["1000","",""],
"senderId":0,"range":0},"yes":{"kinahDelta":-1000,"npcExpandsAfter":1,"cubeSlotsAdded":9,
"message":{"name":"STR_EXTEND_INVENTORY_SIZE_EXTENDED","id":1300431,"value":9},"smCubeUpdate":{"action":0,"storage":0,"npcExpands":1,
"questExpands":0,"itemExpands":0}},"notEnoughKinah":{"name":"STR_WAREHOUSE_EXPAND_NOT_ENOUGH_MONEY","id":1300831}}],
"manastoneRemoval":{"basePrice":650,"byRace":{"ELYOS":917,"ASMODIANS":917},"messages":{"succeed":1300473,"notEnoughKinah":1300472}},
"mail":[{"spec":"162000002:5:200","itemCommission":25,"kinahCommission":2,"serviceBase":37,"byRace":{"ELYOS":{"servicePrice":51,
"total":251},"ASMODIANS":{"servicePrice":51,"total":251}}}],"items":[{"itemId":110100355,"name":"Plainsman's Tunic","level":4,
"itemGroup":"RB_TORSO","equipType":"ARMOR","manastoneSlots":1,"maxEnchant":10,"identification":{"canTune":true,
"sqlDefaultLoadsIdentified":true,"seedTuneCountForUnidentified":-1,"optionalSocketsRange":[0,1],"enchantBonusRange":[0,0],
"messageId":1401626,"tuneCountAfter":0,"animation":{"time":5000,"start":9,"end":10,"abort":11}},"breakItem":{"breakable":true,
"stones":[{"itemId":166000191,"probability":1.0}],"countRange":[1,3],"message":"STR_DECOMPOSE_ITEM_SUCCEED","messageId":1300449},
"equip":{"passes":true,"refusedBy":null,"messageId":null,"requiredLevel":4,"startExpOfRequiredLevel":3820,"requiredSkills":[103,106],
"knownRequiredSkills":[103],"message":null,"maxLevelRestrict":0,"itemRace":"PC_ALL",
"notModelled":"gender, rank and the cube space of a two-handed weapon (Equipment.java:92-106, between the race and the skill checks)"},
"socketing":[{"stoneId":167000226,"canAct":true,"fits":true,"refusedBy":null,"slotLevel":20,"socketsRange":[1,2],
"successChance":205.7142791748047,"certain":true,"needsOptionalSocket":false,"impossible":false,"rate":200.0,
"success":{"message":"STR_GIVE_ITEM_OPTION_SUCCEED","messageId":1300462,"slot":0,"stoneConsumed":true},
"failure":{"message":"STR_GIVE_ITEM_OPTION_FAILED","messageId":1300463,"stoneConsumed":true},"animationMillis":2000}]},{"itemId":100000133,
"name":"Plainsman's Sword","level":2,"itemGroup":"SWORD","equipType":"WEAPON","manastoneSlots":1,"maxEnchant":10,
"identification":{"canTune":true,"sqlDefaultLoadsIdentified":true,"seedTuneCountForUnidentified":-1,"optionalSocketsRange":[0,1],
"enchantBonusRange":[0,0],"messageId":1401626,"tuneCountAfter":0,"animation":{"time":5000,"start":9,"end":10,"abort":11}},
"breakItem":{"breakable":true,"stones":[{"itemId":166000191,"probability":1.0}],"countRange":[2,5],"message":"STR_DECOMPOSE_ITEM_SUCCEED",
"messageId":1300449},"equip":{"passes":false,"refusedBy":"equipSkill","messageId":null,"requiredLevel":2,"startExpOfRequiredLevel":400,
"requiredSkills":[37,44],"knownRequiredSkills":[],"message":null,"maxLevelRestrict":0,"itemRace":"PC_ALL",
"notModelled":"gender, rank and the cube space of a two-handed weapon (Equipment.java:92-106, between the race and the skill checks)"},
"socketing":[{"stoneId":167000226,"canAct":true,"fits":true,"refusedBy":null,"slotLevel":20,"socketsRange":[1,2],
"successChance":205.7142791748047,"certain":true,"needsOptionalSocket":false,"impossible":false,"rate":200.0,
"success":{"message":"STR_GIVE_ITEM_OPTION_SUCCEED","messageId":1300462,"slot":0,"stoneConsumed":true},
"failure":{"message":"STR_GIVE_ITEM_OPTION_FAILED","messageId":1300463,"stoneConsumed":true},"animationMillis":2000}]}],
"character":{"class":"MAGE","race":"ELYOS","level":4,"learnedSkills":[40,100,103,243,245,302,1282,1328,1363,30001]},
"daeva":{"class":"GLADIATOR","startingClass":"WARRIOR","race":"ELYOS","seed":{"playerClass":"GLADIATOR","exp":126069,"quest":{"id":1006,
"status":"COMPLETE"},"oldLevel":2},"level":10,"levelWithoutQuest":9,"enterWorld":{"learnNewSkills":[3,10],"storedSkills":[37,39,30001],
"learnedSkills":[{"skillId":30003,"level":10,"class":"GLADIATOR"},{"skillId":40009,"level":10,"class":"GLADIATOR"},{"skillId":169,
"level":10,"class":"GLADIATOR"}],"daevaSwap":{"removed":30001,"added":30002,"level":1},"skills":[37,39,30002,30003,40009],
"learnedRecipes":[155000001,155000002,155000005]}},"craft":{"map":110010000,"recipe":{"id":155001381,"skillId":40001,
"components":[[152001001,1],[169400096,2]],"product":{"itemId":160001001,"quantity":2,"name":"Roast Inina","quality":"COMMON"},
"steps":{"fewest":4,"most":14,"mostToAnyEnd":14},"finishMillis":{"fewest":11000,"most":36000},"xpReward":141,"skillLevelAfter":2,
"playerExp":141,"timing":{"interval":2500,"firstTickDelay":1000}},"master":{"npcId":203784,"talk":{"npcId":203784,"name":"hestia",
"canInteract":true,"talkDistance":5,"limit":6.599999904632568,"limitWithoutPlusOne":5.599999904632568,"limitCenterToCenter":6.0,
"chosenSpot":{"x":1848.0699462890625,"y":1543.969970703125,"z":590.1580200195312,"distanceFromReference":0.0},
"bandSpot":{"x":1854.3699951171875,"y":1543.969970703125,"z":590.1580200195312,"distance":6.3,"inTalkRange":true,
"inRangeWithoutPlusOne":false,"inRangeCenterToCenter":false,"otherNpcsInTalkRange":[]},"nearSpot":{"x":1850.0699462890625,
"y":1543.969970703125,"z":590.1580200195312,"distance":2.0,"inTalkRange":true,"inRangeWithoutPlusOne":true,"inRangeCenterToCenter":true,)json"
	R"json("otherNpcsInTalkRange":[]},"farSpot":{"x":1858.0699462890625,"y":1543.969970703125,"z":590.1580200195312,"distance":10.0,
"inTalkRange":false,"inRangeWithoutPlusOne":false,"inRangeCenterToCenter":false,"otherNpcsInTalkRange":[]},
"outOfRange":{"message":"STR_DIALOG_TOO_FAR_TO_TALK","messageId":1300346,"window":null},"startWindow":{"ai":"GeneralNpcAI","page":10,
"questId":0,"pageValue":0},"functions":[{"action":46,"name":"COMBINE_SKILL_LEVELUP","question":900852},{"action":79,"name":null},
{"action":58,"name":null},{"action":80,"name":null}]}},"learn":{"dialogAction":46,"minCharacterLevel":10,"cost":3500,
"question":{"id":900852,"params":[{"l10nId":280383,"utf16":[36,36479,8]},"3500",""],"senderId":0,"range":0},
"yes":{"learnedRecipes":[155001381]},"supported":true,"notEnoughKinah":"STR_NOT_ENOUGH_MONEY"},"components":[{"itemId":152001001,
"quantity":1,"vendor":null,"vendors":[]},{"itemId":169400096,"quantity":2,"vendor":203785,"vendors":[{"npcId":203785,"kinah":140,
"distanceFromMaster":7.688,"talk":{"npcId":203785,"name":"luelas","canInteract":true,"talkDistance":5,"limit":6.599999904632568,
"limitWithoutPlusOne":5.599999904632568,"limitCenterToCenter":6.0,"chosenSpot":{"x":1843.469970703125,"y":1537.81005859375,
"z":590.1580200195312,"distanceFromReference":7.688},"bandSpot":{"x":1849.77001953125,"y":1537.81005859375,"z":590.1580200195312,
"distance":6.3,"inTalkRange":true,"inRangeWithoutPlusOne":false,"inRangeCenterToCenter":false,"otherNpcsInTalkRange":[203784]},
"nearSpot":{"x":1845.469970703125,"y":1537.81005859375,"z":590.1580200195312,"distance":2.0,"inTalkRange":true,"inRangeWithoutPlusOne":true,
"inRangeCenterToCenter":true,"otherNpcsInTalkRange":[]},"farSpot":{"x":1853.469970703125,"y":1537.81005859375,"z":590.1580200195312,
"distance":10.0,"inTalkRange":false,"inRangeWithoutPlusOne":false,"inRangeCenterToCenter":false,"otherNpcsInTalkRange":[]},
"outOfRange":{"message":"STR_DIALOG_TOO_FAR_TO_TALK","messageId":1300346,"window":null},"startWindow":{"ai":"GeneralNpcAI","page":10,
"questId":0,"pageValue":0},"functions":[{"action":2,"name":"BUY"},{"action":3,"name":"SELL"}]}}]}],"seedItems":[{"itemId":152001001,
"count":1}],"exactKinah":3640,"seedSpot":{"worldId":110010000,"x":1850.0699462890625,"y":1543.969970703125,"z":590.1580200195312},
"tool":{"templateId":150000009,"tools":[{"staticId":103,"x":1849.7879638671875,"y":1549.136962890625,"z":590.0260009765625,
"distanceFromMaster":5.447},{"staticId":118,"x":1846.031005859375,"y":1549.1419677734375,"z":590.02099609375,"distanceFromMaster":5.561}],
"chosen":{"staticId":103},"checkCraftRange":5.25,"packetRange":10.0,"spots":[{"distance":3.0,"x":1852.7879638671875,"y":1549.136962890625,
"z":590.0260009765625,"inPacketRange":true,"inCheckCraftRange":true,"outcome":"CraftingTask","otherToolsInCheckCraftRange":[104]},
{"distance":7.0,"x":1856.7879638671875,"y":1549.136962890625,"z":590.0260009765625,"inPacketRange":true,"inCheckCraftRange":false,
"outcome":"STR_COMBINE_TOO_FAR_FROM_TOOL and the cancel pair","otherToolsInCheckCraftRange":[104]},{"distance":12.0,"x":1861.7879638671875,
"y":1549.136962890625,"z":590.0260009765625,"inPacketRange":false,"inCheckCraftRange":false,"outcome":"nothing (CM_CRAFT returns)",
"otherToolsInCheckCraftRange":[]}]}}})json";

TEST(EconomyOracleTest, TheCommandLineSpellsEveryParameterAsOraclePyReadsIt) {
	// tools/oracle/oracle.py, the m5c-economy parser: one flag per value, the lists repeated per value in the caller's order
	EconomyRequest request;
	request.staticData = std::filesystem::path("data");
	request.noProfile = true;
	request.settings = {"gameserver.siege.enable=false", "gameserver.rates.manastone_chances=200, 200"};
	request.mapId = 210010000;
	request.npcIds = {798007, 700000};
	request.near = std::array<float, 3>{851.671f, 1252.67f, 118.833f};
	request.far = 10.5;
	request.direction = 90;
	request.recoverExp = 1000;
	request.npcExpands = 1;
	request.questExpands = 2;
	request.itemExpands = 3;
	request.mails = {"162000002:5:200", "0:0:10"};
	request.itemIds = {110100355, 100000133};
	request.manastones = {167000226};
	request.membership = 1;
	request.playerClass = "MAGE";
	request.race = "ASMODIANS";
	request.level = 4;
	request.influences = {"ELYOS=10"};
	request.daevaClass = "GLADIATOR";
	request.daevaOldLevel = 2;
	request.craftRecipe = 155001381;
	request.craftTool = 150000009;
	request.craftMap = 110010000;
	request.craftDistances = {3, 7.5};
	EXPECT_EQ(economyArguments(request),
		(std::vector<std::string>{"m5c-economy", "--static-data", "data", "--no-profile", "--set", "gameserver.siege.enable=false", "--set",
			"gameserver.rates.manastone_chances=200, 200", "--map", "210010000", "--npc", "798007", "--npc", "700000", "--near", "851.671,1252.67,118.833",
			"--far", "10.5", "--direction", "90", "--recover-exp", "1000", "--npc-expands", "1", "--quest-expands", "2", "--item-expands", "3", "--mail",
			"162000002:5:200", "--mail", "0:0:10", "--item", "110100355", "--item", "100000133", "--manastone", "167000226", "--membership", "1",
			"--class", "MAGE", "--race", "ASMODIANS", "--level", "4", "--influence", "ELYOS=10", "--daeva", "GLADIATOR", "--daeva-old-level", "2",
			"--craft-recipe", "155001381", "--craft-tool", "150000009", "--craft-map", "110010000", "--craft-distance", "3", "--craft-distance",
			"7.5"}));
	EXPECT_EQ(economyArguments(EconomyRequest{}), (std::vector<std::string>{"m5c-economy"})) << "nothing set, nothing sent: the oracle's defaults";

	EconomyRequest profile; // a named profile wins over --no-profile: argparse would take both, the oracle refuses nothing, so only one is sent
	profile.profile = std::filesystem::path("m5c.properties");
	profile.noProfile = true;
	EXPECT_EQ(economyArguments(profile), (std::vector<std::string>{"m5c-economy", "--profile", "m5c.properties"}));
	EconomyRequest spot; // the shortest text that reads back as the same float: nothing rounded to six decimals
	spot.near = std::array<float, 3>{858.0935f, 0.1f, -3.5f};
	EXPECT_EQ(economyArguments(spot), (std::vector<std::string>{"m5c-economy", "--near", "858.0935,0.1,-3.5"}));
}

/** the refusal shapes of a second real answer (see the header comment), trimmed as GATE_ANSWER */
constexpr std::string_view REFUSALS = R"json({
"recovery":{"recoverableExp":0,"question":null,"message":"STR_DONOT_HAVE_RECOVER_EXPERIENCE","messageId":1300682},
"cube":[{"npcId":798008,"answer":"STR_EXTEND_INVENTORY_CANT_EXTEND_MORE","messageId":1300430}],
"sword":{"itemId":100901051,"name":"[Event] Extraction Greatsword","level":60,"itemGroup":"EXTRACT_SWORD","equipType":"NONE","manastoneSlots":1,
"maxEnchant":15,"identification":{"canTune":false,"sqlDefaultLoadsIdentified":true,"seedTuneCountForUnidentified":null},
"breakItem":{"breakable":false},"equip":{"passes":false,"refusedBy":"class","message":"STR_CANNOT_USE_ITEM_INVALID_CLASS","messageId":1300371,
"requiredLevel":-1,"startExpOfRequiredLevel":null,"requiredSkills":[],"knownRequiredSkills":[],"maxLevelRestrict":0,"itemRace":"PC_ALL",
"notModelled":"gender, rank and the cube space of a two-handed weapon (Equipment.java:92-106, between the race and the skill checks)"},
"socketing":[{"stoneId":167000290,"canAct":true,"fits":false,"refusedBy":"noSocket","slotLevel":70,"socketsRange":[0,0],
"successChance":222.85714721679688,"rate":200.0,"messageId":1300463,"stoneConsumed":true,"auditLog":"Manastone socket overload"}]},
"stoneLevel":{"stoneId":167000290,"canAct":true,"fits":false,"refusedBy":"stoneLevel","slotLevel":20,"socketsRange":[1,2],
"successChance":194.2857208251953,"rate":200.0,"messageId":1300463,"stoneConsumed":true},
"canAct":{"stoneId":167000290,"canAct":false,"fits":false,"refusedBy":"canAct","messageId":null,"stoneConsumed":false}
})json";

/** one of a talk block's spots, field by field */
void expectSpot(const EconomySpot& spot, float x, float y, float z, double distance, bool inTalkRange, bool inRangeWithoutPlusOne,
                bool inRangeCenterToCenter, const std::vector<int32_t>& otherNpcsInTalkRange) {
	EXPECT_FLOAT_EQ(spot.x, x);
	EXPECT_FLOAT_EQ(spot.y, y);
	EXPECT_FLOAT_EQ(spot.z, z);
	EXPECT_DOUBLE_EQ(spot.distance, distance);
	EXPECT_EQ(spot.inTalkRange, inTalkRange);
	EXPECT_EQ(spot.inRangeWithoutPlusOne, inRangeWithoutPlusOne);
	EXPECT_EQ(spot.inRangeCenterToCenter, inRangeCenterToCenter);
	EXPECT_EQ(spot.otherNpcsInTalkRange, otherNpcsInTalkRange);
}

/** one function arm of a talk block */
void expectFunction(const EconomyFunction& function, int32_t action, const std::optional<std::string>& name, std::optional<int32_t> question) {
	EXPECT_EQ(function.action, action);
	EXPECT_EQ(function.name, name);
	EXPECT_FALSE(function.page) << "only REMOVE_ITEM_OPTION has a page";
	EXPECT_EQ(function.question, question);
}

TEST(EconomyOracleTest, TheTalkPricesAndServicesAreParsedAsTheOracleWritesThem) {
	const EconomyAnswer answer = parseEconomy(GATE_ANSWER);
	EXPECT_EQ(answer.mapId, 210010000);
	ASSERT_EQ(answer.smPrices.size(), 2u);
	EXPECT_EQ(answer.smPrices.at("ELYOS"), (std::array<int32_t, 3>{125, 100, 113})) << "X1";
	EXPECT_EQ(answer.smPrices.at("ASMODIANS"), (std::array<int32_t, 3>{125, 100, 113}));
	ASSERT_EQ(answer.talk.size(), 2u);
	const EconomyTalk& minalinerk = answer.talkOf(798007);
	EXPECT_EQ(minalinerk.npcId, 798007);
	EXPECT_EQ(minalinerk.name, "minalinerk");
	EXPECT_TRUE(minalinerk.canInteract);
	EXPECT_EQ(minalinerk.talkDistance, 5);
	EXPECT_FLOAT_EQ(minalinerk.limit, 6.845f) << "5 + 1 + 0.595 + 0.25";
	EXPECT_FLOAT_EQ(minalinerk.limitWithoutPlusOne, 5.845f);
	EXPECT_FLOAT_EQ(minalinerk.limitCenterToCenter, 6.0f);
	EXPECT_FLOAT_EQ(minalinerk.x, 851.671f);
	EXPECT_FLOAT_EQ(minalinerk.y, 1252.67f);
	EXPECT_FLOAT_EQ(minalinerk.z, 118.833f);
	EXPECT_EQ(minalinerk.distanceFromReference, 0.0);
	{
		SCOPED_TRACE("798007's band spot: X2's, only the + 1 admits it; C3: Seril is in range along +x");
		expectSpot(minalinerk.bandSpot, 858.0935f, 1252.67f, 118.833f, 6.4225, true, false, false, {203336});
	}
	{
		SCOPED_TRACE("798007's near spot");
		expectSpot(minalinerk.nearSpot, 853.671f, 1252.67f, 118.833f, 2.0, true, true, true, {});
	}
	{
		SCOPED_TRACE("798007's far spot");
		expectSpot(minalinerk.farSpot, 861.671f, 1252.67f, 118.833f, 10.0, false, false, false, {203336});
	}
	EXPECT_EQ(minalinerk.outOfRangeMessage, "STR_DIALOG_TOO_FAR_TO_TALK");
	EXPECT_EQ(minalinerk.outOfRangeMessageId, 1300346);
	ASSERT_TRUE(minalinerk.startWindow);
	EXPECT_EQ(minalinerk.startWindow->ai, "GeneralNpcAI");
	EXPECT_EQ(minalinerk.startWindow->page, 10);
	EXPECT_EQ(minalinerk.startWindow->questId, 0);
	EXPECT_EQ(minalinerk.startWindow->pageValue, 0);
	ASSERT_EQ(minalinerk.functions.size(), 2u);
	expectFunction(minalinerk.functions[0], 2, "BUY", std::nullopt);
	expectFunction(minalinerk.functions[1], 3, "SELL", std::nullopt);
	const EconomyTalk& postbox = answer.talkOf(700000);
	EXPECT_EQ(postbox.name, "mailbox");
	EXPECT_FLOAT_EQ(postbox.limit, 6.6f);
	EXPECT_FLOAT_EQ(postbox.x, 827.231f);
	EXPECT_EQ(postbox.distanceFromReference, 26.207) << "from 798007, the first --npc";
	EXPECT_DOUBLE_EQ(postbox.bandSpot.distance, 6.3);
	ASSERT_TRUE(postbox.startWindow);
	EXPECT_EQ(postbox.startWindow->ai, "PostboxAI");
	EXPECT_EQ(postbox.startWindow->page, 18);
	EXPECT_EQ(postbox.startWindow->pageValue, 1) << "X3: MAIL with REGULAR";
	EXPECT_EQ(postbox.outOfRangeMessage, "STR_WAREHOUSE_TOO_FAR_FROM_NPC") << "not an is_dialog npc";
	EXPECT_EQ(postbox.outOfRangeMessageId, 1300419);
	EXPECT_TRUE(postbox.functions.empty());
	EXPECT_THROW(answer.talkOf(1), std::out_of_range);

	// X15: the question, what yes sends, and the refusal without the kinah
	ASSERT_TRUE(answer.recovery);
	const EconomyRecovery& recovery = *answer.recovery;
	EXPECT_EQ(recovery.recoverableExp, 1000);
	EXPECT_EQ(recovery.price, 249);
	ASSERT_TRUE(recovery.question);
	EXPECT_EQ(recovery.question->id, 160011);
	EXPECT_EQ(recovery.question->params, (std::array<std::string, 3>{"249", "", ""}));
	EXPECT_TRUE(recovery.question->l10nParams.empty());
	EXPECT_EQ(recovery.question->senderId, 0);
	EXPECT_EQ(recovery.question->range, 0);
	EXPECT_FALSE(recovery.messageId) << "a question, not STR_DONOT_HAVE_RECOVER_EXPERIENCE";
	EXPECT_EQ(recovery.yesKinahDelta, -249);
	EXPECT_EQ(recovery.yesExpDelta, 1000);
	EXPECT_EQ(recovery.yesRecoverableExpAfter, 0);
	EXPECT_EQ(recovery.yesMessages, (std::vector<EconomyMessage>{{"STR_GET_EXP2", 1370002, 1000}, {"STR_SUCCESS_RECOVER_EXPERIENCE", 1300674, {}}}))
		<< "STR_GET_EXP2 with the exp first, the success message without a value second";
	EXPECT_EQ(recovery.notEnoughKinah, (EconomyMessage{"STR_MSG_NOT_ENOUGH_KINA", 901285, 249}));

	// X26: the cube expander's question and its yes
	ASSERT_EQ(answer.cube.size(), 1u);
	const EconomyCube& cube = answer.cube[0];
	EXPECT_EQ(cube.npcId, 798008);
	EXPECT_EQ(cube.answer, "SM_QUESTION_WINDOW");
	EXPECT_EQ(cube.price, 1000);
	ASSERT_TRUE(cube.question);
	EXPECT_EQ(cube.question->id, 900686);
	EXPECT_EQ(cube.question->params, (std::array<std::string, 3>{"1000", "", ""}));
	EXPECT_EQ(cube.npcExpandsAfter, 1);
	EXPECT_EQ(cube.yesKinahDelta, -1000);
	EXPECT_EQ(cube.cubeSlotsAdded, 9);
	EXPECT_EQ(cube.yesMessage, (EconomyMessage{"STR_EXTEND_INVENTORY_SIZE_EXTENDED", 1300431, 9}));
	EXPECT_EQ(cube.smCubeUpdate, (EconomyCubeUpdate{0, 0, 1, 0, 0}));
	EXPECT_EQ(cube.notEnoughKinah, (EconomyMessage{"STR_WAREHOUSE_EXPAND_NOT_ENOUGH_MONEY", 1300831, {}}));
	EXPECT_FALSE(cube.messageId);

	// X25: Seril's price and messages
	ASSERT_TRUE(answer.removalPrice);
	EXPECT_EQ(*answer.removalPrice, (std::map<std::string, int64_t>{{"ELYOS", 917}, {"ASMODIANS", 917}}));
	EXPECT_EQ(answer.removalBasePrice, 650);
	EXPECT_EQ(answer.removalSucceedMessageId, 1300473);
	EXPECT_EQ(answer.removalNotEnoughKinahMessageId, 1300472);

	// X13
	ASSERT_EQ(answer.mail.size(), 1u);
	const EconomyMail& mail = answer.mail[0];
	EXPECT_EQ(mail.spec, "162000002:5:200");
	EXPECT_EQ(mail.itemCommission, 25);
	EXPECT_EQ(mail.kinahCommission, 2);
	EXPECT_EQ(mail.serviceBase, 37);
	EXPECT_EQ(mail.byRace, (std::map<std::string, std::pair<int64_t, int64_t>>{{"ELYOS", {51, 251}}, {"ASMODIANS", {51, 251}}}));
}

TEST(EconomyOracleTest, TheItemsAndTheCharacterAreParsedAsTheOracleWritesThem) {
	const EconomyAnswer answer = parseEconomy(GATE_ANSWER);
	ASSERT_EQ(answer.items.size(), 2u);
	const EconomyItem& tunic = answer.item(110100355);
	EXPECT_EQ(tunic.itemId, 110100355);
	EXPECT_EQ(tunic.name, "Plainsman's Tunic");
	EXPECT_EQ(tunic.level, 4);
	EXPECT_EQ(tunic.itemGroup, "RB_TORSO");
	EXPECT_EQ(tunic.equipType, "ARMOR");
	EXPECT_EQ(tunic.manastoneSlots, 1);
	EXPECT_EQ(tunic.maxEnchant, 10);
	// X23, D5
	EXPECT_TRUE(tunic.canTune);
	EXPECT_TRUE(tunic.sqlDefaultLoadsIdentified) << "D5: the seed must write tune_count -1";
	EXPECT_EQ(tunic.seedTuneCountForUnidentified, -1);
	EXPECT_EQ(tunic.optionalSocketsRange, (std::array<int32_t, 2>{0, 1}));
	EXPECT_EQ(tunic.enchantBonusRange, (std::array<int32_t, 2>{0, 0}));
	EXPECT_EQ(tunic.identifyMessageId, 1401626);
	EXPECT_EQ(tunic.tuneCountAfter, 0);
	EXPECT_EQ(tunic.identifyAnimation, (EconomyAnimation{5000, 9, 10, 11}));
	// X27
	EXPECT_TRUE(tunic.breakable);
	EXPECT_EQ(tunic.breakStones, (std::vector<std::pair<int32_t, double>>{{166000191, 1.0}})) << "Alpha only";
	EXPECT_EQ(tunic.breakCountRange, (std::array<int32_t, 2>{1, 3})) << "an armour: Rnd.get(1, 3)";
	EXPECT_EQ(tunic.breakMessageId, 1300449);
	// C15
	EXPECT_TRUE(tunic.equipPasses) << "a level-4 Mage wears the robe piece";
	EXPECT_FALSE(tunic.equipRefusedBy);
	EXPECT_FALSE(tunic.equipMessage);
	EXPECT_FALSE(tunic.equipMessageId) << "null, not 0";
	EXPECT_EQ(tunic.maxLevelRestrict, 0);
	EXPECT_EQ(tunic.itemRace, "PC_ALL");
	EXPECT_EQ(tunic.equipNotModelled,
	          "gender, rank and the cube space of a two-handed weapon (Equipment.java:92-106, between the race and the skill checks)");
	EXPECT_EQ(tunic.requiredLevel, 4);
	EXPECT_EQ(tunic.startExpOfRequiredLevel, 3820);
	EXPECT_EQ(tunic.requiredSkills, (std::vector<int32_t>{103, 106}));
	EXPECT_EQ(tunic.knownRequiredSkills, (std::vector<int32_t>{103}));
	// C16, X24
	ASSERT_EQ(tunic.socketing.size(), 1u);
	const EconomySocketing& stone = tunic.socketing[0];
	EXPECT_EQ(stone.stoneId, 167000226);
	EXPECT_TRUE(stone.canAct);
	EXPECT_TRUE(stone.fits) << "the seeded manastone fits the seeded armour";
	EXPECT_FALSE(stone.refusedBy);
	EXPECT_EQ(stone.slotLevel, 20);
	EXPECT_EQ(stone.socketsRange, (std::array<int32_t, 2>{1, 2}));
	EXPECT_EQ(stone.successChance, 205.7142791748047f) << "200 + 10 / 1.75f in float arithmetic";
	EXPECT_EQ(stone.certain, true) << "D6's chance 200";
	EXPECT_EQ(stone.impossible, false);
	EXPECT_EQ(stone.needsOptionalSocket, false);
	EXPECT_EQ(stone.rate, 200.0f);
	EXPECT_FALSE(stone.messageId) << "a stone that fits has no refusal message";
	EXPECT_FALSE(stone.stoneConsumed);
	EXPECT_FALSE(stone.auditLog);
	EXPECT_EQ(stone.successMessageId, 1300462);
	EXPECT_EQ(stone.failureMessageId, 1300463);
	EXPECT_EQ(stone.animationMillis, 2000);

	const EconomyItem& sword = answer.item(100000133);
	EXPECT_EQ(sword.name, "Plainsman's Sword");
	EXPECT_EQ(sword.level, 2);
	EXPECT_EQ(sword.itemGroup, "SWORD");
	EXPECT_EQ(sword.equipType, "WEAPON");
	EXPECT_EQ(sword.breakStones, (std::vector<std::pair<int32_t, double>>{{166000191, 1.0}}));
	EXPECT_EQ(sword.breakCountRange, (std::array<int32_t, 2>{2, 5})) << "a weapon: Rnd.get(2, 5)";
	EXPECT_FALSE(sword.equipPasses);
	EXPECT_EQ(sword.equipRefusedBy, "equipSkill");
	EXPECT_FALSE(sword.equipMessage) << "a refusal without a packet";
	EXPECT_FALSE(sword.equipMessageId);
	EXPECT_EQ(sword.requiredLevel, 2);
	EXPECT_EQ(sword.startExpOfRequiredLevel, 400);
	EXPECT_EQ(sword.requiredSkills, (std::vector<int32_t>{37, 44}));
	EXPECT_TRUE(sword.knownRequiredSkills.empty());
	ASSERT_EQ(sword.socketing.size(), 1u);
	EXPECT_TRUE(sword.socketing[0].fits);
	EXPECT_THROW(answer.item(1), std::out_of_range);

	EXPECT_EQ(answer.characterClass, "MAGE");
	EXPECT_EQ(answer.characterRace, "ELYOS");
	EXPECT_EQ(answer.characterLevel, 4);
	ASSERT_TRUE(answer.learnedSkills);
	EXPECT_EQ(*answer.learnedSkills, (std::vector<int32_t>{40, 100, 103, 243, 245, 302, 1282, 1328, 1363, 30001}));
}

TEST(EconomyOracleTest, TheC19BlocksAreParsedAsTheOracleWritesThem) {
	const EconomyAnswer answer = parseEconomy(GATE_ANSWER);
	ASSERT_TRUE(answer.daeva);
	const EconomyDaeva& daeva = *answer.daeva;
	EXPECT_EQ(daeva.playerClass, "GLADIATOR");
	EXPECT_EQ(daeva.startingClass, "WARRIOR");
	EXPECT_EQ(daeva.race, "ELYOS");
	EXPECT_EQ(daeva.exp, 126069);
	EXPECT_EQ(daeva.questId, 1006);
	EXPECT_EQ(daeva.questStatus, "COMPLETE");
	EXPECT_EQ(daeva.oldLevel, 2);
	EXPECT_EQ(daeva.level, 10);
	EXPECT_EQ(daeva.levelWithoutQuest, 9) << "F-1";
	EXPECT_EQ(daeva.learnNewSkills, (std::array<int32_t, 2>{3, 10}));
	EXPECT_EQ(daeva.storedSkills, (std::vector<int32_t>{37, 39, 30001})) << "trimmed: the level-1-2 Warrior's skills";
	ASSERT_EQ(daeva.learnedSkills.size(), 3u);
	const std::array<int32_t, 3> learnedIds{30003, 40009, 169};
	for (size_t i = 0; i < learnedIds.size(); i++) {
		EXPECT_EQ(daeva.learnedSkills[i].skillId, learnedIds[i]);
		EXPECT_EQ(daeva.learnedSkills[i].level, 10) << "the level whose skill_tree row taught it";
		EXPECT_EQ(daeva.learnedSkills[i].playerClass, "GLADIATOR");
	}
	EXPECT_EQ(daeva.swapRemoved, 30001);
	EXPECT_EQ(daeva.swapAdded, 30002);
	EXPECT_EQ(daeva.skills, (std::vector<int32_t>{37, 39, 30002, 30003, 40009})) << "X21a: the burst's SM_SKILL_LIST (trimmed)";
	EXPECT_EQ(daeva.learnedRecipes, (std::vector<int32_t>{155000001, 155000002, 155000005})) << "X21a";

	ASSERT_TRUE(answer.craft);
	const EconomyCraft& craft = *answer.craft;
	EXPECT_EQ(craft.mapId, 110010000);
	EXPECT_EQ(craft.recipeId, 155001381);
	EXPECT_EQ(craft.skillId, 40001);
	EXPECT_EQ(craft.recipeComponents, (std::vector<std::pair<int32_t, int64_t>>{{152001001, 1}, {169400096, 2}}));
	EXPECT_EQ(craft.productId, 160001001);
	EXPECT_EQ(craft.productQuantity, 2);
	EXPECT_EQ(craft.fewestSteps, 4) << "X20";
	EXPECT_EQ(craft.mostSteps, 14);
	EXPECT_EQ(craft.fewestMillis, 11000);
	EXPECT_EQ(craft.mostMillis, 36000);
	EXPECT_EQ(craft.interval, 2500);
	EXPECT_EQ(craft.firstTickDelay, 1000);
	EXPECT_EQ(craft.xpReward, 141) << "X19";
	EXPECT_EQ(craft.playerExp, 141);
	EXPECT_EQ(craft.skillLevelAfter, 2);
	// X17: Hestia, COMBINE_SKILL_LEVELUP's question
	EXPECT_EQ(craft.masterNpcId, 203784);
	EXPECT_EQ(craft.master.npcId, 203784);
	EXPECT_EQ(craft.master.name, "hestia");
	EXPECT_FLOAT_EQ(craft.master.x, 1848.07f);
	EXPECT_FLOAT_EQ(craft.master.y, 1543.97f);
	EXPECT_FLOAT_EQ(craft.master.z, 590.158f);
	ASSERT_EQ(craft.master.functions.size(), 4u);
	expectFunction(craft.master.functions[0], 46, "COMBINE_SKILL_LEVELUP", 900852);
	expectFunction(craft.master.functions[1], 79, std::nullopt, std::nullopt);
	expectFunction(craft.master.functions[2], 58, std::nullopt, std::nullopt);
	expectFunction(craft.master.functions[3], 80, std::nullopt, std::nullopt);
	EXPECT_EQ(craft.dialogAction, 46);
	EXPECT_EQ(craft.minCharacterLevel, 10);
	EXPECT_EQ(craft.learnCost, 3500) << "X17";
	EXPECT_TRUE(craft.learnSupported) << "Hestia's func_dialogs hold 46";
	EXPECT_EQ(craft.learnQuestion.id, 900852);
	ASSERT_EQ(craft.learnQuestion.l10nParams.size(), 1u) << "the profession name is ChatUtil.l10n(280383)";
	constexpr uint32_t code = 280383u << 1 | 1u;
	EXPECT_EQ(craft.learnQuestion.l10nParams.at(0), (std::array<uint16_t, 3>{36, code & 0xFFFF, code >> 16}));
	EXPECT_EQ(craft.learnQuestion.params, (std::array<std::string, 3>{"", "3500", ""})) << "the l10n parameter is in l10nParams only";
	EXPECT_EQ(craft.learnQuestion.senderId, 0);
	EXPECT_EQ(craft.learnQuestion.range, 0);
	EXPECT_EQ(craft.recipesLearnedWithTheSkill, (std::vector<int32_t>{155001381}));
	EXPECT_EQ(craft.learnNotEnoughKinah, "STR_NOT_ENOUGH_MONEY");
	// C19: the components, Luelas and the Inina seed
	ASSERT_EQ(craft.components.size(), 2u);
	EXPECT_EQ(craft.components[0].itemId, 152001001);
	EXPECT_EQ(craft.components[0].quantity, 1);
	EXPECT_FALSE(craft.components[0].vendor) << "no vendor sells Inina: a seed item";
	EXPECT_TRUE(craft.components[0].vendors.empty());
	EXPECT_EQ(craft.components[1].itemId, 169400096);
	EXPECT_EQ(craft.components[1].quantity, 2);
	EXPECT_EQ(craft.components[1].vendor, 203785);
	ASSERT_EQ(craft.components[1].vendors.size(), 1u);
	const EconomyVendor& luelas = craft.components[1].vendors[0];
	EXPECT_EQ(luelas.npcId, 203785);
	EXPECT_EQ(luelas.kinah, 140);
	EXPECT_EQ(luelas.distanceFromMaster, 7.688);
	EXPECT_EQ(luelas.talk.npcId, 203785);
	EXPECT_EQ(luelas.talk.name, "luelas");
	EXPECT_EQ(luelas.talk.distanceFromReference, 7.688) << "from Hestia";
	EXPECT_EQ(luelas.talk.bandSpot.otherNpcsInTalkRange, (std::vector<int32_t>{203784}));
	EXPECT_EQ(craft.seedItems, (std::vector<std::pair<int32_t, int64_t>>{{152001001, 1}}));
	EXPECT_EQ(craft.exactKinah, 3640) << "C19";
	EXPECT_EQ(craft.seedWorldId, 110010000);
	EXPECT_FLOAT_EQ(craft.seedX, 1850.07f) << "Hestia's near spot";
	EXPECT_FLOAT_EQ(craft.seedY, 1543.97f);
	EXPECT_FLOAT_EQ(craft.seedZ, 590.158f);
	// X18, X19: the ovens and the spots
	EXPECT_EQ(craft.toolTemplateId, 150000009);
	ASSERT_EQ(craft.tools.size(), 2u);
	EXPECT_EQ(craft.tools[0].staticId, 103);
	EXPECT_FLOAT_EQ(craft.tools[0].x, 1849.788f);
	EXPECT_FLOAT_EQ(craft.tools[0].y, 1549.137f);
	EXPECT_FLOAT_EQ(craft.tools[0].z, 590.026f);
	EXPECT_EQ(craft.tools[0].distanceFromMaster, 5.447);
	EXPECT_EQ(craft.tools[1].staticId, 118);
	EXPECT_EQ(craft.tools[1].distanceFromMaster, 5.561);
	EXPECT_EQ(craft.chosenToolStaticId, 103);
	EXPECT_FLOAT_EQ(craft.checkCraftRange, 5.25f);
	EXPECT_FLOAT_EQ(craft.packetRange, 10.0f);
	ASSERT_EQ(craft.spots.size(), 3u);
	const std::array<double, 3> distances{3, 7, 12};
	const std::array<float, 3> xs{1852.788f, 1856.788f, 1861.788f};
	const std::array<bool, 3> inPacket{true, true, false}, inCheck{true, false, false};
	const std::array<const char*, 3> outcomes{"CraftingTask", "STR_COMBINE_TOO_FAR_FROM_TOOL and the cancel pair", "nothing (CM_CRAFT returns)"};
	const std::array<std::vector<int32_t>, 3> otherTools{std::vector<int32_t>{104}, std::vector<int32_t>{104}, std::vector<int32_t>{}};
	for (size_t i = 0; i < craft.spots.size(); i++) {
		SCOPED_TRACE("the spot at " + std::to_string(distances[i]) + " m");
		EXPECT_EQ(craft.spots[i].distance, distances[i]);
		EXPECT_FLOAT_EQ(craft.spots[i].x, xs[i]);
		EXPECT_FLOAT_EQ(craft.spots[i].y, 1549.137f);
		EXPECT_FLOAT_EQ(craft.spots[i].z, 590.026f);
		EXPECT_EQ(craft.spots[i].inPacketRange, inPacket[i]);
		EXPECT_EQ(craft.spots[i].inCheckCraftRange, inCheck[i]);
		EXPECT_EQ(craft.spots[i].outcome, outcomes[i]);
		EXPECT_EQ(craft.spots[i].otherToolsInCheckCraftRange, otherTools[i]) << "oven 104 lies within 5.25 m of the 3 and 7 m spots too";
	}
}

TEST(EconomyOracleTest, EachFieldIsReadFromItsOwnKey) {
	// the real answer gives these siblings the same value; edited apart, each field must follow its own key (the edits make the answer
	// inconsistent on purpose: this is about the binding's key names only)
	nlohmann::json answer = nlohmann::json::parse(GATE_ANSWER);
	answer["craft"]["recipe"]["playerExp"] = 142;                                // xpReward 141
	answer["craft"]["recipe"]["skillLevelAfter"] = 3;                            // product quantity 2
	answer["items"][0]["equip"]["requiredLevel"] = 5;                            // the item's level 4
	answer["items"][0]["identification"]["sqlDefaultLoadsIdentified"] = false;   // canTune true
	answer["items"][0]["socketing"][0]["fits"] = false;                          // canAct true
	answer["items"][0]["socketing"][0]["impossible"] = true;                     // needsOptionalSocket false
	answer["talk"][0]["startWindow"]["questId"] = 7;                             // pageValue 0
	answer["talk"][0]["nearSpot"]["inRangeWithoutPlusOne"] = false;              // inRangeCenterToCenter true
	answer["recovery"]["question"]["senderId"] = 3;                              // range 0
	answer["recovery"]["yes"]["expDelta"] = 1001;                                // recoverableExp 1000
	answer["recovery"]["notEnoughKinah"]["value"] = 250;                         // price 249
	answer["cube"][0]["yes"]["smCubeUpdate"]["npcExpands"] = 2;                  // yes.npcExpandsAfter 1
	const EconomyAnswer parsed = parseEconomy(answer.dump());
	EXPECT_EQ(parsed.craft->playerExp, 142);
	EXPECT_EQ(parsed.craft->xpReward, 141);
	EXPECT_EQ(parsed.craft->skillLevelAfter, 3);
	EXPECT_EQ(parsed.craft->productQuantity, 2);
	EXPECT_EQ(parsed.items[0].requiredLevel, 5);
	EXPECT_EQ(parsed.items[0].level, 4);
	EXPECT_FALSE(parsed.items[0].sqlDefaultLoadsIdentified);
	EXPECT_TRUE(parsed.items[0].canTune);
	EXPECT_FALSE(parsed.items[0].socketing[0].fits);
	EXPECT_TRUE(parsed.items[0].socketing[0].canAct);
	EXPECT_EQ(parsed.items[0].socketing[0].impossible, true);
	EXPECT_EQ(parsed.items[0].socketing[0].needsOptionalSocket, false);
	EXPECT_EQ(parsed.talk[0].startWindow->questId, 7);
	EXPECT_EQ(parsed.talk[0].startWindow->pageValue, 0);
	EXPECT_FALSE(parsed.talk[0].nearSpot.inRangeWithoutPlusOne);
	EXPECT_TRUE(parsed.talk[0].nearSpot.inRangeCenterToCenter);
	EXPECT_EQ(parsed.recovery->question->senderId, 3);
	EXPECT_EQ(parsed.recovery->question->range, 0);
	EXPECT_EQ(parsed.recovery->yesExpDelta, 1001);
	EXPECT_EQ(parsed.recovery->recoverableExp, 1000);
	EXPECT_EQ(parsed.recovery->notEnoughKinah->value, 250);
	EXPECT_EQ(parsed.recovery->price, 249);
	EXPECT_EQ(parsed.cube[0].smCubeUpdate->npcExpands, 2);
	EXPECT_EQ(parsed.cube[0].npcExpandsAfter, 1);
}

TEST(EconomyOracleTest, NullsStayNull) {
	const nlohmann::json refusals = nlohmann::json::parse(REFUSALS);
	nlohmann::json answer = nlohmann::json::parse(GATE_ANSWER);
	answer["recovery"] = refusals["recovery"];
	answer["cube"] = refusals["cube"];
	answer["items"][1] = refusals["sword"];
	answer["items"][0]["socketing"] = nlohmann::json::array({refusals["stoneLevel"], refusals["canAct"]});
	answer["manastoneRemoval"] = nullptr;
	answer["daeva"] = nullptr;
	answer["craft"] = nullptr;
	answer["character"]["learnedSkills"] = nullptr;
	answer["talk"][0]["outOfRange"] = nullptr;
	answer["talk"][0]["startWindow"]["page"] = nullptr;
	answer["talk"][1]["startWindow"] = nullptr;
	const EconomyAnswer parsed = parseEconomy(answer.dump());
	// X15 without recoverable exp: the message, no question and no yes
	ASSERT_TRUE(parsed.recovery);
	EXPECT_EQ(parsed.recovery->recoverableExp, 0);
	EXPECT_FALSE(parsed.recovery->price);
	EXPECT_FALSE(parsed.recovery->question);
	EXPECT_EQ(parsed.recovery->messageId, 1300682) << "STR_DONOT_HAVE_RECOVER_EXPERIENCE";
	EXPECT_FALSE(parsed.recovery->yesKinahDelta || parsed.recovery->yesExpDelta || parsed.recovery->yesRecoverableExpAfter);
	EXPECT_TRUE(parsed.recovery->yesMessages.empty());
	EXPECT_FALSE(parsed.recovery->notEnoughKinah);
	// X26 past the limit: the refusal's message, nothing else
	ASSERT_EQ(parsed.cube.size(), 1u);
	const EconomyCube& full = parsed.cube[0];
	EXPECT_EQ(full.answer, "STR_EXTEND_INVENTORY_CANT_EXTEND_MORE");
	EXPECT_EQ(full.messageId, 1300430);
	EXPECT_FALSE(full.price || full.question || full.npcExpandsAfter || full.yesKinahDelta || full.cubeSlotsAdded || full.yesMessage ||
	             full.smCubeUpdate || full.notEnoughKinah);
	// the stone above the slot level (X24's refusal) and canAct's refusal
	const EconomySocketing& tooHigh = parsed.items[0].socketing[0];
	EXPECT_EQ(tooHigh.refusedBy, "stoneLevel");
	EXPECT_EQ(tooHigh.messageId, 1300463) << "STR_GIVE_ITEM_OPTION_FAILED after the 2 s";
	EXPECT_EQ(tooHigh.stoneConsumed, true);
	EXPECT_FALSE(tooHigh.certain || tooHigh.impossible || tooHigh.needsOptionalSocket || tooHigh.successMessageId || tooHigh.failureMessageId ||
	             tooHigh.animationMillis || tooHigh.auditLog);
	EXPECT_EQ(tooHigh.rate, 200.0f);
	const EconomySocketing& refused = parsed.items[0].socketing[1];
	EXPECT_FALSE(refused.canAct);
	EXPECT_EQ(refused.refusedBy, "canAct");
	EXPECT_FALSE(refused.messageId) << "canAct's refusal sends nothing";
	EXPECT_EQ(refused.stoneConsumed, false) << "and keeps the stone";
	EXPECT_FALSE(refused.slotLevel || refused.socketsRange || refused.successChance || refused.rate || refused.certain);
	// an item that cannot be tuned, broken or socketed, refused by its class
	const EconomyItem& sword = parsed.item(100901051);
	EXPECT_EQ(sword.equipType, "NONE");
	EXPECT_FALSE(sword.canTune);
	EXPECT_FALSE(sword.seedTuneCountForUnidentified || sword.optionalSocketsRange || sword.enchantBonusRange || sword.identifyMessageId ||
	             sword.tuneCountAfter || sword.identifyAnimation);
	EXPECT_FALSE(sword.breakable);
	EXPECT_TRUE(sword.breakStones.empty());
	EXPECT_FALSE(sword.breakCountRange || sword.breakMessageId);
	EXPECT_EQ(sword.equipRefusedBy, "class");
	EXPECT_EQ(sword.equipMessage, "STR_CANNOT_USE_ITEM_INVALID_CLASS");
	EXPECT_EQ(sword.equipMessageId, 1300371);
	EXPECT_EQ(sword.requiredLevel, -1);
	EXPECT_FALSE(sword.startExpOfRequiredLevel) << "no required level for the class";
	ASSERT_EQ(sword.socketing.size(), 1u);
	EXPECT_EQ(sword.socketing[0].refusedBy, "noSocket") << "Item.getSockets: 0 for an item that is no weapon and no armour";
	EXPECT_EQ(sword.socketing[0].socketsRange, (std::array<int32_t, 2>{0, 0}));
	EXPECT_EQ(sword.socketing[0].auditLog, "Manastone socket overload");
	// the blocks the request did not ask for
	EXPECT_FALSE(parsed.removalPrice || parsed.removalBasePrice || parsed.removalSucceedMessageId || parsed.removalNotEnoughKinahMessageId)
		<< "no REMOVE_ITEM_OPTION npc: null, not a price of 0";
	EXPECT_FALSE(parsed.daeva);
	EXPECT_FALSE(parsed.craft);
	EXPECT_FALSE(parsed.learnedSkills);
	EXPECT_FALSE(parsed.talk[0].outOfRangeMessage || parsed.talk[0].outOfRangeMessageId) << "an npc without talk_info answers nothing out of range";
	ASSERT_TRUE(parsed.talk[0].startWindow);
	EXPECT_FALSE(parsed.talk[0].startWindow->page) << "a dialog npc without functions: the page depends on the player";
	EXPECT_FALSE(parsed.talk[1].startWindow);

	nlohmann::json other = nlohmann::json::parse(GATE_ANSWER);
	other["format"] = "aion-m5c-trade";
	EXPECT_THROW(parseEconomy(other.dump()), std::runtime_error) << "an answer of another oracle";
	nlohmann::json twoParams = nlohmann::json::parse(GATE_ANSWER);
	twoParams["recovery"]["question"]["params"] = {"249", ""};
	EXPECT_THROW(parseEconomy(twoParams.dump()), std::runtime_error) << "SM_QUESTION_WINDOW writes three parameters";
}

TEST(OracleRunTest, TheEconomyBindingAsksTheRealOracleWhatItWasGiven) {
	std::optional<Oracle> oracle = Oracle::fromEnvironment(std::filesystem::path(AION_SCENARIO_OUTPUT_DIR) / "selftest" / "oracle");
	if (!oracle)
		GTEST_SKIP() << "no Python interpreter for tools/oracle: set AION_TEST_PYTHON (ctest sets the configure-time one)";

	// the gate's own request (m5c-plan.md §10.1 profile keys, C3, C11, C13-C19), every list parameter with more than one value
	EconomyRequest request;
	request.noProfile = true;
	request.settings = {"gameserver.siege.enable=false", "gameserver.event.service.disabled_events=*", "gameserver.craft.fail.chance=0",
		"gameserver.rates.crafting.crit_chances=0, 0", "gameserver.rates.manastone_chances=200, 200"};
	request.npcIds = {798007, 700000, 203336, 203064, 798008};
	request.direction = 270;
	request.recoverExp = 1000;
	request.mails = {"162000002:5:200", "0:0:10"};
	request.itemIds = {110100355, 100000133};
	request.manastones = {167000226};
	request.playerClass = "MAGE";
	request.level = 4;
	request.daevaClass = "GLADIATOR";
	request.daevaOldLevel = 2;
	request.craftRecipe = 155001381;
	request.craftTool = 150000009;
	request.craftDistances = {3, 7, 12};
	const EconomyAnswer answer = runEconomy(*oracle, request);
	EXPECT_EQ(answer.talk.size(), 5u) << "one block per --npc";
	EXPECT_TRUE(answer.talkOf(798007).bandSpot.inTalkRange);
	EXPECT_FALSE(answer.talkOf(798007).bandSpot.inRangeWithoutPlusOne);
	EXPECT_LT(answer.talkOf(798007).bandSpot.y, answer.talkOf(798007).y) << "--direction 270 puts the spots at -y";
	EXPECT_EQ(answer.recovery->price, 249);
	EXPECT_EQ(answer.cube.at(0).price, 1000);
	EXPECT_EQ(answer.removalPrice->at("ELYOS"), 917);
	ASSERT_EQ(answer.mail.size(), 2u);
	EXPECT_EQ(answer.mail[0].byRace.at("ELYOS").second, 251);
	EXPECT_EQ(answer.mail[1].byRace.at("ELYOS").second, 23) << "the second --mail";
	EXPECT_EQ(answer.item(110100355).socketing.at(0).certain, true) << "--manastone with the --set rate 200";
	EXPECT_EQ(answer.item(100000133).breakCountRange, (std::array<int32_t, 2>{2, 5}));
	EXPECT_TRUE(answer.item(110100355).equipPasses) << "--class MAGE --level 4";
	ASSERT_TRUE(answer.daeva);
	EXPECT_EQ(answer.daeva->learnNewSkills, (std::array<int32_t, 2>{3, 10})) << "--daeva-old-level 2";
	ASSERT_TRUE(answer.craft);
	EXPECT_EQ(answer.craft->exactKinah, 3640);
	ASSERT_EQ(answer.craft->spots.size(), 3u);
	EXPECT_FALSE(answer.craft->spots[2].inPacketRange) << "--craft-distance 12";
}

} // namespace
} // namespace aion::gameserver::scenario
