"""m5c-economy's C19 blocks (m5c-plan.md G-01's rest, §10.1 Seeds, §10.2 C19, §10.3 X17-X21a; stage 1 of the harness-b lane): the Daeva seed and
what its enter world teaches, and the crafting case in the capital - the profession master, the vendors of the recipe's components, the craft
tool's static objects with the spots at the gate's distances, and C19's exact kinah. The recipe, the master's price and the craft ranges are
m5c-craft's (craft.py, composed through its CraftContext, whose JavaCraftRules pin CraftService, CM_CRAFT, CraftingTask, SkillLearnService.
onLearnSkill, RecipeService.autoLearnRecipes, RecipeData.getAutolearnRecipes, PlayerSkillEntry.is*Skill and CraftSkillUpdateService.learnSkill),
the component prices m5c-trade's (trade.py) and the talk spots economy.py's.

Java rules, each with the method it comes from (game-server/src/com/aionemu/gameserver):
- the Daeva seed (`daeva`): PlayerDAO loads the class before the exp (PlayerDAO.java:142-143) and PlayerCommonData.setExp, offline, takes
  maxLevel = pxt.getMaxLevel() when updateDaeva() finds the ascension quest COMPLETE (1006 for the Elyos, 2008 for the Asmodians, checked both, on
  an advanced class only) or the exp is ABOVE getStartExpForLevel(10), else 10; exp = min(exp, getStartExpForLevel(maxLevel)) and level =
  min(getLevelForExp(exp), maxLevel - 1) (PlayerCommonData.java:273-293, 588-610; PlayerExperienceTable.java:29-56). So the seed at exactly
  getStartExpForLevel(10) is level 10 with the quest and 9 without it (m5c0-client-session.md F-1), and the exp one above it is level 10 without
  the quest but not a Daeva;
- its enter world (`daeva.enterWorld`): PlayerEnterWorldService.java:204 runs onLevelChange(PlayerDAO.getOldCharacterLevel, level), old_level
  being the level PlayerLeaveWorldService stored at the last quit (PlayerLeaveWorldService.java:148); onLevelChange (PlayerController.java:568-599)
  does nothing for equal levels and otherwise calls learnNewSkills(minNewLevel, newLevel), minNewLevel = old + 1 (old - 1 for a level loss);
  learnNewSkills (SkillLearnService.java:60-75) walks the levels down, teaching below level 10 the starting class's autolearn rows before the
  class's own (economy.learned_skills' row filter: getTemplatesFor's class, class-less and race rows; 30001 skipped for the class passed in when
  it is no starting class), each at its skill template's lvl (SkillLearnTemplate.getSkillLevel); PlayerSkillList.addSkill adds a missing skill or
  raises a lower level and calls SkillLearnService.onLearnSkill, which for a crafting or morph skill (PlayerSkillEntry: 40001-40010, 40009 the
  morph) calls RecipeService.autoLearnRecipes -> RecipeList.addRecipe for every RecipeData.getAutolearnRecipes(race, skill, level) the player
  does not know, each with an SM_LEARN_RECIPE (RecipeList.java:29-35); then, from level 10, a Daeva holding 30001 gets 30002 at 30001's level
  and loses 30001 (SkillLearnService.removeSkill: SM_SKILL_REMOVE). The skills the character had are the autolearn skills of levels 1 to
  old_level of its STARTING class (learnNewSkills at creation and at every level change as that class, the assumption economy.learned_skills
  makes); the burst's SM_SKILL_LIST (PlayerEnterWorldService.java:228-230) and SM_RECIPE_LIST (:316) carry the result;
- the master (`craft.master`): CraftSkillUpdateService.learnSkill (DialogService's COMBINE_SKILL_LEVELUP arm): nothing below level 10, else the
  question SM_QUESTION_WINDOW(STR_CRAFT_ADDSKILL_CONFIRM, 0, 0, professionName, String.valueOf(price)) whose name is the skill template's l10n
  (Profession.getClientName() for a skill not learned yet, ChatUtil.l10n: "$" and the two chars of `nameId << 1 | 1`); yes pays the price and
  adds the skill at level 1, whose onLearnSkill broadcasts SM_ACTION_ANIMATION(player, CRAFT_LEVEL_UP) (level 1 of a crafting skill; the id is
  ActionAnimation's) and teaches the level-1 autolearn recipes of the race;
- the craft's SM_CRAFT_UPDATEs (`craft.recipe`): every analyze tick carries the product bar's executionSpeed and showBarDelay
  (CraftingTask.sendInteractionUpdate), the start pair, the end and sendCancelCraft's update what m5c-craft's `packets` state;
- the tool (`craft.tool`): SpawnEngine.spawnInstance hands a handler="STATIC" group to StaticObjectSpawnManager.spawnTemplate, which spawns a
  StaticObject on EVERY spot of a group without a pool (a pool, a difficult id or a temporary spawn is refused here) with the ITEM template of the
  group's npc_id (none without one); a player sees it as SM_GATHERABLE_INFO, which carries the object id, the spot's static id and the template id
  (PlayerController.see, SM_GATHERABLE_INFO.java); the spots at the given distances lie along --direction at the tool's z, each checked with
  CM_CRAFT's centre-to-centre range and checkCraft's range plus both bound radii (m5c-craft's `craft.station`, in float arithmetic);
- the kinah (`craft.exactKinah`): the master's price plus, for every component of the recipe's first alternative that a vendor with a fixed spot
  on the map sells, m5c-trade's kinah for the component's quantity at the vendor nearest the master; a component no vendor there sells is a seed
  item (`craft.seedItems`).

Not modelled (OracleError, exit 2): a Java member above that changed (fingerprinted whole, as economy.py does), a starting class for --daeva, an
old level outside 1..9 (the stored skills are modelled as a starting class's), a recipe of another race, a skill that is no crafting skill (the morph skill needs no master and no tool), a map
where no master of the profession or no tool spot stands, a tool group with a pool, a difficult id or a temporary spawn, and everything economy,
craft and trade refuse. Player state beyond the arguments is not: quests (their onLevelChanged), the repose and salvation energy, a skill book.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path

from staticdata_oracle import OracleError

from m5a.creation import RACES
from m5a.data import StaticData, java_boolean, java_int
from m5a.javafloat import distance, f32, in_range
from m5a.spawns import GameClock, evaluate, load_groups, load_npc_templates

from .craft import CraftContext, autolearn_recipes, recipe_report, skill_report
from .economy import EconomyContext, _check_members, _dialog_npcs, _search, _spot_at, talk_block
from .trade import JavaTradeRules, TradeData, _read, _strip_comments, trade_report

# below com/aionemu/gameserver, fingerprinted like economy.MODELLED_MEMBERS (craft_java pins the rest of the craft and learn path)
C19_MEMBERS = (
	# the Daeva level at load
	("model/gameobjects/player/PlayerCommonData.java", "method setExp", "8f1c76d85106eab4"),
	("model/gameobjects/player/PlayerCommonData.java", "method updateDaeva", "560b6a99faabf08c"),
	("dataholders/PlayerExperienceTable.java", "method getLevelForExp", "034c55faa5b0c634"),
	("dataholders/PlayerExperienceTable.java", "method getMaxLevel", "79d8bff9cde0b3a3"),
	# what the enter world's onLevelChange teaches
	("model/skill/PlayerSkillList.java", "method addSkill(Player player, int skillId, int skillLevel, boolean isTemporary)", "9fc4c0b8b3e01cb1"),
	("model/skill/PlayerSkillList.java", "method removeSkill", "b468e43d795ec841"),
	("services/SkillLearnService.java", "method removeSkill", "3ecdc03591c9278a"),
	("skillengine/model/SkillLearnTemplate.java", "method getSkillLevel", "888800d4a488c741"),
	("model/gameobjects/player/RecipeList.java", "method addRecipe", "fa39a8b5bb3c3fd9"),
	# the master's arm and question
	("services/DialogService.java", "case COMBINE_SKILL_LEVELUP", "3131ecef00d030d2"),
	("model/craft/Profession.java", "method getClientName()", "28b18e7d47a2459e"),
	# the tool's static objects and how a player sees them
	("spawnengine/SpawnEngine.java", "method spawnInstance(WorldMapInstance instance, byte difficultId, int ownerId, EventTemplate eventTemplate)",
	 "d76082493a981f84"),
	("spawnengine/StaticObjectSpawnManager.java", "method spawnTemplate", "00c6fff698ee5268"),
	("controllers/PlayerController.java", "method see", "7e684dab788e9a2e"),
	("network/aion/serverpackets/SM_GATHERABLE_INFO.java", "method writeImpl", "2e97ee63fa75b78a"),
)

# statements whose literals this module follows, each present verbatim (comments and white space removed)
C19_STATEMENTS = (
	("dao/PlayerDAO.java", 'cd.setExp(resultSet.getLong("exp"));'),
	("services/player/PlayerLeaveWorldService.java", "PlayerDAO.storeOldCharacterLevel(player.getObjectId(), player.getLevel());"),
	("services/player/PlayerEnterWorldService.java",
	 "player.getController().onLevelChange(PlayerDAO.getOldCharacterLevel(player.getObjectId()), player.getLevel());"),
	("skillengine/model/SkillTemplate.java", "@XmlAttribute private int lvl;"),
)

C19_SOURCES = tuple(sorted({relative for relative, _, _ in C19_MEMBERS} | {relative for relative, _ in C19_STATEMENTS} | {
	"model/DialogAction.java", "network/aion/serverpackets/SM_QUESTION_WINDOW.java", "services/SkillLearnService.java",
	"services/craft/CraftSkillUpdateService.java", "model/animations/ActionAnimation.java"}))

DEFAULT_DISTANCES = (3.0, 7.0, 12.0)  # m5c-plan.md C19: CM_CRAFT from 7 m, from 12 m, then from 3 m


@dataclass(frozen=True)
class JavaC19Rules:
	ascension_quests: dict[str, int]  # PlayerCommonData.updateDaeva: race -> the quest whose COMPLETE makes a Daeva
	daeva_level: int                  # setExp: getStartExpForLevel(N) is the exp above which the level cap goes away
	non_daeva_max_level: int          # setExp: the maxLevel of a non-Daeva (level at most maxLevel - 1)
	daeva_swap: tuple[int, int, int]  # learnNewSkills: from toLevel >= N a Daeva's first skill is replaced by the second
	combine_action: int               # DialogAction.COMBINE_SKILL_LEVELUP
	question_id: int                  # SM_QUESTION_WINDOW.STR_CRAFT_ADDSKILL_CONFIRM
	learn_min_level: int              # CraftSkillUpdateService.learnSkill: nothing below this level
	craft_level_up_animation: int     # ActionAnimation.CRAFT_LEVEL_UP's id, the one SM_ACTION_ANIMATION writes (SM_ACTION_ANIMATION.writeImpl)

	@staticmethod
	def read(java_src: Path) -> "JavaC19Rules":
		base = Path(java_src) / "com" / "aionemu" / "gameserver"
		texts: dict[str, str] = {}
		_check_members(base, C19_MEMBERS, texts)
		for relative, statement in C19_STATEMENTS:
			if relative not in texts:
				texts[relative] = _read(base / relative)
			if re.sub(r"\s+", "", statement) not in re.sub(r"\s+", "", _strip_comments(texts[relative])):
				raise OracleError(f"{relative} no longer contains `{statement}`: the Java source does not have the shape this oracle was written against")
		for relative in C19_SOURCES:
			if relative not in texts:
				texts[relative] = _read(base / relative)
		common = _strip_comments(texts["model/gameobjects/player/PlayerCommonData.java"])
		quests = {"ELYOS": int(_search(common, r"elyAscentQuestStatus\s*=\s*qsl\.getQuestState\((\d+)\)", "updateDaeva's Elyos quest").group(1)),
		          "ASMODIANS": int(_search(common, r"asmoAscentQuestStatus\s*=\s*qsl\.getQuestState\((\d+)\)", "updateDaeva's Asmodian quest").group(1))}
		cap = _search(common, r"exp\s*>\s*pxt\.getStartExpForLevel\((\d+)\)\)\s*\?\s*pxt\.getMaxLevel\(\)\s*:\s*(\d+)\s*;", "setExp's level cap")
		learn = _strip_comments(texts["services/SkillLearnService.java"])
		swap = _search(learn, r"toLevel\s*>=\s*(\d+)\s*&&\s*player\.getCommonData\(\)\.isDaeva\(\)\s*&&\s*player\.getSkillList\(\)\.isSkillPresent\((\d+)\)\)"
		                      r"\s*\{\s*if\s*\(!player\.getSkillList\(\)\.isSkillPresent\((\d+)\)\)", "learnNewSkills' Daeva swap")
		if not re.search(rf"addSkill\(player,\s*{swap.group(3)},\s*player\.getSkillList\(\)\.getSkillLevel\({swap.group(2)}\)\);\s*removeSkill\(player,\s*"
		                 rf"{swap.group(2)}\);", learn):
			raise OracleError("SkillLearnService.learnNewSkills: the Daeva swap does not have the shape this oracle was written against")
		action = int(_search(_strip_comments(texts["model/DialogAction.java"]), r"public\s+static\s+final\s+int\s+COMBINE_SKILL_LEVELUP\s*=\s*(\d+)\s*;",
		                     "DialogAction.COMBINE_SKILL_LEVELUP").group(1))
		question = int(_search(_strip_comments(texts["network/aion/serverpackets/SM_QUESTION_WINDOW.java"]),
		                       r"public\s+static\s+final\s+int\s+STR_CRAFT_ADDSKILL_CONFIRM\s*=\s*(\d+)\s*;", "SM_QUESTION_WINDOW.STR_CRAFT_ADDSKILL_CONFIRM")
		               .group(1))
		minimum = int(_search(_strip_comments(texts["services/craft/CraftSkillUpdateService.java"]),
		                      r"public\s+void\s+learnSkill\(Player\s+player,\s*Npc\s+npc\)\s*\{\s*if\s*\(player\.getLevel\(\)\s*<\s*(\d+)\)\s*return;",
		                      "learnSkill's level check").group(1))
		animation = int(_search(_strip_comments(texts["model/animations/ActionAnimation.java"]), r"\bCRAFT_LEVEL_UP\s*\(\s*(\d+)\s*\)",
		                        "ActionAnimation.CRAFT_LEVEL_UP").group(1))
		return JavaC19Rules(quests, int(cap.group(1)), int(cap.group(2)), (int(swap.group(1)), int(swap.group(2)), int(swap.group(3))), action,
		                    question, minimum, animation)


# ---- the Daeva --------------------------------------------------------------------------------------------------------------------------

def level_at_load(experience: list[int], exp: int, daeva: bool, rules: JavaC19Rules) -> int:
	"""PlayerCommonData.setExp at load (offline): the level the character gets, `daeva` meaning updateDaeva() found the quest."""
	table_max = len(experience)  # PlayerExperienceTable.getMaxLevel
	threshold = experience[rules.daeva_level - 1]
	max_level = table_max if daeva or exp > threshold else rules.non_daeva_max_level
	exp = min(exp, experience[max_level - 1] if max_level > 0 else 0)
	level = 0
	for i in range(table_max, 0, -1):  # getLevelForExp
		if exp >= experience[i - 1]:
			level = i
			break
	if table_max <= level:
		level = table_max - 1
	return min(level, max_level - 1)


def _skill_tree(data: StaticData, classes) -> list[tuple[str | None, str, int, bool, int]]:
	rows = []
	for element in data.children("skill_tree", "skill"):
		class_id = element.get("classId")
		if class_id is not None and class_id not in classes:
			raise OracleError(f"skill_tree: classId {class_id!r} is not a PlayerClass")
		rows.append((class_id, element.get("race", "PC_ALL"), java_int(element.get("minLevel"), "skill minLevel"),
		             java_boolean(element.get("autolearn")), java_int(element.get("skillId"), "skill skillId")))
	return rows


def _autolearn(rows, cls: str, level: int, race: str, starting_classes: dict[str, str]) -> list[int]:
	"""SkillLearnService.autoLearnSkills(player, level, cls, race) in its order: SkillTreeData.getTemplatesFor adds the rows of the class and
	the race first, then those of the class and PC_ALL (SkillTreeData.java:71-83), each list in skill_tree order (afterUnmarshal appends a
	class-less row to every class's list where it stands)."""
	result = []
	for wanted_race in (race, "PC_ALL"):
		for class_id, skill_race, min_level, autolearn, skill_id in rows:
			if min_level != level or class_id not in (None, cls) or skill_race != wanted_race or not autolearn:
				continue
			if skill_id == 30001 and starting_classes[cls] != cls:
				continue
			result.append(skill_id)
	return result


def skill_templates(data: StaticData, wanted: set[int]) -> dict[int, dict]:
	"""skill_id -> the template's lvl (SkillLearnTemplate.getSkillLevel) and nameId (the l10n of Profession.getClientName)."""
	found: dict[int, dict] = {}
	for element in data.stream("skill_data", "skill_template"):
		skill_id = java_int(element.get("skill_id"), "skill_template skill_id")
		if skill_id in wanted:
			found[skill_id] = {"lvl": java_int(element.get("lvl"), f"skill_template {skill_id} lvl", 0),
			                   "nameId": java_int(element.get("nameId"), f"skill_template {skill_id} nameId", 0), "name": element.get("name")}
	for skill_id in wanted - found.keys():
		raise OracleError(f"skill {skill_id} has no skill_template (SkillLearnTemplate.getSkillLevel: NullPointerException)")
	return found


def daeva_block(data: StaticData, rules: JavaC19Rules, enums, craft_ctx: CraftContext, race: str, player_class: str, old_level: int,
                experience: list[int]) -> dict:
	if player_class not in enums.classes:
		raise OracleError(f"--daeva {player_class}: unknown PlayerClass")
	starting = enums.starting_classes[player_class]
	if starting == player_class:
		raise OracleError(f"--daeva {player_class}: a starting class is never a Daeva (updateDaeva returns false for isStartingClass)")
	if race not in rules.ascension_quests:
		raise OracleError(f"--race {race}: updateDaeva knows the quests of {sorted(rules.ascension_quests)}")
	exp = experience[rules.daeva_level - 1]
	level = level_at_load(experience, exp, True, rules)
	if not 1 <= old_level < min(level, rules.non_daeva_max_level):
		raise OracleError(f"--daeva-old-level {old_level}: the level the last quit stored as the starting class, 1..{min(level, rules.non_daeva_max_level) - 1} "
		                  "(the skills the character had are a starting class's; a Daeva's own are not modelled)")
	rows = _skill_tree(data, enums.classes)
	# what the character knew: the autolearn skills of levels 1..old_level as its starting class (economy.learned_skills' assumption)
	stored: set[int] = set()
	for at in range(old_level, 0, -1):
		stored.update(_autolearn(rows, starting, at, race, enums.starting_classes))
	skills = set(stored)
	learned: list[dict] = []
	low = old_level + 1  # onLevelChange's minNewLevel for a level gain
	for at in range(level, low - 1, -1):
		for cls in ([starting] if at < rules.daeva_level and starting != player_class else []) + [player_class]:
			for skill_id in _autolearn(rows, cls, at, race, enums.starting_classes):
				if skill_id not in skills:
					skills.add(skill_id)
					learned.append({"skillId": skill_id, "level": at, "class": cls})
	templates = skill_templates(data, skills | {rules.daeva_swap[2]})
	swap = None
	first, replacement = rules.daeva_swap[1], rules.daeva_swap[2]
	if level >= rules.daeva_swap[0] and first in skills:
		added = replacement not in skills
		skills.add(replacement)
		skills.discard(first)
		swap = {"removed": first, "added": replacement if added else None, "level": templates[first]["lvl"], "smSkillRemove": first}
	# onLearnSkill of every added crafting or morph skill: its autolearn recipes (the character knew none: no crafting skill below level 10)
	s = craft_ctx.rules.skill
	stored_crafting = sorted(k for k in stored if k == s["morphSkill"] or s["craftMin"] <= k <= s["craftMax"])
	if stored_crafting:
		raise OracleError(f"the starting class already learns the crafting or morph skills {stored_crafting} below --daeva-old-level: its recipes "
		                  "would be known already (not modelled)")
	recipes: dict[int, list[int]] = {}
	known: set[int] = set()
	for entry in learned:
		skill_id = entry["skillId"]
		if skill_id == s["morphSkill"] or s["craftMin"] <= skill_id <= s["craftMax"]:
			ids = [r.id for r in autolearn_recipes(craft_ctx.recipes, race, skill_id, templates[skill_id]["lvl"]) if r.id not in known]
			known.update(ids)
			recipes[skill_id] = ids
	without_quest = level_at_load(experience, exp, False, rules)
	return {
		"class": player_class, "startingClass": starting, "race": race,
		"seed": {"playerClass": player_class, "exp": exp, "quest": {"id": rules.ascension_quests[race], "status": "COMPLETE"}, "oldLevel": old_level},
		"ascensionQuests": rules.ascension_quests,
		"level": level,
		"levelWithoutQuest": without_quest,
		"levelAboveThresholdWithoutQuest": {"exp": exp + 1, "level": level_at_load(experience, exp + 1, False, rules), "isDaeva": False},
		"enterWorld": {
			"onLevelChange": [old_level, level],
			"learnNewSkills": [low, level],
			"storedSkills": sorted(stored),
			"learnedSkills": learned,
			"daevaSwap": swap,
			"skills": sorted(skills),
			"skillLevels": {str(k): (templates[first]["lvl"] if swap and k == replacement and swap.get("added") else templates[k]["lvl"])
			                for k in sorted(skills)},
			"learnedRecipes": sorted(known),
			"recipesBySkill": {str(k): v for k, v in recipes.items()},
			"packets": "SM_SKILL_REMOVE for the swapped skill and one SM_LEARN_RECIPE per learned recipe during onLevelChange (PlayerEnterWorldService.java"
			           ":204), then the burst's SM_SKILL_LIST with `skills` and SM_RECIPE_LIST with `learnedRecipes` (:228-230, :316)",
		},
		"needs": ["the ascension quest's player_quests row must load (M5d's QuestState restore path, m5c0-client-session.md F-1)",
		          "the seed must be written while the account is disconnected (the account's characters are loaded at connect and saved at "
		          "logout, m5c0-client-session.md F-3)"],
	}


# ---- the craft in the capital -----------------------------------------------------------------------------------------------------------

def _fixed(row: dict) -> bool:
	return row["spawned"] is True and not (row["flags"]["pool"] or row["flags"]["walker"] or row["flags"]["randomWalk"])


def _talk(ctx: EconomyContext, npc_id: int, rows: list[dict], reference, far: float, direction: float) -> dict:
	npc = _dialog_npcs(ctx.data, {npc_id})[npc_id]
	block = talk_block(ctx, npc, rows, reference, far, direction)
	if reference is None:
		chosen = block["chosenSpot"]
		block = talk_block(ctx, npc, rows, (chosen["x"], chosen["y"], chosen["z"]), far, direction)
	return block


def _craft_level_up(rules: JavaC19Rules) -> dict:
	"""SkillLearnService.onLearnSkill's SM_ACTION_ANIMATION(player, CRAFT_LEVEL_UP), broadcast to the player too; the two-argument constructor
	writes 0 last (SM_ACTION_ANIMATION.java)"""
	return {"packet": "SM_ACTION_ANIMATION", "to": "everyone, the player too", "animation": "CRAFT_LEVEL_UP", "id": rules.craft_level_up_animation,
	        "levelOrObjectId": 0}


def _learn_animations(rules: JavaC19Rules, craft_ctx: CraftContext, skill_id: int) -> list[dict]:
	"""onLearnSkill (pinned by craft_java) for the learn's level 1: the animation at a profession skill's animation levels, level 1 only for a
	crafting skill - the rule m5c-craft's skill_up_packets applies to a level-up"""
	s = craft_ctx.rules.skill
	crafting = craft_ctx.skill_kind(skill_id) == "crafting"
	return [_craft_level_up(rules)] if 1 in s["levelUpAnimation"] and (1 != s["levelUpAnimationCraftingOnly"] or crafting) else []


def _craft_update(row: dict) -> dict:
	"""the SM_CRAFT_UPDATE fields m5c-craft states for one of the task's own updates (a bar it leaves out is not modelled)"""
	return {key: row[key] for key in ("action", "success", "failure", "executionSpeed", "delay") if key in row}


def craft_block(ctx: EconomyContext, rules: JavaC19Rules, craft_ctx: CraftContext, java_src: Path, recipe_id: int, map_id: int, tool_id: int,
                distances: tuple[float, ...], direction: float, race: str, far: float, handlers_dir: Path | None) -> dict:
	recipes = craft_ctx.recipes
	if recipe_id not in recipes:
		raise OracleError(f"--craft-recipe {recipe_id}: no recipe_template (startCrafting: NullPointerException)")
	recipe = recipes[recipe_id]
	if recipe.race not in ("PC_ALL", race):
		raise OracleError(f"--craft-recipe {recipe_id} is {recipe.race}'s: a {race} character never learns it")
	skill_id = recipe.skill_id
	if craft_ctx.skill_kind(skill_id) != "crafting":
		raise OracleError(f"--craft-recipe {recipe_id}: skill {skill_id} is no crafting skill (the morph skill needs no master and no tool)")
	recipe_rep = recipe_report(craft_ctx, recipe_id)
	upgrade = skill_report(craft_ctx, skill_id, 0)["upgrade"]
	if upgrade is None or upgrade["cost"] is None:
		raise OracleError(f"skill {skill_id}: no master price for level 0")
	rows = evaluate(load_groups(ctx.data, map_id), load_npc_templates(ctx.data), GameClock())
	by_npc: dict[int, list[dict]] = {}
	for row in rows:
		by_npc.setdefault(row["npcId"], []).append(row)
	masters = [m for m in upgrade["masters"] if any(_fixed(r) for r in by_npc.get(m, []))]
	if not masters:
		raise OracleError(f"no master of {upgrade['profession']} ({upgrade['masters']}) stands on map {map_id}")
	master_id = masters[0]
	master = _talk(ctx, master_id, by_npc[master_id], None, far, direction)
	reference = (master["chosenSpot"]["x"], master["chosenSpot"]["y"], master["chosenSpot"]["z"])
	names = skill_templates(ctx.data, {skill_id})[skill_id]
	name_code = names["nameId"] << 1 | 1
	cost = upgrade["cost"]
	learned_on_learn = [r.id for r in autolearn_recipes(recipes, race, skill_id, 1)]
	for arm in master["functions"]:
		if arm["action"] == rules.combine_action:
			arm.update({"name": "COMBINE_SKILL_LEVELUP", "answer": "SM_QUESTION_WINDOW (see `learn`)", "question": rules.question_id})
			arm.pop("notModelled", None)
	learn = {
		"dialogAction": rules.combine_action, "supported": rules.combine_action in master["funcDialogs"], "minCharacterLevel": rules.learn_min_level,
		"skillId": skill_id, "skillName": names["name"], "profession": upgrade["profession"], "cost": cost,
		# SM_QUESTION_WINDOW(STR_CRAFT_ADDSKILL_CONFIRM, 0, 0, professionName, price): the name is ChatUtil.l10n(nameId), "$" and two chars
		"question": {"id": rules.question_id, "params": [{"l10nId": names["nameId"], "utf16": [36, name_code & 0xFFFF, (name_code >> 16) & 0xFFFF]},
		                                                 str(cost), ""], "senderId": 0, "range": 0},
		"yes": {"kinahDelta": -cost, "skill": {"skillId": skill_id, "level": 1}, "learnedRecipes": learned_on_learn,
		        "animations": _learn_animations(rules, craft_ctx, skill_id)},
		"notEnoughKinah": "STR_NOT_ENOUGH_MONEY",
	}
	# the vendors of the components: m5c-trade's kinah for the quantity, the vendor nearest the master
	trade_rules = JavaTradeRules.read(java_src)
	trade = TradeData(ctx.data, trade_rules)
	alternative = recipe.components[0] if recipe.components and recipe.components[0] else []
	components = []
	seed_items = []
	bought = 0
	for item_id, quantity in alternative:
		sold = trade_report(ctx.data, java_src, ctx.config, item_id=item_id, count=quantity, races=(race,), trade=trade,
		                    handlers_dir=handlers_dir)["soldBy"]
		vendors = []
		for seller in sold:
			if not seller.get("npcTemplate") or not seller["buyable"] or not seller["shown"]:
				continue
			spots = [r for r in by_npc.get(seller["npcId"], []) if _fixed(r)]
			if not spots:
				continue
			talk = _talk(ctx, seller["npcId"], by_npc[seller["npcId"]], reference, far, direction)
			vendors.append({"npcId": seller["npcId"], "name": seller["name"], "kinah": seller["kinah"][race],
			                "distanceFromMaster": talk["chosenSpot"]["distanceFromReference"], "talk": talk})
		vendors.sort(key=lambda v: v["distanceFromMaster"])
		entry = {"itemId": item_id, "quantity": quantity, "vendors": vendors, "vendor": vendors[0]["npcId"] if vendors else None}
		if vendors:
			bought += vendors[0]["kinah"]
		else:
			seed_items.append({"itemId": item_id, "count": quantity})
		components.append(entry)
	# which other of these npcs could also be talked to from each spot (economy_report's otherNpcsInTalkRange, over the master and the vendors)
	blocks = [master] + [v["talk"] for c in components for v in c["vendors"]]
	for block in blocks:
		for key in ("bandSpot", "nearSpot", "farSpot"):
			spot = (block[key]["x"], block[key]["y"], block[key]["z"])
			block[key]["otherNpcsInTalkRange"] = sorted({other["npcId"] for other in blocks if other["npcId"] != block["npcId"] and in_range(
				other["chosenSpot"]["x"], other["chosenSpot"]["y"], other["chosenSpot"]["z"], *spot, other["limit"])})
	# the tool: every spot of the map's STATIC groups of the template, the nearest to the master chosen
	groups = [g for g in load_groups(ctx.data, map_id) if g.npc_id == tool_id and g.handler == "STATIC"]
	if not groups:
		raise OracleError(f"--craft-tool {tool_id}: no handler=\"STATIC\" spawn of it on map {map_id}")
	for group in groups:
		if group.pool > 0 or group.difficult_id != 0 or group.temporary is not None or any(spot.temporary is not None for spot in group.spots):
			raise OracleError(f"--craft-tool {tool_id}: a static group with a pool, a difficult id or a temporary spawn (not modelled)")
	tools = [{"staticId": spot.static_id, "x": f32(spot.x), "y": f32(spot.y), "z": f32(spot.z), "h": spot.h,
	          "distanceFromMaster": round(distance(*reference, f32(spot.x), f32(spot.y), f32(spot.z)), 3)} for g in groups for spot in g.spots]
	tools.sort(key=lambda t: t["distanceFromMaster"])
	station = recipe_rep["craft"]["station"]
	check_range, packet_range = station["effectiveRange"], f32(station["packetRange"])
	chosen = tools[0]
	where = (chosen["x"], chosen["y"], chosen["z"])
	spots = []
	for d in distances:
		spot = _spot_at(where, f32(d), direction)
		spots.append({"distance": d, "x": spot[0], "y": spot[1], "z": spot[2],
		              # CM_CRAFT: isInRange(player, staticObject, 10), centre to centre; checkCraft: isInRange(player, target, 5, false)
		              "inPacketRange": in_range(*spot, *where, packet_range), "inCheckCraftRange": in_range(*spot, *where, check_range),
		              "outcome": "CraftingTask" if in_range(*spot, *where, check_range) else
		              "STR_COMBINE_TOO_FAR_FROM_TOOL and the cancel pair" if in_range(*spot, *where, packet_range) else "nothing (CM_CRAFT returns)",
		              "otherToolsInCheckCraftRange": [t["staticId"] for t in tools[1:] if in_range(*spot, t["x"], t["y"], t["z"], check_range)]})
	items = craft_ctx.item(tool_id)
	craft = recipe_rep["craft"]
	bar = craft["bars"][0]
	outcome = craft["outcomes"][0]
	skill_up = recipe_rep["skillUp"]
	packets = craft["packets"]
	started = [_craft_update(p) for p in packets["start"] if p.get("packet") == "SM_CRAFT_UPDATE"]
	ended = [_craft_update(p) for p in packets["success"] if p.get("packet") == "SM_CRAFT_UPDATE"]
	refused = [_craft_update(p) for p in packets["refused"] if p.get("packet") == "SM_CRAFT_UPDATE"]
	if (len(started), len(ended), len(refused)) != (2, 1, 1):
		raise OracleError(f"m5c-craft's packets for recipe {recipe_id}: {len(started)} start, {len(ended)} success and {len(refused)} refusal "
		                  "SM_CRAFT_UPDATEs, not the 2, 1 and 1 this block names")
	return {
		"map": map_id,
		"recipe": {"id": recipe_id, "skillId": skill_id, "skillpoint": recipe.skillpoint, "components": [list(c) for c in alternative],
		           "product": recipe_rep["recipe"]["product"], "comboProducts": recipe_rep["recipe"]["comboProducts"],
		           "timing": craft["timing"], "steps": bar["steps"], "finishMillis": outcome["finishMillis"],
		           # every analyze tick's SM_CRAFT_UPDATE carries the product bar's speed and delay (CraftingTask.sendInteractionUpdate); the
		           # task's other updates carry what m5c-craft's packets say (the start pair, the end, sendCancelCraft's)
		           "executionSpeed": bar["executionSpeed"], "showBarDelay": bar["showBarDelay"],
		           "updates": {"init": started[0], "start": started[1], "success": ended[0], "cancel": refused[0]},
		           "xpReward": skill_up["xpReward"], "skillLevelAfter": skill_up["levelAfter"], "playerExp": skill_up["playerExp"]["reward"],
		           # onLearnSkill's animation on the craft's level-up, where m5c-craft's skillUp.packets name one
		           "skillUpAnimations": [_craft_level_up(rules) for p in skill_up["packets"] if p.get("packet") == "SM_ACTION_ANIMATION"],
		           "cmCraftMaterials": {str(item_id): quantity for item_id, quantity in alternative}},
		"master": {"npcId": master_id, "masters": upgrade["masters"], "talk": master},
		"learn": learn,
		"components": components,
		"seedItems": seed_items,
		"exactKinah": cost + bought,
		"kinahParts": {"learn": cost, "components": bought},
		"seedSpot": {"worldId": map_id, "x": master["nearSpot"]["x"], "y": master["nearSpot"]["y"], "z": master["nearSpot"]["z"]},
		"tool": {"templateId": tool_id, "name": items.get("name"), "tools": tools, "chosen": chosen, "stationBoundRadius": station["stationBoundRadius"],
		         "playerBoundRadius": station["playerBoundRadius"], "checkCraftRange": check_range, "packetRange": packet_range,
		         "direction": direction, "spots": spots,
		         "seenAs": "SM_GATHERABLE_INFO: x, y, z, object id, static id, template id (PlayerController.see)"},
	}
