"""m5c-economy: the constants of the M5c gate that m5c-trade and m5c-craft do not answer (m5c-plan.md G-01, §10.2-§10.3; stage 0 of the
harness-a lane).

Java rules, each with the method it comes from (game-server/src/com/aionemu/gameserver unless another tree is named):
- talking to an npc (X2, X3): CM_SHOW_DIALOG -> NpcController.onDialogRequest (NpcController.java:250-262): nothing without <talk_info>
  (NpcTemplate.canInteract); outside PositionUtil.isInTalkRange STR_DIALOG_TOO_FAR_TO_TALK for an is_dialog npc, STR_WAREHOUSE_TOO_FAR_FROM_NPC
  otherwise; inside, the npc's AI gets DIALOG_START. isInTalkRange (PositionUtil.java:306-309) is isInRange(npc, player, talkDistance + 1,
  false), which adds the npc's and then the player's BoundRadius.getMaxOfFrontAndSide to the range in float arithmetic and compares the float
  squared 3D distance strictly below its square (:243-251, 257-262). The npc's bound radius is its <bound_radius> (BoundRadius.DEFAULT
  without one, NpcTemplate.java:254-257), the player's the BoundRadius(0.25f, 0.25f, height) PlayerAccountData gives PlayerCommonData, the
  player's object template (PlayerAccountData.java:99, PlayerCommonData.java:515-522). The report gives, per npc, the X2 BAND SPOT - a point
  whose distance lies in [talk + R_npc + R_player, talk + 1 + R_npc + R_player), so only the "+ 1" admits it, and which the centre-to-centre
  range (talk + 1, the bound radii dropped) refuses too - a near spot 2 m away and a far spot (--far, default 10 m) outside the range, each
  checked with the float arithmetic;
- the window DIALOG_START opens: GeneralNpcAI.handleDialogStart -> TalkEventHandler.onTalk (data/handlers/ai/GeneralNpcAI.java:50-52,
  TalkEventHandler.java:22-46) -> DialogPage.getStartPageId (DialogPage.java:113-125): 0 without a conversation or function, 1011 when
  DialogService.isInteractionAllowed refuses, 10 for an npc with func_dialogs (or a quest interaction), 1352 for a daeva at an npc with an
  alternative dialog, else 1011; PostboxAI.handleDialogStart (data/handlers/ai/PostboxAI.java:23-26) sets the mailbox state REGULAR and sends
  DialogPage.MAIL, whose SM_DIALOG_WINDOW carries the state in its last short (SM_DIALOG_WINDOW.java:35-36);
- the function arms of DialogService.onDialogSelect the gate uses: REMOVE_ITEM_OPTION -> sendDialogWindow (DialogService.java:103, 293-296:
  SM_DIALOG_WINDOW with DialogPage.getByActionId(action), 20 for REMOVE_MANASTONE, when the npc supports the action); RECOVERY (:126-163,
  `recovery`); EXTEND_INVENTORY -> CubeExpandService.expandCube (:203-205, `cube`); BUY and SELL are m5c-trade's;
- soul healing (X15, `recovery`): factor = expLost < 1000000 ? 0.25 - 0.00000015 * expLost : 0.1 (double), price = (int) (expLost * factor),
  asked with SM_QUESTION_WINDOW(STR_ASK_RECOVER_EXPERIENCE, 0, 0, String.valueOf(price)); yes pays `price`, sends STR_GET_EXP2(expLost) and
  STR_SUCCESS_RECOVER_EXPERIENCE and gives the exp back (PlayerCommonData.resetRecoverableExp, PlayerCommonData.java:153-157); no recoverable
  exp answers STR_DONOT_HAVE_RECOVER_EXPERIENCE and no question;
- the cube (X26, `cube`): CubeExpandService.expandCube (CubeExpandService.java:29-64) over the cube_expander template of the npc
  (CubeExpandData.afterUnmarshal, StorageExpansionTemplate): canExpand (:114-123, npc + quest + item expansions + 1 above
  gameserver.cube.expansion_limit refuses), the next npc expansion below the template's minimum level refuses, its price (the raw <expand
  price>, no price factor) or above min(the template's maximum, gameserver.npcexpands.limit) refuses; the question is STR_WAREHOUSE_EXPAND_WARNING
  with the price; yes pays it (tryDecreaseKinah) and expands (STR_EXTEND_INVENTORY_SIZE_EXTENDED(9), SM_CUBE_UPDATE with the npc expansions + 1);
- the manastone removal at Seril (X25, `manastoneRemoval`): ItemSocketService.removeManastone (ItemSocketService.java:102-138) pays
  PricesService.getPriceForService(650, race) (PricesService.java:86-90: three `(long) (x * factor / 100D)` truncations over the global prices,
  the modifier and the taxes - m5c-trade's SM_PRICES factors);
- a letter (X13, `mail`): MailService.sendMail (MailService.java:56-170): base cost 500 for EXPRESS, else 10; cost factor 5 / 1; the item
  commission (long) (price * getQualityPriceRate(item) * count * factor) in float arithmetic (the rate 0.05f MYTHIC and EPIC, 0.04f UNIQUE and
  LEGEND, 0.03f RARE, else 0.02f, :186-199); the kinah commission (long) (kinah * 0.01f * factor); the sender pays
  getPriceForService(base + both commissions) + kinah;
- extraction (X27, `items[].breakItem`): EnchantService.breakItem (EnchantService.java:37-76) for an armour or a weapon: calculateEffectiveLevel
  of its quality and level (:82-98), plus Rnd.get(0, 10) (commons Rnd.java: both bounds inclusive), plus 5 for a weapon; the stone is the first
  of EPSILON, DELTA, GAMMA, BETA whose effective level (EnchantmentStone's base quality and level) the roll reaches, else ALPHA; the count is
  Rnd.get(2, 5) for a weapon, Rnd.get(1, 3) for armour;
- identification (X23, D5, `items[].identification`): ItemTemplate.afterUnmarshal (ItemTemplate.java:149-162) sets maxTuneCount (rnd_count,
  default -1) to 0 for an item without equipment slots or, when it is -1, without option_slot_bonus, max_enchant_bonus and rnd_bonus;
  canTune() is maxTuneCount != 0 (:471-473). A new item of a tunable template has tuneCount -1, i.e. is not identified (Item.java:78-89,
  846-848); a row loaded with tune_count -1 stays unidentified only while the template can be tuned (Item.java:128-130), and the SQL default
  of the column is 0, which loads identified - so a seed that must load unidentified writes -1 (m5c-plan.md D5). ItemActionService.identifyItem
  (ItemActionService.java:23-54) rolls the optional sockets Rnd.get(0, option_slot_bonus), the enchant bonus Rnd.get(0, max_enchant_bonus) and
  the stat bonus (TuningAction.getRandomStatBonusIdFor: 0 without a rnd_bonus set), and raises the tune count by one (-1 -> 0), 5 s after the
  first SM_ITEM_USAGE_ANIMATION(..., 5000, 9, 0);
- equipping it (A-05, C15, X23, `items[].equip`), Equipment.equipItem's checks in its order (Equipment.java:59-129): isClassSpecific
  (restrict[class ordinal] > 0, or the starting class's for an advanced class) else STR_CANNOT_USE_ITEM_INVALID_CLASS; getRequiredLevel
  (restrict[ordinal], -1 for 0) -1 or above the level refuses with STR_CANNOT_USE_ITEM_TOO_LOW_LEVEL_MUST_BE_THIS_LEVEL; getMaxLevelRestrict
  (restrict_max[ordinal], 0 without restrict_max) not 0 and below the level refuses with STR_CANNOT_USE_ITEM_TOO_HIGH_LEVEL; an item race
  other than PC_ALL and the character's (--race) refuses with STR_CANNOT_USE_ITEM_INVALID_RACE; checkAvailableEquipSkills (:108-109,
  303-314) refuses WITHOUT a packet unless the item group's getRequiredSkills is empty or the character knows one of them
  (PlayerSkillList.isSkillPresent) - the known skills are the autolearn ones of SkillLearnService.learnNewSkills(player, 1, level)
  (SkillLearnService.java:60-93: the level's skill_tree rows of the class, class-less rows included (SkillTreeData.afterUnmarshal), of the
  race or PC_ALL (getTemplatesFor), and below level 10 an advanced class's starting class's too), which is what creation
  (PlayerService.java:202) and every level change since (PlayerController.java:573, 594, the enter-world one from players.old_level included,
  PlayerEnterWorldService.java:204) teach together, PlayerSkillList.addSkill never removing a skill; an item group without equipment slots
  (getItemSlot 0) is never equipped (:127-129, no packet). `startExpOfRequiredLevel` is the exp that level starts at
  (PlayerExperienceTable.getStartExpForLevel, PlayerExperienceTable.java:29-34), what a gate seeds to give a character that level.

What the oracle does NOT model, and raises OracleError for (exit 2): a Java member it models that changed (every one in MODELLED_MEMBERS,
HANDLER_MEMBERS and COMMONS_MEMBERS is fingerprinted WHOLE, comments and white space removed, as m5c-trade does), an npc whose DIALOG_START AI is
not GeneralNpcAI or PostboxAI, an npc whose talk_info has a subdialog_type (isInteractionAllowed then depends on the player), a town npc
(title 462877, TalkEventHandler.java:29-38), a moving npc (pool, walker, random walk) or one without a regular spawn on the map, an empty float
band, a quality calculateEffectiveLevel answers 0 for (breakItem throws IllegalArgumentException), an unknown item, class or level, sieges
without --influence (m5c-trade's race_prices), a config value Java rejects, an item group whose required skills include 30001 or 30002 (the
daeva branch of learnNewSkills, SkillLearnService.java:69-74, is quest state), a skill_tree classId that is no PlayerClass. Player state beyond
the arguments is not modelled: the known list, trading, hide effects, the kinah a question's yes needs, whether a quest handler answers
DialogAction.USE_OBJECT at the npc first (TalkEventHandler.java:26-27, the M5d quest oracle's field - the report says so in `assumptions`), a
rnd_bonus set's stat bonus draw, skills learned otherwise than by autolearn (skill books, stigmas), and equipItem's gender, rank and cube-space
checks (Equipment.java:92-106, between the race and the skill checks) and everything after the slot check (the slot the client asks for,
stigma, soul binding, identification).
"""

from __future__ import annotations

import math
import re
from dataclasses import dataclass
from pathlib import Path

from staticdata_oracle import OracleError

from m5a.creation import RACES, JavaEnums, enum_constants
from m5a.data import StaticData, java_boolean, java_int
from m5a.javafloat import distance, f32, in_range, parse_float, to_int, to_long
from m5a.spawns import GameClock, evaluate, load_groups, load_npc_templates
from m5b.monster import _java_long

from .trade import NO_AI, _read, _strip_comments, check_country_code, member_fingerprint, race_prices, times_div_100d
from .trade_config import TRADE_KEYS, ConfigValue

FORMAT = "aion-m5c-economy"

# The keys this oracle reads: m5c-trade's (the price factors, sieges, the country code) and the cube limits
ECONOMY_KEYS = {
	**TRADE_KEYS,
	"gameserver.cube.expansion_limit": ("CustomConfig", "CUBE_EXPANSION_LIMIT", "int"),
	"gameserver.npcexpands.limit": ("CustomConfig", "NPC_CUBE_EXPANDS_SIZE_LIMIT", "int"),
}

# Every member whose code the oracle models, fingerprinted whole like m5c-trade's MODELLED_MEMBERS (trade.member_fingerprint: sha256 of the text
# without comments and white space, the first 16 hex digits), below com/aionemu/gameserver
MODELLED_MEMBERS = (
	# talking: the range, the request, the start page
	("utils/PositionUtil.java", "method isInTalkRange(Creature creature, Npc npc)", "e82076de9b588ded"),
	("utils/PositionUtil.java", "method isInRange(VisibleObject object, VisibleObject object2, float range, boolean centerToCenter)", "935d6ed823532f4e"),
	("utils/PositionUtil.java", "method isInRange(float x1, float y1, float z1, float x2, float y2, float z2, float range)", "355bda96b2e0e672"),
	("controllers/NpcController.java", "method onDialogRequest", "090a13d60247c786"),
	("model/templates/npc/NpcTemplate.java", "method getTalkDistance", "9e4b01452f321734"),
	("model/templates/npc/NpcTemplate.java", "method canInteract", "6317f07bb76b59fe"),
	("model/templates/npc/NpcTemplate.java", "method isDialogNpc", "390aac5346f773e9"),
	("model/templates/npc/NpcTemplate.java", "method getBoundRadius", "f28e08cc67b5b366"),
	("model/templates/npc/NpcTemplate.java", "method getTitleId", "ef4654e4fe66fc90"),
	("model/templates/npc/TalkInfo.java", "class TalkInfo", "f1b9c8609047b988"),
	("model/templates/BoundRadius.java", "method getMaxOfFrontAndSide", "25054c9b55e09f3c"),
	("model/templates/VisibleObjectTemplate.java", "method getBoundRadius", "0574488a0f7c97fb"),
	("model/gameobjects/player/PlayerCommonData.java", "method getBoundRadius", "1d230f85198d84eb"),
	("model/account/PlayerAccountData.java", "method updateBoundingRadius", "f3aa8208d3ba3466"),
	("model/gameobjects/player/PlayerCommonData.java", "method setBoundingRadius", "0116ea60c330e94f"),
	("ai/handler/TalkEventHandler.java", "method onTalk", "9119f81ae4878c95"),
	("model/DialogPage.java", "method getStartPageId", "6d32365ac1b14ce0"),
	("model/DialogPage.java", "method getByActionId", "0fab85d60fae4e85"),
	("services/DialogService.java", "method isInteractionAllowed", "50b0bc09944cefc1"),
	("services/DialogService.java", "method sendDialogWindow", "0eaf02bfe499b6f5"),
	("services/DialogService.java", "case REMOVE_ITEM_OPTION", "733c8ca898fbbdb2"),
	# soul healing
	("services/DialogService.java", "case RECOVERY", "a15c91e7d78e4315"),
	("model/gameobjects/player/PlayerCommonData.java", "method resetRecoverableExp", "0b1713632665f7e7"),
	# the cube
	("services/DialogService.java", "case EXTEND_INVENTORY", "64c2316aa83a3392"),
	("services/CubeExpandService.java", "method expandCube", "9d55cc8796396b71"),
	("services/CubeExpandService.java", "method expand", "183833157c8c3af9"),
	("services/CubeExpandService.java", "method canExpand", "bfda165e5a057600"),
	("model/templates/StorageExpansionTemplate.java", "class StorageExpansionTemplate", "cfd5fdfc28f5baaa"),
	("dataholders/CubeExpandData.java", "class CubeExpandData", "09dec637ac634fe2"),
	# the removal, the letter and the service price
	("services/item/ItemSocketService.java", "method removeManastone", "ba5cffb5d0e78681"),
	("services/trade/PricesService.java", "method getPriceForService", "91f39b0dea4c33f4"),
	("services/mail/MailService.java", "method sendMail", "3f81127bf199f13f"),
	("services/mail/MailService.java", "method getQualityPriceRate", "8525fc9e710606b9"),
	# extraction
	("services/EnchantService.java", "method breakItem", "fa31ae00e94e34dd"),
	("services/EnchantService.java", "method calculateEffectiveLevel(EnchantmentStone enchantmentStone)", "f03564015856a7ac"),
	("services/EnchantService.java", "method calculateEffectiveLevel(ItemQuality itemQuality, int itemLevel)", "05705295d1ea7564"),
	("model/enchants/EnchantmentStone.java", "class EnchantmentStone", "46ff72f52908de96"),
	# identification, the template's tuning and equipping
	("services/item/ItemActionService.java", "method identifyItem", "ac73fc6d0cd29981"),
	("model/templates/item/actions/TuningAction.java", "method getRandomStatBonusIdFor", "b2f6948887f8b00d"),
	("dataholders/ItemRandomBonusData.java", "method selectRandomBonusNumber", "87cfc73d12bb13a8"),
	("model/templates/item/ItemTemplate.java", "method afterUnmarshal", "d1aa10c234ff4818"),
	("model/templates/item/ItemTemplate.java", "method getItemSlot", "712766b8c16153fd"),
	("model/templates/item/ItemTemplate.java", "method canTune", "f3954702d3697a82"),
	("model/templates/item/ItemTemplate.java", "method isWeapon", "9d98f9ddb627b80c"),
	("model/templates/item/ItemTemplate.java", "method isArmor", "47ced425d29d7d63"),
	("model/templates/item/ItemTemplate.java", "method getEquipmentType", "78ecf2b8699f704d"),
	("model/templates/item/enums/ItemGroup.java", "method getEquipType", "62af64f7c4b54aa0"),
	("model/templates/item/ItemTemplate.java", "method isClassSpecific", "d19f39c47efb2fd4"),
	("model/templates/item/ItemTemplate.java", "method getRequiredLevel", "00b55cb320770a06"),
	("model/gameobjects/Item.java", "method Item(int objId, ItemTemplate itemTemplate)", "d6c5b750db61ae32"),
	("model/gameobjects/Item.java", "method isIdentified", "e97d4e21fc09f062"),
	("model/gameobjects/player/Equipment.java", "method equipItem", "bdc71eb930120d79"),
	("dataholders/PlayerExperienceTable.java", "method getStartExpForLevel", "c0c3883d5a2ccccd"),
	# equipping: the maximum level, the race, the equip skills and how a character learns them
	("model/templates/item/ItemTemplate.java", "method getMaxLevelRestrict", "02e545d5b11efda7"),
	("model/templates/item/ItemTemplate.java", "method getRace", "77effa67b7eb579b"),
	("model/gameobjects/player/Equipment.java", "method checkAvailableEquipSkills", "ba9393acc35dadae"),
	("model/templates/item/ItemTemplate.java", "method getRequiredSkills", "ccfe3172e88c282d"),
	("model/templates/item/enums/ItemGroup.java", "method getRequiredSkills", "4645369b58d4761e"),
	("model/templates/item/enums/ItemGroup.java", "method ItemGroup()", "7a022528ae65ae68"),
	("model/templates/item/enums/ItemGroup.java", "method ItemGroup(long validEquipmentSlots, ArmorType armorType)", "cd64ab6b665ed000"),
	("model/templates/item/enums/ItemGroup.java", "method ItemGroup(long validEquipmentSlots, ItemSubType itemSubType)", "7ed1d781f948f261"),
	("model/templates/item/enums/ItemGroup.java", "method ItemGroup(long validEquipmentSlots, ItemSubType itemSubType, int[] requiredSkill)",
	 "a00c55aaac301841"),
	("model/templates/item/enums/ItemGroup.java", "method ItemGroup(long validEquipmentSlots, ArmorType armorType, int[] requiredSkill)",
	 "83464b7b2229a00c"),
	("model/skill/PlayerSkillList.java", "method isSkillPresent", "80b5167b84919126"),
	("services/SkillLearnService.java", "method learnNewSkills", "44ad1d6ab2eb73bb"),
	("services/SkillLearnService.java", "method autoLearnSkills", "ff996a57b840effc"),
	("dataholders/SkillTreeData.java", "method afterUnmarshal", "944e793b3aa907c8"),
	("dataholders/SkillTreeData.java", "method addTemplate", "789b5a82481135c7"),
	("dataholders/SkillTreeData.java", "method getTemplatesFor", "452c89e61d7b274b"),
	("model/PlayerClass.java", "method isStartingClass", "f8d319f1500c8ae1"),
	("model/PlayerClass.java", "method getStartingClass", "3add7210221eb924"),
	# the rest of what the arms above lean on: the npc expansion, the cube packet, the spot's ai and the bonus set lookup
	("services/CubeExpandService.java", "method npcExpand", "3fa9e9a6541961a0"),
	("network/aion/serverpackets/SM_CUBE_UPDATE.java", "method cubeSize", "0ab0e1b8b67404d3"),
	("model/gameobjects/Creature.java", "method Creature(int objId, CreatureController<? extends Creature> controller, SpawnTemplate spawnTemplate, "
	 "CreatureTemplate objectTemplate, WorldPosition position, boolean autoReleaseObjectId)", "bb89200104d6a250"),
	("dataholders/ItemRandomBonusData.java", "method getBonusSet", "96d45c2966e31fd3"),
)

# below data/handlers: the two DIALOG_START AIs the report models
HANDLER_MEMBERS = (
	("ai/GeneralNpcAI.java", "method handleDialogStart", "2d59a379ca3c6618"),
	("ai/PostboxAI.java", "class PostboxAI", "bac11294007c689a"),
)

# below commons/src/com/aionemu/commons
COMMONS_MEMBERS = (
	("utils/Rnd.java", "method get(int minInclusive, int maxInclusive)", "aa21293fb0e45ba4"),
)

# The statements whose literals the report follows; each must be present verbatim (comments and white space removed) in its file, so the
# message names the one that moved, besides the member fingerprint
MODELLED_STATEMENTS = (
	("model/templates/item/ItemTemplate.java", '@XmlAttribute(name = "rnd_count") private int maxTuneCount = -1;'),
	("model/templates/item/ItemTemplate.java",
	 "private static final byte[] DEFAULT_LEVEL_RESTRICTION = new byte[] { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };"),
	("model/gameobjects/Item.java", "if (tuneCount == -1 && !itemTemplate.canTune()) { this.tuneCount = 0; }"),
	("model/templates/npc/TalkInfo.java", '@XmlAttribute(name = "distance") private int talkDistance = 2;'),
	("model/templates/item/ItemTemplate.java", '@XmlAttribute(name = "restrict_max") private byte[] maxLevelRestrictions;'),
	("model/templates/item/ItemTemplate.java", '@XmlAttribute(name = "race") private Race race = Race.PC_ALL;'),
	("skillengine/model/SkillLearnTemplate.java", '@XmlAttribute(name = "classId") private PlayerClass classId;'),
	("skillengine/model/SkillLearnTemplate.java", '@XmlAttribute(name = "race") private Race race = Race.PC_ALL;'),
	("skillengine/model/SkillLearnTemplate.java", '@XmlAttribute(name = "minLevel", required = true) private int minLevel;'),
	("skillengine/model/SkillLearnTemplate.java", "@XmlAttribute private boolean autolearn;"),
	# the character's skills are learnNewSkills(1, level): at creation, then on every level change (the enter-world one from old_level too)
	("services/player/PlayerService.java", "SkillLearnService.learnNewSkills(newPlayer, 1, newPlayer.getLevel());"),
	("controllers/PlayerController.java", "int minNewLevel = oldLevel < newLevel ? oldLevel + 1 : oldLevel - 1;"),
	("controllers/PlayerController.java", "SkillLearnService.learnNewSkills(player, minNewLevel, newLevel);"),
	("services/player/PlayerEnterWorldService.java",
	 "player.getController().onLevelChange(PlayerDAO.getOldCharacterLevel(player.getObjectId()), player.getLevel());"),
)

# Every Java file JavaEconomyRules.read reads below com/aionemu/gameserver (JavaEnums: ItemSlot, ItemSubType, ItemGroup, PlayerClass, ItemAttackType)
JAVA_SOURCES = tuple(sorted({relative for relative, _, _ in MODELLED_MEMBERS} | {relative for relative, _ in MODELLED_STATEMENTS} | {
	"model/DialogAction.java", "model/DialogPage.java", "services/player/PlayerMailboxState.java",
	"network/aion/serverpackets/SM_QUESTION_WINDOW.java", "network/aion/serverpackets/SM_SYSTEM_MESSAGE.java", "model/enchants/EnchantmentStone.java",
	"model/items/ItemSlot.java", "model/templates/item/enums/ItemSubType.java", "model/templates/item/enums/ItemGroup.java", "model/PlayerClass.java",
	"model/templates/item/ItemAttackType.java"}))

# the DialogAction constants the function arms are keyed by (DialogAction.java), and the arms this oracle models
FUNCTION_ARMS = ("BUY", "SELL", "RECOVERY", "REMOVE_ITEM_OPTION", "EXTEND_INVENTORY")
QUESTION_NAMES = ("STR_ASK_RECOVER_EXPERIENCE", "STR_WAREHOUSE_EXPAND_WARNING")
MESSAGE_NAMES = ("STR_DIALOG_TOO_FAR_TO_TALK", "STR_WAREHOUSE_TOO_FAR_FROM_NPC", "STR_GET_EXP2", "STR_SUCCESS_RECOVER_EXPERIENCE",
                 "STR_DONOT_HAVE_RECOVER_EXPERIENCE", "STR_MSG_NOT_ENOUGH_KINA", "STR_REMOVE_ITEM_OPTION_SUCCEED",
                 "STR_REMOVE_ITEM_OPTION_NOT_ENOUGH_GOLD", "STR_EXTEND_INVENTORY_SIZE_EXTENDED", "STR_WAREHOUSE_EXPAND_NOT_ENOUGH_MONEY",
                 "STR_EXTEND_INVENTORY_CANT_EXTEND_MORE", "STR_EXTEND_INVENTORY_CANT_EXTEND_DUE_TO_MINIMUM_EXTEND_LEVEL_BY_THIS_NPC",
                 "STR_EXTEND_INVENTORY_CANT_EXTEND_MORE_DUE_TO_MAXIMUM_EXTEND_LEVEL_BY_THIS_NPC", "STR_DECOMPOSE_ITEM_SUCCEED",
                 "STR_MSG_ITEM_IDENTIFY_SUCCEED", "STR_NOT_ENOUGH_MONEY", "STR_CANNOT_USE_ITEM_INVALID_CLASS",
                 "STR_CANNOT_USE_ITEM_TOO_LOW_LEVEL_MUST_BE_THIS_LEVEL", "STR_CANNOT_USE_ITEM_TOO_HIGH_LEVEL", "STR_CANNOT_USE_ITEM_INVALID_RACE")
# the skills learnNewSkills' daeva branch swaps (SkillLearnService.java:69-74), which needs PlayerCommonData.isDaeva - not modelled
DAEVA_GATHERING_SKILLS = (30001, 30002)
# the stones breakItem can give, in its order (EnchantService.java:58-67)
BREAK_STONES = (("EPSILON", 166000195), ("DELTA", 166000194), ("GAMMA", 166000193), ("BETA", 166000192))
BREAK_STONE_ALPHA = 166000191
TOWN_NPC_TITLE = 462877  # TalkEventHandler.onTalk's villager arm
POSTBOX_AI, GENERAL_AI = "postbox", "general"
NEAR_DISTANCE = 2.0


def _search(text: str, pattern: str, what: str) -> re.Match:
	match = re.search(pattern, text)
	if not match:
		raise OracleError(f"{what}: not found; the Java source does not have the shape this oracle was written against")
	return match


def _check_members(base: Path, members: tuple[tuple[str, str, str], ...], texts: dict[str, str]) -> None:
	for relative, member, fingerprint in members:
		if relative not in texts:
			texts[relative] = _read(base / relative)
		actual = member_fingerprint(texts[relative], member, relative)
		if actual != fingerprint:
			raise OracleError(f"{relative}: `{member}` changed (fingerprint {actual}, the oracle models {fingerprint}): the Java code this oracle "
			                  "models is not the one it was written against")


@dataclass(frozen=True)
class JavaEconomyRules:
	"""The literals and tables of the economy rules, read from the Java sources after every modelled member was checked."""

	dialog_actions: dict[str, int]      # DialogAction: the constants of FUNCTION_ARMS
	page_by_action: dict[int, int]      # DialogPage: dialog action id -> page id (getByActionId), for the actions that have a page
	pages: dict[str, int]               # DialogPage: name -> id
	mailbox_states: dict[str, int]      # PlayerMailboxState
	questions: dict[str, int]           # SM_QUESTION_WINDOW ids of QUESTION_NAMES
	messages: dict[str, int]            # SM_SYSTEM_MESSAGE ids of MESSAGE_NAMES
	player_bound: float                 # PlayerAccountData: max(front, side) of the player's BoundRadius
	stones: dict[str, tuple[int, str]]  # EnchantmentStone: name -> (base level, base quality)
	effective_level_bonus: dict[str, int]  # EnchantService.calculateEffectiveLevel(quality, level): quality -> the added value
	removal_base: int                   # ItemSocketService.removeManastone: getPriceForService(N, race)
	mail: dict[str, object]             # MailService.sendMail's literals
	tune_count_sql_default: int         # sql/aion_gs.sql: the DEFAULT of inventory.tune_count, what a row written without it loads
	enums: JavaEnums                    # ItemGroup (slots, equip type), PlayerClass (ordinals, starting classes)
	required_skills: dict[str, tuple[int, ...]]  # ItemGroup: name -> getRequiredSkills (the `new int[] {...}` argument, empty without one)

	@staticmethod
	def read(java_src: Path, handlers_dir: Path, commons_src: Path, sql_file: Path) -> "JavaEconomyRules":
		base = Path(java_src) / "com" / "aionemu" / "gameserver"
		texts: dict[str, str] = {}
		_check_members(base, MODELLED_MEMBERS, texts)
		_check_members(Path(handlers_dir), HANDLER_MEMBERS, {})
		_check_members(Path(commons_src) / "com" / "aionemu" / "commons", COMMONS_MEMBERS, {})
		for relative, statement in MODELLED_STATEMENTS:
			if relative not in texts:
				texts[relative] = _read(base / relative)
			if re.sub(r"\s+", "", statement) not in re.sub(r"\s+", "", _strip_comments(texts[relative])):
				raise OracleError(f"{relative} no longer contains `{statement}`: the Java source does not have the shape this oracle was written against")

		dialog_action = _strip_comments(_read(base / "model" / "DialogAction.java"))
		actions = {name: int(_search(dialog_action, rf"public\s+static\s+final\s+int\s+{name}\s*=\s*(\d+)\s*;", f"DialogAction.{name}").group(1))
		           for name in FUNCTION_ARMS + ("OPEN_POSTBOX",)}

		pages: dict[str, int] = {}
		page_by_action: dict[int, int] = {}
		for name, args in enum_constants(base / "model" / "DialogPage.java", "DialogPage"):
			parts = [p.strip() for p in (args or "").split(",")]
			if len(parts) == 1:
				pages[name] = java_int(parts[0], f"DialogPage.{name}")
			elif len(parts) == 2 and parts[0].startswith("DialogAction."):
				pages[name] = java_int(parts[1], f"DialogPage.{name}")
				action = parts[0].removeprefix("DialogAction.")
				action_id = int(_search(dialog_action, rf"public\s+static\s+final\s+int\s+{action}\s*=\s*(\d+)\s*;", f"DialogAction.{action}").group(1))
				page_by_action.setdefault(action_id, pages[name])  # getByActionId: the first page of the action in declaration order
			else:
				raise OracleError(f"DialogPage.{name}: cannot read {args!r}")

		mailbox = _strip_comments(_read(base / "services" / "player" / "PlayerMailboxState.java"))
		states = {name: int(value, 16) for name, value in re.findall(r"public\s+static\s+final\s+byte\s+(\w+)\s*=\s*\(byte\)\s*0x([0-9A-Fa-f]+)\s*;",
		                                                              mailbox)}
		if "REGULAR" not in states:
			raise OracleError("PlayerMailboxState.java: no REGULAR state")

		question = _strip_comments(_read(base / "network" / "aion" / "serverpackets" / "SM_QUESTION_WINDOW.java"))
		questions = {name: int(_search(question, rf"public\s+static\s+final\s+int\s+{name}\s*=\s*(\d+)\s*;", f"SM_QUESTION_WINDOW.{name}").group(1))
		             for name in QUESTION_NAMES}
		system = _read(base / "network" / "aion" / "serverpackets" / "SM_SYSTEM_MESSAGE.java")
		messages = {name: int(_search(system, rf"public\s+static\s+SM_SYSTEM_MESSAGE\s+{name}\([^)]*\)\s*\{{\s*return\s+new\s+SM_SYSTEM_MESSAGE\((\d+)",
		                                  f"SM_SYSTEM_MESSAGE.{name}").group(1)) for name in MESSAGE_NAMES}

		bound = _search(texts["model/account/PlayerAccountData.java"], r"setBoundingRadius\(new BoundRadius\(([0-9.]+)f,\s*([0-9.]+)f,",
		                "PlayerAccountData's player bound radius")
		player_bound = max(parse_float(bound.group(1)), parse_float(bound.group(2)))

		stones = {}
		for name, args in enum_constants(base / "model" / "enchants" / "EnchantmentStone.java", "EnchantmentStone"):
			level, quality = [p.strip() for p in (args or "").split(",")]
			stones[name] = (java_int(level, f"EnchantmentStone.{name}"), quality.removeprefix("ItemQuality."))
		effective = _strip_comments(texts["services/EnchantService.java"])
		body = _search(effective, r"(?s)private\s+static\s+int\s+calculateEffectiveLevel\(ItemQuality\s+itemQuality,\s*int\s+itemLevel\)\s*\{(.*?)\n\t\}",
		               "EnchantService.calculateEffectiveLevel(ItemQuality, int)").group(1)
		bonus: dict[str, int] = {}
		for labels, value in re.findall(r"((?:case\s+\w+\s*:\s*)+)return\s+itemLevel\s*\+\s*(\d+)\s*;", body):
			for label in re.findall(r"case\s+(\w+)\s*:", labels):
				bonus[label] = int(value)
		removal = int(_search(texts["services/item/ItemSocketService.java"], r"getPriceForService\((\d+),\s*player\.getRace\(\)\)",
		                      "removeManastone's service price").group(1))
		mail_text = _strip_comments(texts["services/mail/MailService.java"])
		base_cost = _search(mail_text, r"int\s+baseCost\s*=\s*letterType\s*==\s*LetterType\.EXPRESS\s*\?\s*(\d+)\s*:\s*(\d+)\s*;",
		                    "sendMail's base cost")
		cost_factor = _search(mail_text, r"int\s+costFactor\s*=\s*letterType\s*==\s*LetterType\.EXPRESS\s*\?\s*(\d+)\s*:\s*(\d+)\s*;",
		                      "sendMail's cost factor")
		mail = {
			"expressBaseCost": int(base_cost.group(1)), "normalBaseCost": int(base_cost.group(2)),
			"expressCostFactor": int(cost_factor.group(1)), "normalCostFactor": int(cost_factor.group(2)),
			"kinahRate": parse_float(_search(mail_text, r"attachedKinah\s*\*\s*([0-9.]+)f\s*\*\s*costFactor", "sendMail's kinah rate").group(1)),
		}
		rates: dict[str, float] = {}
		rate_body = _search(mail_text, r"(?s)getQualityPriceRate\(Item senderItem\)\s*\{(.*?)\n\t\}", "getQualityPriceRate").group(1)
		for labels, value in re.findall(r"((?:case\s+\w+\s*:\s*)+)return\s+([0-9.]+)f\s*;", rate_body):
			for label in re.findall(r"case\s+(\w+)\s*:", labels):
				rates[label] = parse_float(value)
		rates["default"] = parse_float(_search(rate_body, r"default\s*:\s*return\s+([0-9.]+)f\s*;", "getQualityPriceRate's default").group(1))
		mail["qualityRates"] = rates
		inventory = _search(_read(Path(sql_file)), r"(?s)CREATE TABLE `inventory` \((.*?)\n\)", f"{sql_file}: the inventory table").group(1)
		tune_default = int(_search(inventory, r"`tune_count`\s+smallint\s+NOT\s+NULL\s+DEFAULT\s+'(-?\d+)'", f"{sql_file}: inventory.tune_count")
		                   .group(1))
		# the constructors that take no int[] pass `new int[] {}` (their fingerprints hold), so a constant without one requires no skill
		required_skills: dict[str, tuple[int, ...]] = {}
		for name, args in enum_constants(base / "model" / "templates" / "item" / "enums" / "ItemGroup.java", "ItemGroup"):
			arrays = re.findall(r"new\s+int\s*\[\s*\]\s*\{([^}]*)\}", args or "")
			if len(arrays) > 1:
				raise OracleError(f"ItemGroup.{name}: {len(arrays)} int arrays in {args!r}")
			required_skills[name] = tuple(java_int(t.strip(), f"ItemGroup.{name} required skill") for t in arrays[0].split(",") if t.strip()) \
				if arrays else ()
		return JavaEconomyRules(actions, page_by_action, pages, states, questions, messages, player_bound, stones, bonus, removal, mail,
		                        tune_default, JavaEnums(java_src), required_skills)


# ---- arithmetic ----------------------------------------------------------------------------------------------------------------------------

def service_price(base: int, prices: dict) -> int:
	"""PricesService.getPriceForService (PricesService.java:86-90): three truncations, global prices, the modifier, the taxes."""
	value = times_div_100d(base, prices["globalPrices"], "getPriceForService * getGlobalPrices")
	value = times_div_100d(value, prices["globalPricesModifier"], "getPriceForService * getGlobalPricesModifier")
	return times_div_100d(value, prices["taxes"], "getPriceForService * getTaxes")


def recovery_price(exp_lost: int) -> tuple[float, int]:
	"""DialogService's RECOVERY arm (DialogService.java:132-133): the double factor and (int) (expLost * factor)."""
	factor = 0.25 - (0.00000015 * exp_lost) if exp_lost < 1000000 else 0.1
	return factor, to_int(exp_lost * factor)


def talk_limits(talk_distance: int, npc_bound: float, player_bound: float) -> dict[str, float]:
	"""isInTalkRange's float range with both bound radii (npc first, PositionUtil.java:246-249), and the two ranges X2's mutations use."""
	talk_range = f32(talk_distance + 1)
	return {
		"talkRange": talk_range,
		"limit": f32(f32(talk_range + npc_bound) + player_bound),
		"limitWithoutPlusOne": f32(f32(f32(talk_distance) + npc_bound) + player_bound),
		"limitCenterToCenter": talk_range,
	}


def _spot_at(npc_spot: tuple[float, float, float], d: float, direction: float = 0.0) -> tuple[float, float, float]:
	"""The point `d` from the npc along `direction` (degrees, counter-clockwise from +x) at the npc's z, as the float position the player's object
	holds."""
	angle = math.radians(direction)
	return f32(npc_spot[0] + d * math.cos(angle)), f32(npc_spot[1] + d * math.sin(angle)), npc_spot[2]


def _in(npc_spot: tuple[float, float, float], spot: tuple[float, float, float], rng: float) -> bool:
	return in_range(npc_spot[0], npc_spot[1], npc_spot[2], spot[0], spot[1], spot[2], rng)


def band_spot(npc_spot: tuple[float, float, float], limits: dict[str, float], direction: float = 0.0) -> tuple[float, float, float]:
	"""The X2 spot: in isInTalkRange's range, outside the range without the "+ 1" and outside the centre-to-centre range."""
	# the band is [max(talk + radii, talk + 1), talk + 1 + radii): empty only when both radii are 0 (a player's is 0.25)
	low = max(limits["limitWithoutPlusOne"], limits["limitCenterToCenter"])
	candidates = [f32((low + limits["limit"]) / 2)] + [f32(low + f32(step * (limits["limit"] - low))) for step in (0.25, 0.75, 0.1, 0.9)]
	for d in candidates:
		spot = _spot_at(npc_spot, d, direction)
		if _in(npc_spot, spot, limits["limit"]) and not _in(npc_spot, spot, limits["limitWithoutPlusOne"]) \
				and not _in(npc_spot, spot, limits["limitCenterToCenter"]):
			return spot
	raise OracleError(f"no float spot along {direction} degrees lies in isInTalkRange's range {limits['limit']} and outside both "
	                  f"{limits['limitWithoutPlusOne']} and {limits['limitCenterToCenter']}: the bound radii leave no band the '+ 1' alone admits")


# ---- the static data ------------------------------------------------------------------------------------------------------------------------

@dataclass(frozen=True)
class DialogNpc:
	npc_id: int
	name: str | None
	ai: str | None
	title_id: int
	talk_info: bool
	talk_distance: int
	is_dialog: bool
	func_dialogs: tuple[int, ...] | None
	sub_dialog_type: str | None
	bound_front: float
	bound_side: float


def _dialog_npcs(data: StaticData, wanted: set[int]) -> dict[int, DialogNpc]:
	npcs = {}
	for element in data.stream("npc_templates", "npc_template"):
		npc_id = java_int(element.get("npc_id"), "npc_template npc_id")
		if npc_id not in wanted:
			continue
		talk = element.find("talk_info")
		bound = element.find("bound_radius")  # NpcTemplate.getBoundRadius: BoundRadius.DEFAULT (0, 0, 0) without one
		funcs = None
		if talk is not None and talk.get("func_dialogs") is not None:
			funcs = tuple(java_int(t, f"npc {npc_id} func_dialogs") for t in talk.get("func_dialogs").split())
		npcs[npc_id] = DialogNpc(
			npc_id, element.get("name"), element.get("ai"), java_int(element.get("title_id"), f"npc {npc_id} title_id", 0), talk is not None,
			java_int(talk.get("distance"), f"npc {npc_id} talk_info distance", 2) if talk is not None else 2,
			talk is not None and talk.get("is_dialog") in ("true", "1"), funcs, talk.get("subdialog_type") if talk is not None else None,
			parse_float(bound.get("front", "0")) if bound is not None else 0.0, parse_float(bound.get("side", "0")) if bound is not None else 0.0)
	for npc_id in wanted - npcs.keys():
		raise OracleError(f"no npc_template with npc_id {npc_id}")
	return npcs


@dataclass(frozen=True)
class ItemInfo:
	item_id: int
	name: str | None
	level: int
	quality: str | None
	item_group: str
	price: int
	option_slot_bonus: int
	rnd_bonus: int
	rnd_count: int
	max_enchant: int
	max_enchant_bonus: int
	manastone_slots: int
	restrict: tuple[int, ...]
	race: str
	desc: int
	restrict_max: tuple[int, ...] | None  # ItemTemplate.maxLevelRestrictions: null without restrict_max


def _bytes(text: str, what: str) -> tuple[int, ...]:
	"""SpaceSeparatedBytesAdapter: space separated Java bytes."""
	values = tuple(java_int(t, what) for t in text.split())
	for value in values:
		if not -128 <= value <= 127:
			raise OracleError(f"{what}: {value} is not a Java byte")
	return values


def _items(data: StaticData, wanted: set[int]) -> dict[int, ItemInfo]:
	items = {}
	for element in data.stream("item_templates", "item_template"):
		item_id = java_int(element.get("id"), "item_template id")
		if item_id not in wanted:
			continue
		what = f"item_template {item_id}"
		restrict = element.get("restrict")
		restrict_max = element.get("restrict_max")
		items[item_id] = ItemInfo(
			item_id, element.get("name"), java_int(element.get("level"), f"{what} level", 0), element.get("quality"),
			element.get("item_group", "NONE"), java_int(element.get("price"), f"{what} price", 0),
			java_int(element.get("option_slot_bonus"), f"{what} option_slot_bonus", 0), java_int(element.get("rnd_bonus"), f"{what} rnd_bonus", 0),
			java_int(element.get("rnd_count"), f"{what} rnd_count", -1), java_int(element.get("max_enchant"), f"{what} max_enchant", 0),
			java_int(element.get("max_enchant_bonus"), f"{what} max_enchant_bonus", 0), java_int(element.get("m_slots"), f"{what} m_slots", 0),
			_bytes(restrict, f"{what} restrict") if restrict is not None else (1,) * 17,
			element.get("race", "PC_ALL"), java_int(element.get("desc"), f"{what} desc", 0),
			_bytes(restrict_max, f"{what} restrict_max") if restrict_max is not None else None)
	for item_id in wanted - items.keys():
		raise OracleError(f"item {item_id} has no item_template")
	return items


# ---- the report ---------------------------------------------------------------------------------------------------------------------------

@dataclass
class EconomyContext:
	rules: JavaEconomyRules
	data: StaticData
	config: dict[str, ConfigValue]
	prices: dict[str, dict]

	def cfg(self, key: str):
		return self.config[key].value


def _spots(ctx: EconomyContext, map_id: int, npc_ids: list[int]) -> dict[int, list[dict]]:
	rows = evaluate(load_groups(ctx.data, map_id), load_npc_templates(ctx.data), GameClock())
	by_npc: dict[int, list[dict]] = {npc_id: [] for npc_id in npc_ids}
	for row in rows:
		if row["npcId"] in by_npc:
			by_npc[row["npcId"]].append(row)
	return by_npc


def _effective_ai(npc: DialogNpc, row: dict) -> str | None:
	"""Creature.java:64-67: a spot's ai replaces the template's; SpawnTemplate.NO_AI means none."""
	if row.get("spotAi") is None:
		return npc.ai
	return None if row["spotAi"] == NO_AI else row["spotAi"]


def _start_page(ctx: EconomyContext, npc: DialogNpc, ai: str | None) -> dict:
	rules = ctx.rules
	if ai == POSTBOX_AI:
		return {"ai": "PostboxAI", "page": rules.pages["MAIL"], "questId": 0, "pageValue": rules.mailbox_states["REGULAR"],
		        "via": "PostboxAI.handleDialogStart: the mailbox state REGULAR, DialogPage.MAIL"}
	if ai != GENERAL_AI:
		raise OracleError(f"npc {npc.npc_id}: its AI {ai!r} answers DIALOG_START, which this oracle does not model (GeneralNpcAI and PostboxAI only)")
	if npc.title_id == TOWN_NPC_TITLE:
		raise OracleError(f"npc {npc.npc_id}: a town npc (title {TOWN_NPC_TITLE}) answers by the player's town residence, which is not modelled")
	if npc.sub_dialog_type is not None:
		raise OracleError(f"npc {npc.npc_id}: subdialog_type {npc.sub_dialog_type} makes isInteractionAllowed depend on the player, which is not modelled")
	if not npc.talk_info or (not npc.is_dialog and npc.func_dialogs is None):
		page, why = 0, "no conversation and no function dialog"
	elif npc.func_dialogs is not None:
		page, why = 10, "a function npc (func_dialogs)"
	else:
		page, why = None, "a dialog npc without functions: 10 with a quest interaction, 1352 for a daeva at an npc with an alternative dialog, else 1011"
	return {"ai": "GeneralNpcAI", "page": page, "questId": 0, "pageValue": 0, "via": f"TalkEventHandler.onTalk -> DialogPage.getStartPageId: {why}"}


def _function_arms(ctx: EconomyContext, npc: DialogNpc) -> list[dict]:
	rules = ctx.rules
	by_id = {value: name for name, value in rules.dialog_actions.items()}
	arms = []
	for action in npc.func_dialogs or ():
		name = by_id.get(action)
		if name in ("BUY", "SELL"):
			arms.append({"action": action, "name": name, "answer": "m5c-trade (SM_TRADELIST / SM_SELL_ITEM)"})
		elif name == "REMOVE_ITEM_OPTION":
			arms.append({"action": action, "name": name, "answer": "SM_DIALOG_WINDOW", "page": rules.page_by_action.get(action, 0),
			             "via": "DialogService.sendDialogWindow: DialogPage.getByActionId"})
		elif name == "RECOVERY":
			arms.append({"action": action, "name": name, "answer": "SM_QUESTION_WINDOW (see `recovery`)",
			             "question": rules.questions["STR_ASK_RECOVER_EXPERIENCE"]})
		elif name == "EXTEND_INVENTORY":
			arms.append({"action": action, "name": name, "answer": "SM_QUESTION_WINDOW (see `cube`)",
			             "question": rules.questions["STR_WAREHOUSE_EXPAND_WARNING"]})
		else:
			arms.append({"action": action, "name": None, "answer": None, "notModelled": "an arm this oracle does not model"})
	return arms


def talk_block(ctx: EconomyContext, npc: DialogNpc, rows: list[dict], reference: tuple[float, float, float] | None, far: float,
               direction: float = 0.0) -> dict:
	rules = ctx.rules
	spots = []
	for row in rows:
		fixed = row["spawned"] is True and not (row["flags"]["pool"] or row["flags"]["walker"] or row["flags"]["randomWalk"])
		spot = {"x": row["x"], "y": row["y"], "z": row["z"], "h": row["h"], "staticId": row["staticId"], "spawned": row["spawned"], "fixed": fixed,
		        "ai": _effective_ai(npc, row)}
		if reference is not None:
			spot["distanceFromReference"] = round(distance(*reference, row["x"], row["y"], row["z"]), 3)
		spots.append(spot)
	fixed_spots = [s for s in spots if s["fixed"]]
	if not fixed_spots:
		raise OracleError(f"npc {npc.npc_id} has no spawned spot on the map that stays where it is (a pool, a walker or a random walk moves it)")
	chosen = min(fixed_spots, key=lambda s: s.get("distanceFromReference", 0.0)) if reference is not None else fixed_spots[0]
	where = (chosen["x"], chosen["y"], chosen["z"])
	npc_bound = max(npc.bound_front, npc.bound_side)
	limits = talk_limits(npc.talk_distance, npc_bound, rules.player_bound)
	band = band_spot(where, limits, direction)
	near = _spot_at(where, f32(NEAR_DISTANCE), direction)
	far_spot = _spot_at(where, f32(far), direction)
	if _in(where, far_spot, limits["limit"]):
		raise OracleError(f"--far {far}: that spot is inside npc {npc.npc_id}'s talk range {limits['limit']}")
	too_far = "STR_DIALOG_TOO_FAR_TO_TALK" if npc.is_dialog else "STR_WAREHOUSE_TOO_FAR_FROM_NPC"

	def point(spot: tuple[float, float, float]) -> dict:
		return {"x": spot[0], "y": spot[1], "z": spot[2], "distance": round(distance(*where, *spot), 4),
		        "inTalkRange": _in(where, spot, limits["limit"]), "inRangeWithoutPlusOne": _in(where, spot, limits["limitWithoutPlusOne"]),
		        "inRangeCenterToCenter": _in(where, spot, limits["limitCenterToCenter"])}

	return {
		"npcId": npc.npc_id, "name": npc.name, "canInteract": npc.talk_info, "isDialogNpc": npc.is_dialog, "funcDialogs": list(npc.func_dialogs or []),
		"talkDistance": npc.talk_distance, "boundRadius": {"front": npc.bound_front, "side": npc.bound_side, "maxOfFrontAndSide": npc_bound},
		"playerBoundRadius": rules.player_bound, **limits,
		"spots": spots, "chosenSpot": chosen, "direction": direction,
		"bandSpot": point(band), "nearSpot": point(near), "farSpot": point(far_spot),
		# onDialogRequest returns before the range check for an npc that cannot interact: nothing is sent, near or far
		"outOfRange": {"message": too_far, "messageId": rules.messages[too_far], "window": None} if npc.talk_info else None,
		"startWindow": _start_page(ctx, npc, chosen["ai"]) if npc.talk_info else None,
		"functions": _function_arms(ctx, npc),
	}


def recovery_block(ctx: EconomyContext, exp_lost: int) -> dict:
	rules = ctx.rules
	if exp_lost < 0:
		raise OracleError("--recover-exp must be >= 0")
	if exp_lost == 0:
		return {"recoverableExp": 0, "question": None, "message": "STR_DONOT_HAVE_RECOVER_EXPERIENCE",
		        "messageId": rules.messages["STR_DONOT_HAVE_RECOVER_EXPERIENCE"]}
	factor, price = recovery_price(exp_lost)
	return {
		"recoverableExp": exp_lost, "factor": factor, "price": price,
		"question": {"id": rules.questions["STR_ASK_RECOVER_EXPERIENCE"], "params": [str(price), "", ""], "senderId": 0, "range": 0},
		"yes": {"kinahDelta": -price, "expDelta": exp_lost, "recoverableExpAfter": 0,
		        "messages": [{"name": "STR_GET_EXP2", "id": rules.messages["STR_GET_EXP2"], "value": exp_lost},
		                     {"name": "STR_SUCCESS_RECOVER_EXPERIENCE", "id": rules.messages["STR_SUCCESS_RECOVER_EXPERIENCE"]}]},
		"notEnoughKinah": {"name": "STR_MSG_NOT_ENOUGH_KINA", "id": rules.messages["STR_MSG_NOT_ENOUGH_KINA"], "value": price},
	}


def _cube_templates(data: StaticData) -> dict[int, list[tuple[int, int]]]:
	"""CubeExpandData.afterUnmarshal: npc id -> the <expand level price> list of its expansion_npc (a later template replaces an earlier one)."""
	templates: dict[int, list[tuple[int, int]]] = {}
	for element in data.children("cube_expander", "expansion_npc"):
		expands = [(java_int(e.get("level"), "expand level"), java_int(e.get("price"), "expand price")) for e in element.findall("expand")]
		for npc_id in element.get("ids", "").split():
			templates[java_int(npc_id, "expansion_npc ids")] = expands
	return templates


def cube_block(ctx: EconomyContext, npc_id: int, npc_expands: int, quest_expands: int, item_expands: int) -> dict:
	rules = ctx.rules
	templates = _cube_templates(ctx.data)
	if npc_id not in templates:
		return {"npcId": npc_id, "template": None, "answer": "nothing (log: Cube expansion template could not be found)"}
	expands = templates[npc_id]
	levels = [level for level, _ in expands]
	result = {"npcId": npc_id, "template": [{"level": level, "price": price} for level, price in expands], "npcExpands": npc_expands,
	          "questExpands": quest_expands, "itemExpands": item_expands}
	limit = ctx.cfg("gameserver.cube.expansion_limit")
	if npc_expands + quest_expands + item_expands + 1 < 0:
		return {**result, "answer": "nothing (canExpand: a negative expansion count)"}
	if npc_expands + quest_expands + item_expands + 1 > limit:
		return {**result, "answer": "STR_EXTEND_INVENTORY_CANT_EXTEND_MORE", "messageId": rules.messages["STR_EXTEND_INVENTORY_CANT_EXTEND_MORE"]}
	new = npc_expands + 1
	if new < min(levels, default=0):
		return {**result, "answer": "STR_EXTEND_INVENTORY_CANT_EXTEND_DUE_TO_MINIMUM_EXTEND_LEVEL_BY_THIS_NPC",
		        "messageId": rules.messages["STR_EXTEND_INVENTORY_CANT_EXTEND_DUE_TO_MINIMUM_EXTEND_LEVEL_BY_THIS_NPC"]}
	price = next((p for level, p in expands if level == new), None)
	maximum = min(max(levels, default=0), ctx.cfg("gameserver.npcexpands.limit"))
	if price is None or new > maximum:
		return {**result, "answer": "STR_EXTEND_INVENTORY_CANT_EXTEND_MORE_DUE_TO_MAXIMUM_EXTEND_LEVEL_BY_THIS_NPC",
		        "messageId": rules.messages["STR_EXTEND_INVENTORY_CANT_EXTEND_MORE_DUE_TO_MAXIMUM_EXTEND_LEVEL_BY_THIS_NPC"], "maximum": maximum}
	return {**result, "answer": "SM_QUESTION_WINDOW", "price": price,
	        "question": {"id": rules.questions["STR_WAREHOUSE_EXPAND_WARNING"], "params": [str(price), "", ""], "senderId": 0, "range": 0},
	        "yes": {"kinahDelta": -price, "npcExpandsAfter": new, "cubeSlotsAdded": 9,
	                "message": {"name": "STR_EXTEND_INVENTORY_SIZE_EXTENDED", "id": rules.messages["STR_EXTEND_INVENTORY_SIZE_EXTENDED"], "value": 9},
	                "smCubeUpdate": {"action": 0, "storage": 0, "npcExpands": new, "questExpands": quest_expands, "itemExpands": item_expands}},
	        "notEnoughKinah": {"name": "STR_WAREHOUSE_EXPAND_NOT_ENOUGH_MONEY", "id": rules.messages["STR_WAREHOUSE_EXPAND_NOT_ENOUGH_MONEY"]}}


def mail_block(ctx: EconomyContext, spec: str, items: dict[int, ItemInfo]) -> dict:
	"""One letter, ITEM:COUNT:KINAH[:express] (ITEM 0 for none)."""
	rules = ctx.rules
	parts = spec.split(":")
	if len(parts) not in (3, 4) or (len(parts) == 4 and parts[3] != "express"):
		raise OracleError(f"--mail {spec!r}: expected ITEM:COUNT:KINAH[:express]")
	item_id, count, kinah = (java_int(p, f"--mail {spec}") for p in parts[:3])
	express = len(parts) == 4
	if kinah < 0:
		return {"spec": spec, "answer": "nothing (sendMail: an audit for negative kinah)"}
	base = rules.mail["expressBaseCost"] if express else rules.mail["normalBaseCost"]
	factor = rules.mail["expressCostFactor"] if express else rules.mail["normalCostFactor"]
	item_commission = 0
	quality_rate = None
	if item_id != 0 and count > 0:
		item = items[item_id]
		quality_rate = rules.mail["qualityRates"].get(item.quality or "", rules.mail["qualityRates"]["default"])
		# (long) (price * rate * count * costFactor): a long times a float is a float, and so is every later product
		item_commission = to_long(f32(f32(f32(f32(item.price) * quality_rate) * f32(count)) * f32(factor)))
	kinah_commission = to_long(f32(f32(f32(kinah) * rules.mail["kinahRate"]) * f32(factor))) if kinah > 0 else 0
	service_base = base + kinah_commission + item_commission
	per_race = {race: {"servicePrice": service_price(service_base, prices), "total": service_price(service_base, prices) + kinah}
	            for race, prices in ctx.prices.items()}
	return {"spec": spec, "itemId": item_id, "count": count, "kinah": kinah, "letterType": "EXPRESS" if express else "NORMAL",
	        "baseCost": base, "costFactor": factor, "qualityRate": quality_rate, "itemCommission": item_commission,
	        "kinahCommission": kinah_commission, "serviceBase": service_base, "byRace": per_race}


def _effective_level(ctx: EconomyContext, quality: str | None, level: int) -> int:
	return level + ctx.rules.effective_level_bonus[quality] if quality in ctx.rules.effective_level_bonus else 0


def break_item(ctx: EconomyContext, item: ItemInfo, equip_type: str) -> dict:
	weapon, armour = equip_type == "WEAPON", equip_type == "ARMOR"
	if not weapon and not armour:
		return {"breakable": False, "answer": "false (AuditLogger: tried to break down incompatible item type)"}
	effective = _effective_level(ctx, item.quality, item.level)
	if effective == 0:
		raise OracleError(f"item {item.item_id}: calculateEffectiveLevel answers 0 for quality {item.quality} (breakItem throws IllegalArgumentException)")
	low, high = effective + 0 + (5 if weapon else 0), effective + 10 + (5 if weapon else 0)
	thresholds = [(name, stone_id, _effective_level(ctx, ctx.rules.stones[name][1], ctx.rules.stones[name][0])) for name, stone_id in BREAK_STONES]
	outcomes: dict[int, int] = {}
	for roll in range(low, high + 1):
		stone = next((stone_id for _, stone_id, threshold in thresholds if roll >= threshold), BREAK_STONE_ALPHA)
		outcomes[stone] = outcomes.get(stone, 0) + 1
	count_range = [2, 5] if weapon else [1, 3]
	return {"breakable": True, "effectiveLevel": effective, "rollRange": [low, high],
	        "thresholds": [{"stone": name, "itemId": stone_id, "effectiveLevel": t} for name, stone_id, t in thresholds],
	        "stones": [{"itemId": stone_id, "probability": n / (high - low + 1)} for stone_id, n in sorted(outcomes.items())],
	        "countRange": count_range, "message": "STR_DECOMPOSE_ITEM_SUCCEED", "messageId": ctx.rules.messages["STR_DECOMPOSE_ITEM_SUCCEED"]}


def learned_skills(data: StaticData, enums: JavaEnums, race: str, player_class: str, level: int) -> set[int]:
	"""
	The ids of the skills SkillLearnService.learnNewSkills(player, 1, level) teaches (SkillLearnService.java:60-93), the ones creation and every
	level change since teach together (the module docstring): from `level` down to 1, below level 10 an advanced class first learns its starting
	class's rows of the level, then the class's own; autoLearnSkills takes the SkillTreeData.getTemplatesFor rows - `minLevel` the level, classId
	the class or absent (afterUnmarshal gives a class-less row to every class), race the character's or PC_ALL - that are autolearn, skipping
	30001 when THE CLASS PASSED IN is not a starting class. The daeva branch (30001 -> 30002) is not modelled: item_block refuses to answer for a
	group that requires either skill.
	"""
	rows = []
	for element in data.children("skill_tree", "skill"):
		class_id = element.get("classId")
		if class_id is not None and class_id not in enums.classes:
			raise OracleError(f"skill_tree: classId {class_id!r} is not a PlayerClass")
		rows.append((class_id, element.get("race", "PC_ALL"), java_int(element.get("minLevel"), "skill minLevel"),
		             java_boolean(element.get("autolearn")), java_int(element.get("skillId"), "skill skillId")))
	starting = enums.starting_classes[player_class]
	learned: set[int] = set()
	for at in range(level, 0, -1):
		for cls in ([starting] if at < 10 and starting != player_class else []) + [player_class]:
			cls_is_starting = enums.starting_classes[cls] == cls
			for class_id, skill_race, min_level, autolearn, skill_id in rows:
				if min_level != at or class_id not in (None, cls) or skill_race not in (race, "PC_ALL") or not autolearn:
					continue
				if skill_id == 30001 and not cls_is_starting:
					continue
				learned.add(skill_id)
	return learned


def _equip_block(ctx: EconomyContext, item: ItemInfo, slots: int, player_class: str, race: str, level: int, experience: list[int],
                 learned: set[int]) -> dict:
	"""Equipment.equipItem's modelled checks in its order (the module docstring); the first that fails is `refusedBy`."""
	enums = ctx.rules.enums
	classes = list(enums.classes)
	if player_class not in classes:
		raise OracleError(f"unknown PlayerClass {player_class}")
	ordinal = classes.index(player_class)
	for name, values in (("restrict", item.restrict), ("restrict_max", item.restrict_max)):
		if values is not None and ordinal >= len(values):
			raise OracleError(f"item {item.item_id}: {name} has {len(values)} values, PlayerClass.{player_class} is ordinal {ordinal} "
			                  "(ArrayIndexOutOfBoundsException)")
	related = item.restrict[ordinal] > 0
	starting = enums.starting_classes[player_class]
	if not related and starting != player_class:
		related = item.restrict[classes.index(starting)] > 0
	required = item.restrict[ordinal] if item.restrict[ordinal] != 0 else -1
	if not 0 <= required - 1 < len(experience) and required != -1:
		raise OracleError(f"item {item.item_id}: required level {required} is not in the experience table")
	max_level = item.restrict_max[ordinal] if item.restrict_max is not None else 0
	required_skills = ctx.rules.required_skills.get(item.item_group, ())
	if any(skill in DAEVA_GATHERING_SKILLS for skill in required_skills):
		raise OracleError(f"item {item.item_id}: ItemGroup.{item.item_group} requires one of {list(required_skills)}, which learnNewSkills' daeva "
		                  "branch swaps (not modelled)")
	known = [skill for skill in required_skills if skill in learned]
	# (check, passes, the SM_SYSTEM_MESSAGE it sends or None for a refusal without a packet)
	checks = (
		("class", related, "STR_CANNOT_USE_ITEM_INVALID_CLASS"),
		("requiredLevel", required != -1 and required <= level, "STR_CANNOT_USE_ITEM_TOO_LOW_LEVEL_MUST_BE_THIS_LEVEL"),
		("maxLevel", max_level == 0 or level <= max_level, "STR_CANNOT_USE_ITEM_TOO_HIGH_LEVEL"),
		("race", item.race in ("PC_ALL", race), "STR_CANNOT_USE_ITEM_INVALID_RACE"),
		("equipSkill", not required_skills or bool(known), None),
		("itemSlot", slots != 0, None),
	)
	refused = next(((check, message) for check, passes, message in checks if not passes), None)
	return {
		"passes": refused is None, "refusedBy": refused[0] if refused else None, "message": refused[1] if refused else None,
		"messageId": ctx.rules.messages[refused[1]] if refused and refused[1] else None,
		"class": player_class, "classOrdinal": ordinal, "race": race, "level": level, "classSpecific": related, "requiredLevel": required,
		"startExpOfRequiredLevel": (0 if required <= 0 else experience[required - 1]) if required != -1 else None,
		"maxLevelRestrict": max_level, "itemRace": item.race, "requiredSkills": list(required_skills), "knownRequiredSkills": known,
		"notModelled": "gender, rank and the cube space of a two-handed weapon (Equipment.java:92-106, between the race and the skill checks); "
		               "the slot the client asks for, stigma, soul binding and identification (:111-140, 163-167)",
	}


def item_block(ctx: EconomyContext, item: ItemInfo, player_class: str, level: int, experience: list[int], race: str = "ELYOS",
               learned: set[int] | None = None) -> dict:
	enums = ctx.rules.enums
	if item.item_group not in enums.item_groups:
		raise OracleError(f"item {item.item_id}: unknown ItemGroup {item.item_group}")
	slots, equip_type = enums.item_groups[item.item_group]
	max_tune = item.rnd_count
	if slots == 0:
		max_tune = 0
	elif max_tune == -1 and item.max_enchant_bonus == 0 and item.option_slot_bonus == 0 and item.rnd_bonus == 0:
		max_tune = 0
	can_tune = max_tune != 0
	if player_class not in enums.classes:
		raise OracleError(f"unknown PlayerClass {player_class}")  # before learned_skills looks its starting class up
	if learned is None:
		learned = learned_skills(ctx.data, enums, race, player_class, level)
	equip = _equip_block(ctx, item, slots, player_class, race, level, experience, learned)
	sql_default = ctx.rules.tune_count_sql_default
	identification = {
		"rndCount": item.rnd_count, "maxTuneCount": max_tune, "canTune": can_tune,
		# Item.java:84-85 for a new item; the DAO's row keeps its tune_count unless it is -1 of a template that cannot tune (Item.java:128-130)
		"newItemTuneCount": -1 if can_tune else 0, "newItemIdentified": not can_tune,
		"sqlDefaultTuneCount": sql_default, "sqlDefaultLoadsIdentified": sql_default != -1 or not can_tune,
		"seedTuneCountForUnidentified": -1 if can_tune else None,
	}
	if can_tune:
		identification.update({
			"optionalSocketsRange": [0, item.option_slot_bonus], "enchantBonusRange": [0, item.max_enchant_bonus],
			"statBonusId": 0 if item.rnd_bonus == 0 else None,
			"statBonusNotModelled": None if item.rnd_bonus == 0 else f"rnd_bonus {item.rnd_bonus}: ItemRandomBonusData's draw over its set",
			"tuneCountAfter": 0, "animation": {"time": 5000, "start": 9, "end": 10, "abort": 11},
			"message": "STR_MSG_ITEM_IDENTIFY_SUCCEED", "messageId": ctx.rules.messages["STR_MSG_ITEM_IDENTIFY_SUCCEED"]})
	return {
		"itemId": item.item_id, "name": item.name, "level": item.level, "quality": item.quality, "itemGroup": item.item_group, "equipType": equip_type,
		"price": item.price, "desc": item.desc, "manastoneSlots": item.manastone_slots, "maxEnchant": item.max_enchant, "race": item.race,
		"identification": identification,
		"breakItem": break_item(ctx, item, equip_type),
		"equip": equip,
	}


def economy_report(data: StaticData, java_src: Path, config: dict[str, ConfigValue], map_id: int = 210010000, npc_ids: list[int] = (),
                   near: tuple[float, float, float] | None = None, far: float = 10.0, recover_exp: int | None = None,
                   npc_expands: int = 0, quest_expands: int = 0, item_expands: int = 0, mails: list[str] = (), item_ids: list[int] = (),
                   player_class: str = "MAGE", level: int = 1, races: tuple[str, ...] = RACES, influences: dict[str, int] | None = None,
                   handlers_dir: Path | None = None, commons_src: Path | None = None, sql_file: Path | None = None,
                   rules: JavaEconomyRules | None = None, direction: float = 0.0, player_race: str = "ELYOS") -> dict:
	"""`data` must be read with the configured gameserver.country.code; `handlers_dir` is data/handlers, `commons_src` commons/src and `sql_file`
	sql/aion_gs.sql (default: beside the game-server tree of `java_src`); `direction` the angle in degrees, counter-clockwise from +x, along which
	the band, near and far spots lie; `player_class`, `player_race` and `level` the character the item equip checks are made for (`races` are
	the races of the price blocks)."""
	java_src = Path(java_src)
	handlers_dir = Path(handlers_dir) if handlers_dir is not None else java_src.parent / "data" / "handlers"
	commons_src = Path(commons_src) if commons_src is not None else java_src.parent.parent / "commons" / "src"
	sql_file = Path(sql_file) if sql_file is not None else java_src.parent / "sql" / "aion_gs.sql"
	rules = rules if rules is not None else JavaEconomyRules.read(java_src, handlers_dir, commons_src, sql_file)
	check_country_code(data, config["gameserver.country.code"].value)
	if far <= 0:
		raise OracleError("--far must be positive")
	if level < 1:
		raise OracleError("--level must be at least 1")
	if player_race not in RACES:
		raise OracleError(f"--race {player_race}: expected one of {', '.join(RACES)}")
	if player_class not in rules.enums.classes:
		raise OracleError(f"unknown PlayerClass {player_class}")
	influences = dict(influences or {})
	prices = {race: race_prices(config, race, influences.get(race)) for race in races}
	ctx = EconomyContext(rules, data, config, prices)
	mail_items = set()
	for spec in mails:
		head = spec.split(":")[0]
		if head.lstrip("-").isdigit() and int(head) != 0:
			mail_items.add(int(head))
	items = _items(data, set(item_ids) | mail_items)
	experience = [_java_long(e.text, "player_experience_table exp") for e in data.children("player_experience_table", "exp")]
	learned = learned_skills(data, rules.enums, player_race, player_class, level) if item_ids else set()

	npcs = _dialog_npcs(data, set(npc_ids))
	spots = _spots(ctx, map_id, list(npc_ids))
	talk = []
	reference = tuple(f32(c) for c in near) if near is not None else None
	for npc_id in npc_ids:
		block = talk_block(ctx, npcs[npc_id], spots[npc_id], reference, far, direction)
		if reference is None:
			chosen = block["chosenSpot"]
			reference = (chosen["x"], chosen["y"], chosen["z"])
			block = talk_block(ctx, npcs[npc_id], spots[npc_id], reference, far, direction)
		talk.append(block)
	for block in talk:  # which OTHER reported npc could also be talked to from each spot (a gate that stands there may want to know)
		for key in ("bandSpot", "nearSpot", "farSpot"):
			spot = (block[key]["x"], block[key]["y"], block[key]["z"])
			block[key]["otherNpcsInTalkRange"] = [other["npcId"] for other in talk if other is not block and _in(
				(other["chosenSpot"]["x"], other["chosenSpot"]["y"], other["chosenSpot"]["z"]), spot, other["limit"])]
	cube = [cube_block(ctx, npc_id, npc_expands, quest_expands, item_expands) for npc_id in npc_ids
	        if rules.dialog_actions["EXTEND_INVENTORY"] in (npcs[npc_id].func_dialogs or ())]
	removal = None
	if any(rules.dialog_actions["REMOVE_ITEM_OPTION"] in (npcs[n].func_dialogs or ()) for n in npc_ids) or not npc_ids:
		removal = {"basePrice": rules.removal_base, "byRace": {race: service_price(rules.removal_base, p) for race, p in prices.items()},
		           "messages": {"succeed": rules.messages["STR_REMOVE_ITEM_OPTION_SUCCEED"],
		                        "notEnoughKinah": rules.messages["STR_REMOVE_ITEM_OPTION_NOT_ENOUGH_GOLD"]}}
	return {
		"format": FORMAT,
		"version": 1,
		"map": map_id,
		"config": {key: value.as_json() for key, value in config.items()},
		"prices": prices,
		"assumptions": [
			"no quest handler answers DialogAction.USE_OBJECT at a reported npc before DialogPage.getStartPageId (TalkEventHandler.java:26-27): "
			"the M5d quest oracle (m5d-quest, m5d-quests) owns the quest registry",
			f"every spot lies {direction:g} degrees counter-clockwise from +x (--direction) from the npc at the npc's z, the position the player's "
			"object holds (geo is not consulted: m5c-plan.md D12)",
			"the player is not trading, is not hidden, knows the npc and is not a summon owner's stranger (CM_SHOW_DIALOG.java:33-41, "
			"DialogService.isInteractionAllowed)",
			"the character knows exactly the autolearn skills of levels 1 to its level (learnNewSkills at creation and on every level change, the "
			"enter-world one from players.old_level included) - no skill book or stigma skill, and `learnedSkills` without the daeva branch that "
			"turns 30001 into 30002 from level 10 - and passes equipItem's gender, rank and cube-space checks",
		],
		"talk": talk,
		"recovery": recovery_block(ctx, recover_exp) if recover_exp is not None else None,
		"cube": cube,
		"manastoneRemoval": removal,
		"mail": [mail_block(ctx, spec, items) for spec in mails],
		"items": [item_block(ctx, items[item_id], player_class, level, experience, player_race, learned) for item_id in item_ids],
		"character": {"class": player_class, "race": player_race, "level": level,
		              "learnedSkills": sorted(learned) if item_ids else None},
	}
