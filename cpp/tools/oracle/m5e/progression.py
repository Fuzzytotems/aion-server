"""m5e-progression (m5e-plan.md G-01): what a character learns and sees from level 1 to the Daeva, step by step.

Java rules, each modelled statement for statement:
- the experience table: PlayerExperienceTable.getStartExpForLevel(L) = experience[L - 1] (0 for level 0) and getLevelForExp; the non-Daeva cap
  of PlayerCommonData.setExp (PlayerCommonData.java:273-288): `maxLevel = isDaeva || offline && (updateDaeva() || exp > startExp(10)) ? 66 : 10`,
  exp clamped to startExp(maxLevel), level = min(levelForExp, maxLevel - 1) - a starting class stops at 9 with a full bar;
- PlayerController.onLevelChange(old, new) (PlayerController.java:568-596): nothing for old == new, otherwise
  SkillLearnService.learnNewSkills(minNewLevel, new) with minNewLevel = old + 1 (old - 1 for a level loss, an empty range);
- SkillLearnService.learnNewSkills (SkillLearnService.java:60-75): the levels from `to` down to `from`, a non-starting class first learns its
  starting class's rows below level 10, autoLearnSkills skips non-autolearn rows and 30001 for a non-starting class (:84-93); at to >= 10 a
  Daeva that knows 30001 gets 30002 at 30001's level (addSkill) and loses 30001 (removeSkill: SM_SKILL_REMOVE);
- PlayerSkillList.addSkill (PlayerSkillList.java:57-77): a known skill at the same or a higher level is no add (no packet); a lower one is
  raised with isNew false; a new one is stored FIRST and is then not new if any template of SkillTreeData.getSkillsForSkill(skill, the
  player's class, race, level) names as its skillLearn a skill the list holds - the walk of SkillTreeData.getSkillsForSkill /
  getHighestSkill / createSkillTree / getTemplatesForSkill (SkillTreeData.java:94-162) ported here exactly, the race and class filter of the
  recursion being the TOP template's own (a class-less PC_ALL top only finds class-less PC_ALL pre-skills);
- SkillLearnService.onLearnSkill / sendPacket (SkillLearnService.java:22-55): the packet only for a spawned player, its message id from
  sendPacket's table (read from the Java), a passive applied at once (reported, not modelled);
- PlayerSkillEntry's skill type (PlayerSkillEntry.java:21-34): a stigma row of the player's class gives 1 or 3, else the template's stigma
  attribute 1 or 0;
- ClassChangeService.setClass (ClassChangeService.java:51-88): the validate guard ("You already switched class", "Invalid class chosen"),
  then setPlayerClass and learnNewSkills(9, level); with updateDaevaStatus the quest 1006 / 2008 COMPLETE and updateDaeva; the page and
  action tables of getClassSelectionDialogPageId / getSelectedPlayerClass read from the Java switch statements;
- the class masters of _1205ANewSkill / _2132ANewSkill (data/handlers/quest/{poeta,ishalgen}): which npc pays which starting class, with
  which page, and the quest var and reward group onLevelChangedEvent sets;
- SkillLearnAction (SkillLearnAction.java:31-69): canAct's level, class, race and "known" refusals and learnSkillBook, which adds the book's
  skill once per template of its getSkillsForSkill walk;
- per skill: the start, use and end conditions the gate's casts meet (WeaponCondition on the main hand's item group, DpCondition,
  ChainCondition's category / precategory, RideRobotCondition, TargetCondition), the costs, the effects and the skills they launch, and the
  charge thresholds of skill_charge.xml (CreatureController.useChargeSkill, CreatureController.java:466-492, at cast speed factor 1);
- per weapon: ItemGroup's required skills (Equipment.checkAvailableEquipSkills) and ItemTemplate's robot id.
Not modelled, and reported as such: stats beyond the class template's base max HP / MP (m5a's stats_info_base_max_hp), cast speed factors
other than 1, passives' stat functions after the class change.
"""

from __future__ import annotations

import json
import re
from dataclasses import dataclass, field
from pathlib import Path

from staticdata_oracle import OracleError

from m5a.creation import JavaEnums, stats_info_base_max_hp, stats_info_base_max_mp
from m5a.data import StaticData, java_boolean, java_int

RACES = ("ELYOS", "ASMODIANS")
# the quest the simple class change completes (ClassChangeService.completeAscensionQuest, ClassChangeService.java:36-49) and updateDaeva reads
ASCENSION_QUESTS = {"ELYOS": 1006, "ASMODIANS": 2008}
TAPPING_HUMAN = 30001
TAPPING_DAEVA = 30002


def _read(path: Path) -> str:
	try:
		return path.read_text(encoding="utf-8")
	except OSError as e:
		raise OracleError(f"{path}: {e}") from e


def _strip_comments(text: str) -> str:
	text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
	return re.sub(r"//[^\n]*", "", text)


def _method(text: str, head: str, what: str) -> str:
	"""the body of the first method whose declaration matches `head` (a regex), braces balanced"""
	match = re.search(head, text)
	if not match:
		raise OracleError(f"{what}: method {head!r} not found")
	start = text.index("{", match.end() - 1 if text[match.end() - 1] == "{" else match.end())
	depth = 0
	for i in range(start, len(text)):
		if text[i] == "{":
			depth += 1
		elif text[i] == "}":
			depth -= 1
			if depth == 0:
				return text[start + 1:i]
	raise OracleError(f"{what}: unbalanced braces in {head!r}")


# ---- the Java rules this oracle reads -------------------------------------------------------------------------------------------------------

@dataclass
class ClassMaster:
	npc_id: int
	starting_class: str
	page: int


@dataclass
class ProgressionRules:
	"""What m5e-progression reads from the Java sources and handlers (each method cited at its use)."""

	dialog_actions: dict[str, int]
	# ClassChangeService.getClassSelectionDialogPageId: starting class -> (ELYOS page, ASMODIANS page)
	selection_pages: dict[str, tuple[int, int]]
	# ClassChangeService.getSelectedPlayerClass: race -> dialog action id -> class
	selected_classes: dict[str, dict[int, str]]
	# SkillLearnService.sendPacket's message ids
	messages: dict[str, int]
	# race -> the "A New Skill" quest, its class masters and its per-class (quest var, reward group)
	trainer_quests: dict[str, int]
	trainers: dict[str, list[ClassMaster]]
	trainer_vars: dict[str, dict[str, tuple[int, int]]]
	# ItemGroup name -> required skill ids (the int[] constructor argument; ItemTemplate.getRequiredSkills)
	required_skills: dict[str, list[int]]

	@staticmethod
	def read(java_src: Path, handlers: Path) -> "ProgressionRules":
		base = java_src / "com" / "aionemu" / "gameserver"
		actions = {}
		for name, value in re.findall(r"public static final int (\w+)\s*=\s*(-?\d+)\s*;", _read(base / "model" / "DialogAction.java")):
			actions[name] = int(value)
		ccs = _strip_comments(_read(base / "services" / "ClassChangeService.java"))
		pages_body = _method(ccs, r"static int getClassSelectionDialogPageId\(", "ClassChangeService")
		pages = {}
		for cls, elyos, asmo in re.findall(r"case (\w+):\s*return playerRace == Race\.ELYOS \? (\d+) : (\d+);", pages_body):
			pages[cls] = (int(elyos), int(asmo))
		if len(pages) != 6:
			raise OracleError(f"ClassChangeService.getClassSelectionDialogPageId: {len(pages)} classes read, expected the six starting classes")
		selected_body = _method(ccs, r"static PlayerClass getSelectedPlayerClass\(", "ClassChangeService")
		selected: dict[str, dict[int, str]] = {}
		for race, arm in re.findall(r"case (ELYOS|ASMODIANS):\s*switch \(dialogActionId\) \{(.*?)\}", selected_body, re.DOTALL):
			table = {}
			for action, cls in re.findall(r"case (\w+):\s*return PlayerClass\.(\w+);", arm):
				if action not in actions:
					raise OracleError(f"ClassChangeService.getSelectedPlayerClass: DialogAction.{action} is not an int constant")
				table[actions[action]] = cls
			selected[race] = table
		if set(selected) != set(RACES) or any(len(t) != 11 for t in selected.values()):
			raise OracleError("ClassChangeService.getSelectedPlayerClass: expected 11 classes for each race")

		learn = _strip_comments(_read(base / "services" / "SkillLearnService.java"))
		send = _method(learn, r"private static void sendPacket\(", "SkillLearnService")
		tapping = re.search(r"isTappingSkill\(\)\)\s*PacketSendUtility\.sendPacket\(player, new SM_SKILL_LIST\(skill, isNew \? (\d+) : (\d+)\)\)", send)
		profession = re.search(r"else\s*PacketSendUtility\.sendPacket\(player, new SM_SKILL_LIST\(skill, isNew \? (\d+) : (\d+)\)\)", send)
		normal = re.search(r"isStigmaSkill\(\) \? skill\.isLinkedStigmaSkill\(\) \? (\d+) : (\d+) : (\d+)\)", send)
		if not (tapping and profession and normal):
			raise OracleError("SkillLearnService.sendPacket: the message ids could not be read")
		messages = {"tappingNew": int(tapping.group(1)), "tappingKnown": int(tapping.group(2)), "professionNew": int(profession.group(1)),
		            "professionKnown": int(profession.group(2)), "linkedStigmaNew": int(normal.group(1)), "stigmaNew": int(normal.group(2)),
		            "new": int(normal.group(3)), "known": 0}

		trainer_quests, trainers, trainer_vars = {}, {}, {}
		for race, rel in (("ELYOS", "poeta/_1205ANewSkill.java"), ("ASMODIANS", "ishalgen/_2132ANewSkill.java")):
			text = _strip_comments(_read(handlers / rel))
			quest = re.search(r"super\((\d+)\)", text)
			if not quest:
				raise OracleError(f"{rel}: no quest id")
			trainer_quests[race] = int(quest.group(1))
			masters = []
			dialog = _method(text, r"public boolean onDialogEvent\(", rel)
			for npc, cls, page in re.findall(r"case (\d+):\s*if \(playerClass == PlayerClass\.(\w+)\) \{\s*if \(env\.getDialogActionId\(\) == USE_OBJECT\)"
			                                 r"\s*return sendQuestDialog\(env, (\d+)\);", dialog):
				masters.append(ClassMaster(int(npc), cls, int(page)))
			if len(masters) != 6:
				raise OracleError(f"{rel}: {len(masters)} class masters read, expected 6")
			trainers[race] = masters
			level = _method(text, r"public void onLevelChangedEvent\(", rel)
			vars_ = {}
			for cls, var, group in re.findall(r"case (\w+):\s*qs\.setQuestVar\((\d+)\);\s*qs\.setRewardGroup\((\d+)\);", level):
				vars_[cls] = (int(var), int(group))
			if len(vars_) != 6:
				raise OracleError(f"{rel}: onLevelChangedEvent sets {len(vars_)} classes, expected 6")
			trainer_vars[race] = vars_

		required: dict[str, list[int]] = {}
		groups = _strip_comments(_read(base / "model" / "templates" / "item" / "enums" / "ItemGroup.java"))
		for name, ints in re.findall(r"^\s*(\w+)\([^;\n]*?new int\[\] \{([\d,\s]+)\}\)", groups, re.MULTILINE):
			required[name] = [int(v) for v in ints.split(",")]
		return ProgressionRules(actions, pages, selected, messages, trainer_quests, trainers, trainer_vars, required)

	def action_name(self, action_id: int) -> str | None:
		names = [name for name, value in self.dialog_actions.items() if value == action_id]
		return names[0] if names else None


# ---- the static data ----------------------------------------------------------------------------------------------------------------------

@dataclass(frozen=True)
class LearnRow:
	"""one <skill> of skill_tree.xml (SkillLearnTemplate)"""
	class_id: str | None
	skill_id: int
	skill_learn: int | None
	race: str
	min_level: int
	autolearn: bool
	stigma: int


@dataclass(frozen=True)
class SkillInfo:
	skill_id: int
	name: str
	lvl: int
	stack: str | None
	activation: str
	stigma: str


class ProgressionData:
	"""skill_tree.xml in document order, every skill template's level, stack, activation and stigma type, and the experience table"""

	def __init__(self, data: StaticData):
		self.data = data
		self.rows: list[LearnRow] = []
		for element in data.children("skill_tree", "skill"):
			learn = element.get("skillLearn")
			self.rows.append(LearnRow(element.get("classId"), java_int(element.get("skillId"), "skillId"),
			                          java_int(learn, "skillLearn") if learn is not None else None, element.get("race", "PC_ALL"),
			                          java_int(element.get("minLevel"), "minLevel"), java_boolean(element.get("autolearn")),
			                          java_int(element.get("stigma"), "stigma", 0)))
		self.skills: dict[int, SkillInfo] = {}
		# SkillData.afterUnmarshal: skillTemplateById keeps the LAST template of an id, skillTemplatesByStack lists every template in document order
		self.by_stack: dict[str, list[SkillInfo]] = {}
		for element in data.stream("skill_data", "skill_template"):
			info = SkillInfo(java_int(element.get("skill_id"), "skill_id"), element.get("name", ""), java_int(element.get("lvl"), "lvl", 0),
			                 element.get("stack"), element.get("activation", ""), element.get("stigma", "NONE"))
			self.skills[info.skill_id] = info
			if info.stack is not None:
				self.by_stack.setdefault(info.stack, []).append(info)
		# PlayerExperienceTable's `long[] experience`
		self.experience = []
		for e in data.children("player_experience_table", "exp"):
			text = (e.text or "").strip()
			if not re.fullmatch(r"-?\d+", text):
				raise OracleError(f"player_experience_table <exp>{text}</exp> is not a long")
			self.experience.append(int(text))
		if not self.experience:
			raise OracleError("player_experience_table.xml has no <exp>")

	# PlayerExperienceTable (PlayerExperienceTable.java:29-56)
	def start_exp(self, level: int) -> int:
		if level > len(self.experience):
			raise OracleError(f"level {level} is above the table's max level {len(self.experience)} (IllegalArgumentException)")
		return 0 if level == 0 else self.experience[level - 1]

	def level_for_exp(self, exp: int) -> int:
		level = 0
		for i in range(len(self.experience), 0, -1):
			if exp >= self.experience[i - 1]:
				level = i
				break
		return len(self.experience) - 1 if len(self.experience) <= level else level

	def max_level(self) -> int:
		return len(self.experience)

	# SkillTreeData (SkillTreeData.java:71-162)
	def templates_for(self, cls: str, level: int, race: str) -> list[LearnRow]:
		"""getTemplatesFor: the race-specific list, then the PC_ALL one, each in document order (a class-less row is in every class's list)"""
		specific = [r for r in self.rows if (r.class_id is None or r.class_id == cls) and r.race == race and r.min_level == level]
		generic = [r for r in self.rows if (r.class_id is None or r.class_id == cls) and r.race == "PC_ALL" and r.min_level == level]
		return specific + generic

	def templates_for_skill(self, skill_id: int, cls: str | None, race: str) -> list[LearnRow]:
		"""getTemplatesForSkill: class-less or the class, PC_ALL or the race (a null class or race passed in matches only null / PC_ALL rows)"""
		return [r for r in self.rows if r.skill_id == skill_id and (r.class_id is None or r.class_id == cls)
		        and (r.race == "PC_ALL" or r.race == race)]

	def highest_skill(self, skill_id: int) -> int:
		"""getHighestSkill: the template of the stack with the highest lvl, the first of equal ones (Stream.max keeps the first)"""
		base = self.skills.get(skill_id)
		if base is None or base.stack is None:
			return skill_id
		templates = self.by_stack.get(base.stack)
		if not templates:
			return skill_id
		best = templates[0]
		for t in templates[1:]:
			if t.lvl > best.lvl:
				best = t
		return best.skill_id

	def skills_for_skill(self, skill_id: int, cls: str, race: str, player_level: int) -> list[LearnRow]:
		tree: list[LearnRow] = []
		tops = self.templates_for_skill(self.highest_skill(skill_id), cls, race)
		if tops:
			self._create_skill_tree(tops[0], tree)
		if player_level > -1:
			tree = [t for t in tree if t.min_level <= player_level]
		return tree

	def _create_skill_tree(self, top: LearnRow | None, tree: list[LearnRow]) -> None:
		if top is None:
			return
		tree.insert(0, top)
		if top.skill_learn is None:
			return
		for template in self.templates_for_skill(top.skill_learn, top.class_id, top.race):
			if (top.stigma > 0) != (template.stigma > 0):
				continue
			self._create_skill_tree(template, tree)
			break

	def skill_level(self, skill_id: int) -> int:
		"""SkillLearnTemplate.getSkillLevel: the skill template's lvl (NullPointerException without a template)"""
		info = self.skills.get(skill_id)
		if info is None:
			raise OracleError(f"skill {skill_id} has no skill template (SkillLearnTemplate.getSkillLevel throws NullPointerException)")
		return info.lvl


# ---- the character -------------------------------------------------------------------------------------------------------------------

@dataclass
class Character:
	race: str
	player_class: str
	level: int
	daeva: bool = False
	spawned: bool = False
	skills: dict[int, int] = field(default_factory=dict)
	skill_types: dict[int, int] = field(default_factory=dict)


class Progression:
	def __init__(self, data: ProgressionData, rules: ProgressionRules, enums: JavaEnums):
		self.data = data
		self.rules = rules
		self.enums = enums

	def is_starting(self, cls: str) -> bool:
		return self.enums.classes[cls][0]

	def message_id(self, skill_id: int, skill_type: int, is_new: bool) -> int:
		"""SkillLearnService.sendPacket (SkillLearnService.java:44-55) with PlayerSkillEntry's isProfessionSkill / isTappingSkill"""
		m = self.rules.messages
		if 30000 <= skill_id < 50000:
			if 30001 <= skill_id <= 30003:
				return m["tappingNew"] if is_new else m["tappingKnown"]
			return m["professionNew"] if is_new else m["professionKnown"]
		if is_new:
			if skill_type > 0:
				return m["linkedStigmaNew"] if skill_type >= 3 else m["stigmaNew"]
			return m["new"]
		return m["known"]

	def skill_type(self, ch: Character, skill_id: int) -> int:
		"""PlayerSkillEntry(Player, ...) (PlayerSkillEntry.java:21-34)"""
		templates = self.data.templates_for_skill(skill_id, ch.player_class, ch.race)
		if not templates:
			info = self.data.skills.get(skill_id)
			return 0 if info is None or info.stigma == "NONE" else 1
		for template in templates:
			if template.stigma > 0:
				return 3 if template.stigma == 4 else 1
		return 0

	def add_skill(self, ch: Character, skill_id: int, level: int, events: list[dict]) -> bool:
		"""PlayerSkillList.addSkill (PlayerSkillList.java:57-77) and SkillLearnService.onLearnSkill"""
		existing = ch.skills.get(skill_id)
		is_new = True
		if existing is not None:
			if level <= existing:
				return False
			ch.skills[skill_id] = level
			is_new = False
		else:
			ch.skills[skill_id] = level
			ch.skill_types[skill_id] = self.skill_type(ch, skill_id)
			for template in self.data.skills_for_skill(skill_id, ch.player_class, ch.race, ch.level):
				if template.skill_learn is not None and template.skill_learn in ch.skills:
					is_new = False
					break
		info = self.data.skills.get(skill_id)
		events.append({"op": "add", "skillId": skill_id, "level": level, "isNew": is_new,
		               "messageId": self.message_id(skill_id, ch.skill_types.get(skill_id, 0), is_new) if ch.spawned else None,
		               "passive": info is not None and info.activation == "PASSIVE",
		               "name": info.name if info is not None else None})
		return True

	def remove_skill(self, ch: Character, skill_id: int, events: list[dict]) -> bool:
		"""SkillLearnService.removeSkill (SkillLearnService.java:101-112): SM_SKILL_REMOVE to the player"""
		if skill_id not in ch.skills:
			return False
		level = ch.skills.pop(skill_id)
		ch.skill_types.pop(skill_id, None)
		events.append({"op": "remove", "skillId": skill_id, "level": level})
		return True

	def auto_learn(self, ch: Character, level: int, cls: str, events: list[dict]) -> None:
		"""SkillLearnService.autoLearnSkills (SkillLearnService.java:84-93)"""
		for template in self.data.templates_for(cls, level, ch.race):
			if not template.autolearn:
				continue
			if template.skill_id == TAPPING_HUMAN and not self.is_starting(cls):
				continue
			self.add_skill(ch, template.skill_id, self.data.skill_level(template.skill_id), events)

	def learn_new_skills(self, ch: Character, from_level: int, to_level: int, events: list[dict]) -> None:
		"""SkillLearnService.learnNewSkills (SkillLearnService.java:60-75)"""
		start_class = None if self.is_starting(ch.player_class) else self.enums.starting_classes[ch.player_class]
		for level in range(to_level, from_level - 1, -1):
			if level < 10 and start_class is not None:
				self.auto_learn(ch, level, start_class, events)
			self.auto_learn(ch, level, ch.player_class, events)
		if to_level >= 10 and ch.daeva and TAPPING_HUMAN in ch.skills:
			if TAPPING_DAEVA not in ch.skills:
				self.add_skill(ch, TAPPING_DAEVA, ch.skills[TAPPING_HUMAN], events)
			self.remove_skill(ch, TAPPING_HUMAN, events)

	def level_change(self, ch: Character, old: int, new: int, events: list[dict]) -> None:
		"""PlayerController.onLevelChange (PlayerController.java:568-596), the skill half: learnNewSkills(minNewLevel, new)"""
		if old == new:
			return
		ch.level = new
		self.learn_new_skills(ch, old + 1 if old < new else old - 1, new, events)

	def set_class(self, ch: Character, new_class: str | None, validate: bool, update_daeva: bool, events: list[dict]) -> dict:
		"""ClassChangeService.setClass (ClassChangeService.java:55-88); the messages are sendMessage's texts"""
		if new_class is None:
			return {"accepted": False, "message": None}
		if validate:
			old = ch.player_class
			if not self.is_starting(old):
				return {"accepted": False, "message": "You already switched class"}
			old_id = self.class_id(old)
			if old == new_class or self.class_id(new_class) <= old_id or self.class_id(new_class) > old_id + 2:
				return {"accepted": False, "message": "Invalid class chosen"}
		ch.player_class = new_class
		self.learn_new_skills(ch, 9, ch.level, events)
		ascension = None
		if update_daeva:
			if not self.is_starting(new_class):
				ascension = ASCENSION_QUESTS[ch.race]
				ch.daeva = True  # completeAscensionQuest, then updateDaeva (PlayerCommonData.java:588-610) finds it COMPLETE
			else:
				ch.daeva = False
		return {"accepted": True, "message": None, "ascensionQuestCompleted": ascension}

	def class_id(self, cls: str) -> int:
		return self.enums.class_ids[cls]


# ---- steps -----------------------------------------------------------------------------------------------------------------------------

STEP_HELP = """steps, applied in order to one character:
  create            PlayerService.newPlayer: learnNewSkills(1, 1) before the character has an effect controller (no packet)
  enter:L           an enter world at level L after the last level stored at leave world (players.old_level): onLevelChange(old, L)
                    runs before the spawn, so it sends no SM_SKILL_LIST; the character is spawned afterwards
  level:L           an online level change to L (a kill, a quest reward): onLevelChange(level, L) with the packets
  class:CLASS       CM_DIALOG_SELECT(0, the action of CLASS, ..., 1006 / 2008) with the simple class change: changeClassToSelection
  action:ID         the same with a raw dialog action id (getSelectedPlayerClass; an id it does not know selects no class)
  quit              leave world: players.old_level = level, the character is no longer spawned
  seed:CLASS        while logged out, players.player_class = CLASS and the ascension quest COMPLETE (a Daeva for an advanced class)
  book:ITEM         CM_USE_ITEM of a skill book: SkillLearnAction.canAct, then learnSkillBook"""


def _load_known(path: Path) -> dict[int, int]:
	"""--known-skills: a JSON list of {"skillId", "level"} or of [id, level], or text of `id[:level]` tokens (level 1 when omitted)"""
	text = _read(path).strip()
	known: dict[int, int] = {}
	if text.startswith("["):
		for entry in json.loads(text):
			if isinstance(entry, dict):
				known[int(entry["skillId"])] = int(entry.get("level", 1))
			else:
				known[int(entry[0])] = int(entry[1])
		return known
	for token in text.replace(",", " ").split():
		skill, _, level = token.partition(":")
		known[int(skill)] = int(level) if level else 1
	return known


def _book_action(data: StaticData, item_id: int) -> dict:
	for element in data.stream("item_templates", "item_template"):
		if java_int(element.get("id"), "item id") != item_id:
			continue
		learn = element.find("actions/skilllearn")
		if learn is None:
			raise OracleError(f"item {item_id} has no <skilllearn> action")
		return {"itemId": item_id, "name": element.get("name"), "race": element.get("race", "PC_ALL"),
		        "skillId": java_int(learn.get("skillid"), "skillid"), "level": java_int(learn.get("level"), "level", 0),
		        "class": learn.get("class")}
	raise OracleError(f"item {item_id} has no template")


class StepRunner:
	def __init__(self, progression: Progression, ch: Character, data: StaticData):
		self.p = progression
		self.ch = ch
		self.data = data
		self.old_level = ch.level

	def stats(self) -> dict:
		_s, health, will, hm, wm = self.p.enums.classes[self.ch.player_class]
		return {"maxHp": stats_info_base_max_hp(health, hm, self.ch.level), "maxMp": stats_info_base_max_mp(will, wm, self.ch.level)}

	def run(self, step: str) -> dict:
		kind, _, arg = step.partition(":")
		ch = self.ch
		events: list[dict] = []
		report: dict = {"step": step}
		if kind == "create":
			ch.spawned = False
			self.p.learn_new_skills(ch, 1, 1, events)
			self.old_level = ch.level
		elif kind == "enter":
			level = int(arg)
			ch.spawned = False
			report["levelChange"] = [self.old_level, level]
			self.p.level_change(ch, self.old_level, level, events)
			ch.level = level
			ch.spawned = True
		elif kind == "level":
			level = int(arg)
			if not ch.spawned:
				raise OracleError(f"{step}: the character is not in the world")
			report["levelChange"] = [ch.level, level]
			self.p.level_change(ch, ch.level, level, events)
		elif kind in ("class", "action"):
			if not ch.spawned:
				raise OracleError(f"{step}: the character is not in the world")
			table = self.p.rules.selected_classes[ch.race]
			if kind == "class":
				actions = [a for a, c in table.items() if c == arg]
				if not actions:
					raise OracleError(f"{step}: {ch.race} has no dialog action that selects {arg}")
				action = actions[0]
			else:
				action = int(arg)
			new_class = table.get(action)
			report["dialogActionId"] = action
			report["dialogAction"] = self.p.rules.action_name(action)
			report["selectedClass"] = new_class
			report["classIdBefore"] = self.p.class_id(ch.player_class)
			report.update(self.p.set_class(ch, new_class, True, True, events))
		elif kind == "quit":
			ch.spawned = False
			self.old_level = ch.level
		elif kind == "seed":
			# the gate's offline seed of a Daeva (m5e-plan.md D8, m5c-plan.md D5's recipe): players.player_class and a player_quests row of
			# 1006 / 2008 COMPLETE, written while the character is logged out - nothing is learned; the next enter world's
			# PlayerCommonData.setExp finds the quest through updateDaeva (PlayerCommonData.java:276, 588-610)
			if ch.spawned:
				raise OracleError(f"{step}: a seed is written while the character is logged out")
			if arg not in self.p.enums.classes:
				raise OracleError(f"{step}: unknown player class {arg}")
			ch.player_class = arg
			ch.daeva = not self.p.is_starting(arg)
		elif kind == "book":
			book = _book_action(self.data, int(arg))
			report["book"] = book
			refusal = None
			if ch.level < book["level"]:
				refusal = "level"
			elif not (book["class"] is None or book["class"] == ch.player_class or book["class"] == self.p.enums.starting_classes[ch.player_class]):
				refusal = "class"
			elif book["race"] != ch.race and book["race"] != "PC_ALL":
				refusal = "race"
			elif book["skillId"] in ch.skills:
				refusal = "known"
			report["canAct"] = refusal is None
			report["refusal"] = refusal
			if refusal is None:
				for template in self.p.data.skills_for_skill(book["skillId"], ch.player_class, ch.race, ch.level):
					self.p.add_skill(ch, book["skillId"], self.p.data.skill_level(template.skill_id), events)
		else:
			raise OracleError(f"unknown step {step!r}\n{STEP_HELP}")
		report.update({"level": ch.level, "playerClass": ch.player_class, "classId": self.p.class_id(ch.player_class), "daeva": ch.daeva,
		               "spawned": ch.spawned, "events": events, "baseStats": self.stats(),
		               "skills": [{"skillId": s, "level": lvl} for s, lvl in sorted(ch.skills.items())]})
		return report


# ---- skills, weapons, the charge table ------------------------------------------------------------------------------------------------

CONDITION_TAGS = ("startconditions", "useconditions", "endconditions")


def _skill_elements(data: StaticData, wanted: set[int]) -> dict[int, dict]:
	found: dict[int, dict] = {}
	for element in data.stream("skill_data", "skill_template"):
		skill_id = java_int(element.get("skill_id"), "skill_id")
		if skill_id not in wanted:
			continue
		conditions = {tag: [(c.tag, dict(c.attrib)) for c in (element.find(tag) or [])] for tag in CONDITION_TAGS}
		effects = []
		container = element.find("effects")
		for child in (container if container is not None else []):
			effects.append({"tag": child.tag, "attrs": dict(child.attrib),
			                "subeffects": [java_int(s.get("skill_id"), "subeffect skill_id") for s in child.findall("subeffect")],
			                "changes": [dict(c.attrib) for c in child.findall("change")]})
		actions = [(a.tag, dict(a.attrib)) for a in (element.find("actions") or [])]
		properties = element.find("properties")
		found[skill_id] = {"attrs": dict(element.attrib), "conditions": conditions, "effects": effects, "actions": actions,
		                   "properties": dict(properties.attrib) if properties is not None else {}}
	missing = sorted(wanted - found.keys())
	if missing:
		raise OracleError(f"no skill_template for {missing}")
	return found


def _charge_entries(data: StaticData) -> dict[int, dict]:
	entries = {}
	for element in data.children("skill_charge", "charge"):
		entries[java_int(element.get("id"), "charge id")] = {
			"minTime": java_int(element.get("min_time"), "min_time"),
			"skills": [{"time": java_int(s.get("time"), "time"), "skillId": java_int(s.get("id"), "id")} for s in element.findall("skill")]}
	return entries


def charged_skill(entry: dict, charge_time: int, factor: float = 1.0) -> int | None:
	"""CreatureController.useChargeSkill (CreatureController.java:466-492): None below min_time (refused), else the released skill id"""
	if charge_time < entry["minTime"] * factor:
		return None
	index, total = 0, 0
	skills = entry["skills"]
	while True:
		total += int(skills[index]["time"] * factor)
		if total >= charge_time:
			break
		index += 1
		if index == len(skills) - 1:
			break
	return skills[index]["skillId"]


# the effect attributes that name another skill (subeffect is read apart): resurrect / rebirth (ResurrectEffect.java:28), aura (AuraEffect.java:71),
# provoker, delayedskill, skilllauncher, condskilllauncher (ProvokerEffect.java:62, DelayedSkillEffect.java:25, SkillLauncherEffect.java:23,
# CondSkillLauncherEffect.java:46)
LAUNCH_ATTRIBUTES = {"resurrect": "skill_id", "rebirth": "skill_id", "aura": "skill_id", "provoker": "skill_id", "delayedskill": "skill_id",
                     "skilllauncher": "skill_id", "condskilllauncher": "skill_id"}


def skill_report(elements: dict[int, dict], skill_id: int, charges: dict[int, dict], character: dict) -> dict:
	"""one skill's constants and its conditions evaluated against `character` (weaponGroup, dp, robot, chainAfter, targetKind)"""
	element = elements[skill_id]
	attrs = element["attrs"]
	conditions = element["conditions"]
	report: dict = {"skillId": skill_id, "name": attrs.get("name"), "level": java_int(attrs.get("lvl"), "lvl", 0),
	                "activation": attrs.get("activation"), "tslot": attrs.get("tslot"), "stack": attrs.get("stack"),
	                "cooldown": java_int(attrs.get("cooldown"), "cooldown", 0), "duration": java_int(attrs.get("duration"), "duration", 0),
	                "chainSkillProb": java_int(attrs.get("chain_skill_prob"), "chain_skill_prob", 0),
	                "properties": element["properties"], "effects": [], "launches": [], "costs": {}, "conditions": [], "refusals": []}
	for effect in element["effects"]:
		row = {"tag": effect["tag"]}
		for key in ("hp_percent", "value", "duration1", "duration2", "skill_id", "npc_id", "distance"):
			if key in effect["attrs"]:
				row[key] = java_int(effect["attrs"][key], key)
		if effect["changes"]:
			row["changes"] = effect["changes"]
		report["effects"].append(row)
		attribute = LAUNCH_ATTRIBUTES.get(effect["tag"])
		if attribute and attribute in effect["attrs"]:
			report["launches"].append({"kind": effect["tag"], "skillId": java_int(effect["attrs"][attribute], attribute)})
		for launched in effect["subeffects"]:
			report["launches"].append({"kind": "subeffect", "skillId": launched, "of": effect["tag"]})
	for tag, values in element["actions"]:
		if tag in ("dpuse", "mpuse", "hpuse", "itemuse"):
			report["costs"][tag] = {k: (java_int(v, k) if re.fullmatch(r"-?\d+", v) else v) for k, v in values.items()}
	for section, entries in conditions.items():
		for tag, values in entries:
			condition = {"section": section, "tag": tag, **values}
			report["conditions"].append(condition)
			if tag == "mp" and section == "endconditions":
				report["costs"]["mp"] = {k: (java_int(v, k) if re.fullmatch(r"-?\d+", v) else v) for k, v in values.items()}
			refusal = _evaluate(tag, values, character, report)
			if refusal:
				report["refusals"].append(refusal)
			if tag == "skillcharge":
				entry = charges.get(java_int(values.get("value"), "skillcharge value"))
				if entry is None:
					raise OracleError(f"skill {skill_id}: skill_charge.xml has no charge {values.get('value')}")
				report["charge"] = entry
	report["accepted"] = not report["refusals"]
	return report


def _evaluate(tag: str, values: dict[str, str], character: dict, report: dict) -> str | None:
	"""the conditions the gate's casts meet; an unknown tag is listed, not evaluated"""
	if tag == "weapon":
		groups = values.get("weapon", "").split()
		weapon = character.get("weaponGroup")
		if weapon not in groups:
			return f"WeaponCondition: the main hand's group {weapon} is not one of {groups} (WeaponCondition.java:44-49)"
	elif tag == "dp":
		need = java_int(values.get("value"), "dp value")
		if character.get("dp", 0) < need:
			return f"DpCondition: DP {character.get('dp', 0)} < {need} (DpCondition.java:24-26)"
	elif tag == "ride_robot":
		if not character.get("robot"):
			return "RideRobotCondition: Player.isInRobotMode() is false (RideRobotCondition.java:18-24)"
	elif tag == "chain":
		pre = values.get("precategory")
		after = character.get("chainAfter")
		if pre is not None and after != pre:
			return f"ChainCondition: the current or previous chain category {after} is not the precategory {pre} (ChainCondition.java:39-48)"
		report["chain"] = {"category": values.get("category"), "precategory": pre, "time": java_int(values.get("time"), "time", 0)}
	elif tag == "target":
		value = values.get("value")
		kind = character.get("targetKind")
		props = report["properties"]
		if value in ("PC", "NPC") and props.get("target_type") != "AREA" and props.get("first_target") in ("TARGET", "TARGETORME") and kind is not None \
				and kind != value:
			return f"TargetCondition: the first target is a {kind}, the condition wants {value} (TargetCondition.java:38-58)"
	return None


def weapon_report(data: StaticData, rules: ProgressionRules, item_id: int, known: dict[int, int]) -> dict:
	for element in data.stream("item_templates", "item_template"):
		if java_int(element.get("id"), "item id") != item_id:
			continue
		group = element.get("item_group", "NONE")
		required = rules.required_skills.get(group, [])
		robot = element.get("robot")
		return {"itemId": item_id, "name": element.get("name"), "itemGroup": group, "level": java_int(element.get("level"), "level", 0),
		        "requiredSkills": required,
		        # Equipment.checkAvailableEquipSkills: true without required skills, else any one of them known
		        "equipSkillKnown": not required or any(s in known for s in required),
		        "robotId": java_int(robot, "robot") if robot is not None else 0}
	raise OracleError(f"item {item_id} has no template")


# ---- a stigma stone ------------------------------------------------------------------------------------------------------------------------

def _stigma_prices(java_src: Path) -> dict[str, int]:
	"""StigmaService.notifyEquipAction's base kinah (StigmaService.java:69-77): 25,000, 50,000 for LEGEND, 100,000 for UNIQUE"""
	text = _strip_comments(_read(java_src / "com" / "aionemu" / "gameserver" / "services" / "StigmaService.java"))
	body = _method(text, r"public static boolean notifyEquipAction\(", "StigmaService")
	base = re.search(r"long kinahcount = (\d+);", body)
	legend = re.search(r"ItemQuality\.LEGEND\)\)\s*kinahcount = (\d+);", body)
	unique = re.search(r"ItemQuality\.UNIQUE\)\)\s*kinahcount = (\d+);", body)
	if not (base and legend and unique):
		raise OracleError("StigmaService.notifyEquipAction: the kinah counts could not be read")
	return {"default": int(base.group(1)), "LEGEND": int(legend.group(1)), "UNIQUE": int(unique.group(1))}


def stigma_report(data: StaticData, java_src: Path, progression: Progression, ch: Character, item_id: int, prices: dict | None) -> dict:
	"""
	One stigma stone equipped by `ch` in an empty regular slot: StigmaService.notifyEquipAction's kinah (PricesService.getPriceForService over
	the base of the stone's quality, m5c's service_price with the profile's prices) and addStigmaSkills (StigmaService.java:423-429): every
	skill template of each gain_skill_group (SkillData.getSkillTemplatesByGroup, document order), each SkillTreeData.getTemplatesForSkill of
	the class and race with minLevel <= level, learned as a temporary skill at the stone's enchant level + 1 (1), with addSkill's isNew and
	sendPacket's message id (1300401 for a new stigma skill).
	"""
	stone = None
	for element in data.stream("item_templates", "item_template"):
		if java_int(element.get("id"), "item id") == item_id:
			stigma = element.find("stigma")
			stone = {"itemId": item_id, "name": element.get("name"), "quality": element.get("quality", "COMMON"),
			         "level": java_int(element.get("level"), "level", 0), "itemGroup": element.get("item_group"),
			         "groups": [g for g in (stigma.get("gain_skill_group1"), stigma.get("gain_skill_group2")) if g] if stigma is not None else []}
			break
	if stone is None:
		raise OracleError(f"item {item_id} has no template")
	if not stone["groups"]:
		raise OracleError(f"item {item_id} is no stigma stone (no <stigma>)")
	by_group: dict[str, list[int]] = {}
	for element in data.stream("skill_data", "skill_template"):
		group = element.get("group")
		if group in stone["groups"]:
			by_group.setdefault(group, []).append(java_int(element.get("skill_id"), "skill_id"))
	bases = _stigma_prices(java_src)
	base = bases.get(stone["quality"], bases["default"])
	events: list[dict] = []
	learner = Character(ch.race, ch.player_class, ch.level, ch.daeva, True, dict(ch.skills), dict(ch.skill_types))
	for group in stone["groups"]:
		for skill_id in by_group.get(group, []):
			for template in progression.data.templates_for_skill(skill_id, ch.player_class, ch.race):
				if ch.level >= template.min_level:
					progression.add_skill(learner, template.skill_id, 1, events)
	from m5c.economy import service_price
	return {**stone, "basePrice": base, "price": service_price(base, prices) if prices is not None else None, "events": events}


# ---- the report --------------------------------------------------------------------------------------------------------------------------

def progression_report(data: StaticData, java_src: Path, handlers: Path, race: str, player_class: str, level: int, steps: list[str],
                       known_file: Path | None, daeva: bool, skills: list[int], weapons: list[int], character: dict,
                       stigmas: list[int] = (), prices: dict | None = None) -> dict:
	if race not in RACES:
		raise OracleError(f"race must be one of {RACES}")
	enums = JavaEnums(java_src)
	if player_class not in enums.classes:
		raise OracleError(f"unknown player class {player_class}")
	enums.class_ids = _class_ids(java_src)
	rules = ProgressionRules.read(java_src, handlers)
	pdata = ProgressionData(data)
	progression = Progression(pdata, rules, enums)
	ch = Character(race, player_class, level, daeva)
	if known_file is not None:
		ch.skills = _load_known(known_file)
		for skill_id in ch.skills:
			ch.skill_types[skill_id] = progression.skill_type(ch, skill_id)
	runner = StepRunner(progression, ch, data)
	step_reports = [runner.run(step) for step in steps]

	starting = enums.starting_classes[player_class]
	pages = rules.selection_pages.get(starting)
	page = None if pages is None else pages[0] if race == "ELYOS" else pages[1]
	choices = []
	for action, cls in sorted(rules.selected_classes[race].items()):
		cid, sid = enums.class_ids[cls], enums.class_ids[starting]
		choices.append({"dialogActionId": action, "dialogAction": rules.action_name(action), "playerClass": cls, "classId": cid,
		                "validFor": starting if sid < cid <= sid + 2 else None})
	masters = [m for m in rules.trainers[race] if m.starting_class == starting]
	var, group = rules.trainer_vars[race][starting]
	experience = {"startExp": {str(lvl): pdata.start_exp(lvl) for lvl in range(1, min(pdata.max_level(), 66) + 1)},
	              "maxLevel": pdata.max_level(),
	              # setExp for a starting class (PlayerCommonData.java:273-288): maxLevel 10, so the level stops at 9 with exp startExp(10)
	              "nonDaevaCap": {"level": 9, "exp": pdata.start_exp(10)}}
	elements = _skill_elements(data, set(skills)) if skills else {}
	charges = _charge_entries(data) if skills else {}
	known_after = dict(ch.skills)
	weapon_reports = [weapon_report(data, rules, w, known_after) for w in weapons]
	if weapon_reports and "weaponGroup" not in character:
		character = {**character, "weaponGroup": weapon_reports[0]["itemGroup"]}
	skill_reports = [skill_report(elements, s, charges, character) for s in skills]
	launched_ids = {l["skillId"] for s in skill_reports for l in s["launches"]} - set(skills)
	launched = {}
	if launched_ids:
		launched_elements = _skill_elements(data, launched_ids)
		for launched_id in sorted(launched_ids):
			launched[str(launched_id)] = skill_report(launched_elements, launched_id, charges, {"weaponGroup": character.get("weaponGroup")})
	return {
		"format": "aion-m5e-progression",
		"version": 1,
		"race": race,
		"playerClass": player_class,
		"startingClass": starting,
		"experience": experience,
		"classChange": {"questId": ASCENSION_QUESTS[race], "pageId": page, "choices": choices,
		                "statsTemplates": {cls: {"healthMultiplier": enums.classes[cls][3], "willMultiplier": enums.classes[cls][4]}
		                                   for cls in sorted(enums.classes)}},
		"trainer": {"questId": rules.trainer_quests[race], "npcId": masters[0].npc_id if masters else None,
		            "page": masters[0].page if masters else None, "questVar": var, "rewardGroup": group,
		            "masters": [{"npcId": m.npc_id, "startingClass": m.starting_class, "page": m.page} for m in rules.trainers[race]]},
		"messages": rules.messages,
		"steps": step_reports,
		"character": character,
		"skills": skill_reports,
		"launched": launched,
		"weapons": weapon_reports,
		# per --stigma: the stone equipped by the character after the steps (stigma_report)
		"stigmas": [stigma_report(data, java_src, progression, ch, s, prices) for s in stigmas],
	}


def _class_ids(java_src: Path) -> dict[str, int]:
	"""PlayerClass.getClassId(): the first constructor argument, `(byte) n`"""
	from m5a.creation import enum_constants
	ids = {}
	for name, args in enum_constants(java_src / "com" / "aionemu" / "gameserver" / "model" / "PlayerClass.java", "PlayerClass"):
		first = (args or "").split(",")[0].strip()
		match = re.fullmatch(r"(?:\(byte\)\s*)?(\d+)", first)
		if not match:
			raise OracleError(f"PlayerClass.{name}: cannot read the class id {first!r}")
		ids[name] = int(match.group(1))
	return ids
