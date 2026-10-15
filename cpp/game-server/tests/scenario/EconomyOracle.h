#pragma once

// oracle.py m5c-economy (tools/oracle/m5c/economy.py and m5c/sanctum.py; m5c-plan.md G-01, §18.4) for the M5c gate (G-03): the request, spelled
// as oracle.py's argparse spells it, and the fields of the answer the gate asserts (§10.3 X2-X3, X13, X15, X17-X21a, X23-X27, C3, C14-C19).
// The answer is parsed by field name from the JSON the oracle writes; a value the oracle writes as null (what it does not model, or an arm
// that does not apply) stays std::nullopt here and never becomes a 0. Runs go through Oracle::run (Oracle.h), so the interpreter, the working
// directory and the captured stdout/stderr are the other oracles'.

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Oracle.h"

namespace aion::gameserver::scenario {

/** oracle.py m5c-economy's parameters (tools/oracle/oracle.py, the m5c-economy parser); every optional one is left off the command line unset */
struct EconomyRequest {
	std::optional<std::filesystem::path> staticData;
	/** --profile FILE (a file named here must exist), else --no-profile when noProfile, else the oracle's default config/mygs.properties */
	std::optional<std::filesystem::path> profile;
	bool noProfile = false;
	/** --set KEY=VALUE, one per entry, e.g. the gate profile's keys (game-server/config/m5c.properties.example) */
	std::vector<std::string> settings;
	std::optional<int32_t> mapId;
	/** --npc, in order: the first one's spot is the reference the others' spots are chosen by */
	std::vector<int32_t> npcIds;
	std::optional<std::array<float, 3>> near;
	std::optional<double> far;
	std::optional<double> direction;
	std::optional<int64_t> recoverExp;
	std::optional<int32_t> npcExpands, questExpands, itemExpands;
	/** --mail ITEM:COUNT:KINAH[:express] */
	std::vector<std::string> mails;
	std::vector<int32_t> itemIds;
	std::vector<int32_t> manastones;
	std::optional<int32_t> membership;
	std::optional<std::string> playerClass;
	std::optional<std::string> race;
	std::optional<int32_t> level;
	/** --influence RACE=N (sieges on only) */
	std::vector<std::string> influences;
	/** the C19 blocks: --daeva CLASS [--daeva-old-level N] and --craft-recipe ID --craft-tool ID [--craft-map ID] [--craft-distance D ...] */
	std::optional<std::string> daevaClass;
	std::optional<int32_t> daevaOldLevel;
	std::optional<int32_t> craftRecipe;
	std::optional<int32_t> craftTool;
	std::optional<int32_t> craftMap;
	std::vector<double> craftDistances;
};

/** the command line m5c-economy gets after the script path: the sub command, then one flag per set parameter, lists repeated per value */
std::vector<std::string> economyArguments(const EconomyRequest& request);

/** a system message as the oracle writes it: {"name", "id"} and, for a message with a parameter, its "value" */
struct EconomyMessage {
	std::string name;
	int32_t id = 0;
	std::optional<int64_t> value;

	bool operator==(const EconomyMessage&) const = default;
};

/** a point the gate may stand at, with the ranges it lies in (economy.talk_block's bandSpot, nearSpot and farSpot) */
struct EconomySpot {
	float x = 0, y = 0, z = 0;
	double distance = 0;
	bool inTalkRange = false, inRangeWithoutPlusOne = false, inRangeCenterToCenter = false;
	/** the other reported npcs this spot is in talk range of (C3: pick a --direction that leaves this empty) */
	std::vector<int32_t> otherNpcsInTalkRange;
};

/** the window DIALOG_START opens (SM_DIALOG_WINDOW's fields) */
struct EconomyStartWindow {
	std::string ai;
	/** null for a dialog npc without functions (10, 1011 or 1352 by player state) */
	std::optional<int32_t> page;
	int32_t questId = 0;
	int32_t pageValue = 0;
};

/** one function arm of a talk block */
struct EconomyFunction {
	int32_t action = 0;
	/** the DialogAction name, null for an arm the oracle does not model */
	std::optional<std::string> name;
	/** the SM_DIALOG_WINDOW page of REMOVE_ITEM_OPTION */
	std::optional<int32_t> page;
	/** the SM_QUESTION_WINDOW id of RECOVERY, EXTEND_INVENTORY, COMBINE_SKILL_LEVELUP */
	std::optional<int32_t> question;
};

/** one entry of `talk` (X2, X3, X25, C3) */
struct EconomyTalk {
	int32_t npcId = 0;
	std::string name;
	bool canInteract = false;
	int32_t talkDistance = 0;
	/** isInTalkRange's range with both bound radii, the one without the "+ 1", and the centre-to-centre one */
	float limit = 0, limitWithoutPlusOne = 0, limitCenterToCenter = 0;
	/** the npc's spot the band, near and far spots are measured from */
	float x = 0, y = 0, z = 0;
	/** metres from the reference spot (the first --npc's), null for the reference npc's own computation */
	std::optional<double> distanceFromReference;
	EconomySpot bandSpot, nearSpot, farSpot;
	/** the message outside the range (STR_DIALOG_TOO_FAR_TO_TALK or STR_WAREHOUSE_TOO_FAR_FROM_NPC), null for an npc that cannot interact */
	std::optional<std::string> outOfRangeMessage;
	std::optional<int32_t> outOfRangeMessageId;
	std::optional<EconomyStartWindow> startWindow;
	std::vector<EconomyFunction> functions;
};

/** SM_QUESTION_WINDOW as the oracle predicts it */
struct EconomyQuestion {
	int32_t id = 0;
	/** the three writeS parameters as text; a parameter the oracle gives as an l10n id is in `l10nParams` instead and empty here */
	std::array<std::string, 3> params;
	/** a parameter written as ChatUtil.l10n: its index and the three UTF-16 code units ('$', low and high half of `id << 1 | 1`) */
	std::map<size_t, std::array<uint16_t, 3>> l10nParams;
	int32_t senderId = 0;
	int32_t range = 0;
};

/** `recovery` (X15) */
struct EconomyRecovery {
	int64_t recoverableExp = 0;
	/** null with no recoverable exp (STR_DONOT_HAVE_RECOVER_EXPERIENCE, no question) */
	std::optional<int64_t> price;
	std::optional<EconomyQuestion> question;
	/** STR_DONOT_HAVE_RECOVER_EXPERIENCE's id without recoverable exp, null with a question */
	std::optional<int32_t> messageId;
	/** `yes`, null without a question: the kinah paid (negative), the exp given back, the recoverable exp left */
	std::optional<int64_t> yesKinahDelta, yesExpDelta, yesRecoverableExpAfter;
	/** `yes.messages` in the order they are sent: STR_GET_EXP2 with the exp, then STR_SUCCESS_RECOVER_EXPERIENCE */
	std::vector<EconomyMessage> yesMessages;
	/** STR_MSG_NOT_ENOUGH_KINA with the price, null without a question */
	std::optional<EconomyMessage> notEnoughKinah;
};

/** SM_CUBE_UPDATE's expansion form: the action, the storage and the three expansion counts after the yes */
struct EconomyCubeUpdate {
	int32_t action = 0, storage = 0, npcExpands = 0, questExpands = 0, itemExpands = 0;

	bool operator==(const EconomyCubeUpdate&) const = default;
};

/** one `cube` entry (X26) */
struct EconomyCube {
	int32_t npcId = 0;
	std::string answer;
	std::optional<int64_t> price;
	std::optional<EconomyQuestion> question;
	/**
	 * `yes`, all null for an answer without a question: the npc expansions after it, the kinah paid (negative), the slots added, the message
	 * STR_EXTEND_INVENTORY_SIZE_EXTENDED and SM_CUBE_UPDATE
	 */
	std::optional<int32_t> npcExpandsAfter;
	std::optional<int64_t> yesKinahDelta;
	std::optional<int32_t> cubeSlotsAdded;
	std::optional<EconomyMessage> yesMessage;
	std::optional<EconomyCubeUpdate> smCubeUpdate;
	/** STR_WAREHOUSE_EXPAND_NOT_ENOUGH_MONEY, null for an answer without a question */
	std::optional<EconomyMessage> notEnoughKinah;
	/** the refusal's message id of an answer without a question */
	std::optional<int32_t> messageId;
};

/** one `mail` entry (X13): what the sender pays per race */
struct EconomyMail {
	std::string spec;
	int64_t itemCommission = 0, kinahCommission = 0, serviceBase = 0;
	/** race -> (the service price, the total with the kinah) */
	std::map<std::string, std::pair<int64_t, int64_t>> byRace;
};

/** `items[].socketing[]` (C16, X24): the manastone the gate seeds in the armour the gate seeds */
struct EconomySocketing {
	int32_t stoneId = 0;
	bool canAct = false;
	bool fits = false;
	std::optional<std::string> refusedBy;
	std::optional<int32_t> slotLevel;
	std::optional<std::array<int32_t, 2>> socketsRange;
	std::optional<float> successChance;
	/** a chance of 100 or more: no randomness (D6's 200) */
	std::optional<bool> certain;
	/** a chance of 0 or less: Rnd.chance() never lies below it */
	std::optional<bool> impossible;
	std::optional<bool> needsOptionalSocket;
	/** Rates.get's rate, null for canAct's refusal */
	std::optional<float> rate;
	/**
	 * a refusal: STR_GIVE_ITEM_OPTION_FAILED's id after the 2 s (null for canAct's, which sends nothing), whether the stone is lost, and the
	 * AuditLogger line of noSocket; all null for a stone that fits
	 */
	std::optional<int32_t> messageId;
	std::optional<bool> stoneConsumed;
	std::optional<std::string> auditLog;
	/** a stone that fits: STR_GIVE_ITEM_OPTION_SUCCEED's and STR_GIVE_ITEM_OPTION_FAILED's ids and the animation's 2,000 ms */
	std::optional<int32_t> successMessageId, failureMessageId, animationMillis;
};

/** the identification's item usage animation (identification.animation): its time and the start, end and abort actions */
struct EconomyAnimation {
	int32_t time = 0, start = 0, end = 0, abort = 0;

	bool operator==(const EconomyAnimation&) const = default;
};

/** one `items` entry (X23, X27, C15) */
struct EconomyItem {
	int32_t itemId = 0;
	std::string name;
	int32_t level = 0;
	std::string itemGroup, equipType;
	int32_t manastoneSlots = 0, maxEnchant = 0;
	// identification (D5, X23)
	bool canTune = false;
	/** false: a row written with the SQL default tune_count loads unidentified - the gate must write -1 (D5) */
	bool sqlDefaultLoadsIdentified = false;
	std::optional<int32_t> seedTuneCountForUnidentified;
	std::optional<std::array<int32_t, 2>> optionalSocketsRange, enchantBonusRange;
	std::optional<int32_t> identifyMessageId;
	/** the tune count after the identification and its animation, null for an item that cannot be tuned */
	std::optional<int32_t> tuneCountAfter;
	std::optional<EconomyAnimation> identifyAnimation;
	// breakItem (X27)
	bool breakable = false;
	/** the stone ids the extraction can give (with a probability each), and the count range */
	std::vector<std::pair<int32_t, double>> breakStones;
	std::optional<std::array<int32_t, 2>> breakCountRange;
	/** STR_DECOMPOSE_ITEM_SUCCEED's id, null for an item that cannot be broken */
	std::optional<int32_t> breakMessageId;
	// equipItem (C15)
	bool equipPasses = false;
	std::optional<std::string> equipRefusedBy;
	/** the refusal's message name and id, null for a refusal without a packet (equipSkill, itemSlot) */
	std::optional<std::string> equipMessage;
	std::optional<int32_t> equipMessageId;
	/** restrict_max for the class (0: none) and the template's race */
	int32_t maxLevelRestrict = 0;
	std::string itemRace;
	/** the checks of Equipment.equipItem the oracle does not model */
	std::string equipNotModelled;
	int32_t requiredLevel = 0;
	std::optional<int64_t> startExpOfRequiredLevel;
	std::vector<int32_t> requiredSkills, knownRequiredSkills;
	std::vector<EconomySocketing> socketing;
};

/** one skill the Daeva's enter world teaches */
struct EconomyLearnedSkill {
	int32_t skillId = 0;
	int32_t level = 0;
	std::string playerClass;
};

/** `daeva` (C19's seed, X21a) */
struct EconomyDaeva {
	std::string playerClass, startingClass, race;
	int64_t exp = 0;
	int32_t questId = 0;
	std::string questStatus;
	int32_t oldLevel = 0;
	int32_t level = 0, levelWithoutQuest = 0;
	std::array<int32_t, 2> learnNewSkills{};
	std::vector<int32_t> storedSkills;
	std::vector<EconomyLearnedSkill> learnedSkills;
	/** the skill the Daeva swap removes and the one it adds (null when the character already had it), null without a swap */
	std::optional<int32_t> swapRemoved, swapAdded;
	/** every skill id after the enter world: what the burst's SM_SKILL_LIST holds */
	std::vector<int32_t> skills;
	/** one SM_LEARN_RECIPE each */
	std::vector<int32_t> learnedRecipes;
};

/** one vendor of a C19 component */
struct EconomyVendor {
	int32_t npcId = 0;
	/** the kinah of the component's quantity */
	int64_t kinah = 0;
	double distanceFromMaster = 0;
	EconomyTalk talk;
};

/** one component of the C19 recipe's first alternative */
struct EconomyComponent {
	int32_t itemId = 0;
	int64_t quantity = 0;
	/** the vendor nearest the master, null when no vendor on the map sells it (then it is a seed item) */
	std::optional<int32_t> vendor;
	std::vector<EconomyVendor> vendors;
};

/** one static object of the C19 tool */
struct EconomyTool {
	int32_t staticId = 0;
	float x = 0, y = 0, z = 0;
	double distanceFromMaster = 0;
};

/** one spot at a --craft-distance from the chosen tool (X18, X19) */
struct EconomyCraftSpot {
	double distance = 0;
	float x = 0, y = 0, z = 0;
	bool inPacketRange = false, inCheckCraftRange = false;
	/** what CM_CRAFT from here does: "CraftingTask", "STR_COMBINE_TOO_FAR_FROM_TOOL and the cancel pair" or "nothing (CM_CRAFT returns)" */
	std::string outcome;
	/** the other tools' static ids within checkCraft's range of this spot */
	std::vector<int32_t> otherToolsInCheckCraftRange;
};

/** `craft` (C19, X17-X21) */
struct EconomyCraft {
	int32_t mapId = 0;
	int32_t recipeId = 0, skillId = 0;
	std::vector<std::pair<int32_t, int64_t>> recipeComponents;
	int32_t productId = 0;
	int64_t productQuantity = 0;
	int32_t fewestSteps = 0, mostSteps = 0;
	int64_t fewestMillis = 0, mostMillis = 0;
	int32_t interval = 0, firstTickDelay = 0;
	int64_t xpReward = 0, playerExp = 0;
	int32_t skillLevelAfter = 0;
	int32_t masterNpcId = 0;
	EconomyTalk master;
	int32_t dialogAction = 0, minCharacterLevel = 0;
	int64_t learnCost = 0;
	/** whether the master's func_dialogs hold COMBINE_SKILL_LEVELUP (else CM_DIALOG_SELECT's npc gate audits it) */
	bool learnSupported = false;
	EconomyQuestion learnQuestion;
	/** the message name of a learn without the kinah (the oracle writes no id for it) */
	std::string learnNotEnoughKinah;
	std::vector<int32_t> recipesLearnedWithTheSkill;
	std::vector<EconomyComponent> components;
	std::vector<std::pair<int32_t, int64_t>> seedItems;
	int64_t exactKinah = 0;
	int32_t seedWorldId = 0;
	float seedX = 0, seedY = 0, seedZ = 0;
	int32_t toolTemplateId = 0;
	std::vector<EconomyTool> tools;
	int32_t chosenToolStaticId = 0;
	float checkCraftRange = 0, packetRange = 0;
	std::vector<EconomyCraftSpot> spots;
};

/** the whole answer */
struct EconomyAnswer {
	int32_t mapId = 0;
	/** race -> SM_PRICES' three bytes (X1) */
	std::map<std::string, std::array<int32_t, 3>> smPrices;
	std::vector<EconomyTalk> talk;
	std::optional<EconomyRecovery> recovery;
	std::vector<EconomyCube> cube;
	/** race -> Seril's removal price (X25), null without a REMOVE_ITEM_OPTION npc among the --npc */
	std::optional<std::map<std::string, int64_t>> removalPrice;
	/** the removal's base price and the ids of STR_REMOVE_ITEM_OPTION_SUCCEED and STR_REMOVE_ITEM_OPTION_NOT_ENOUGH_GOLD, null as removalPrice */
	std::optional<int64_t> removalBasePrice;
	std::optional<int32_t> removalSucceedMessageId, removalNotEnoughKinahMessageId;
	std::vector<EconomyMail> mail;
	std::vector<EconomyItem> items;
	std::string characterClass, characterRace;
	int32_t characterLevel = 0;
	/** null without --item */
	std::optional<std::vector<int32_t>> learnedSkills;
	std::optional<EconomyDaeva> daeva;
	std::optional<EconomyCraft> craft;

	/** @throws std::out_of_range when the answer has no talk block of the npc */
	const EconomyTalk& talkOf(int32_t npcId) const;
	/** @throws std::out_of_range when the answer has no item block of the id */
	const EconomyItem& item(int32_t itemId) const;
};

/** Parses an m5c-economy answer (@throws nlohmann::json::exception on invalid JSON, std::runtime_error on an answer of another format) */
EconomyAnswer parseEconomy(std::string_view economyJson);

/** Runs oracle.py m5c-economy with the request's arguments and parses the answer */
EconomyAnswer runEconomy(const Oracle& oracle, const EconomyRequest& request);

} // namespace aion::gameserver::scenario
