"""m5d-quest and m5d-quests: one quest end to end, and the quests a character meets on a map (m5d-plan.md G-01, D9).

Java rules, each with the method the value comes from (the template kinds and their steps are in templates.py, the Java readers in javasrc.py):
- the data: QuestsData.afterUnmarshal (QuestsData.java:28-45, a later <quest> of an id replaces the earlier one) over QuestTemplate's JAXB
  fields (QuestTemplate.java:39-131, the defaults max_repeat_count 1, category QUEST, extra_category/mentor_type/target NONE, restricted and
  data_driven false, FinishedQuestCond.reward -1, QuestItems.count 1, QuestDrop.chance null = 100 - QuestDrop.getChance); XMLQuests.afterUnmarshal
  (XMLQuests.java:33-39): the 16 @XmlElements tags of quest_script_data, questsById.put in document order (a later id replaces the value and
  keeps the HashMap position). `quest_zone` is not bound by QuestTemplate: JAXB drops it and so does this oracle (reported, never used);
- the registry: QuestEngine.init (QuestEngine.java:86-110) loads the Java handlers (data/handlers/quest, QuestHandlerLoader) first and then
  `for (XMLQuest q : XML_QUESTS.getAllQuests()) q.register(this)`, in the iteration order of XMLQuests' HashMap; addQuestHandler
  (:892-898) is putIfAbsent, so a Java handler wins over an XML template of the same id. m5d-plan.md D9: until phase 6 the C++ registry has
  the XML quests only - `registry: xml` marks them;
- QuestService.checkStartConditions (QuestService.java:302-398) with QuestState.canRepeat/isStartable (QuestState.java:117-131),
  XMLStartCondition.check (XMLStartCondition.java:46-133, `equipped` is only checked with warn = true), QuestTemplate.getRequiredConditionCount
  (QuestTemplate.java:170-188, with CraftConfig.MAX_MASTER_CRAFTING_SKILLS for a master quest), inventoryItemCheck (:602-620, the cube only -
  equipped items and kinah are not in Inventory.getFirstItemByItemId - of the new character plus what --inventory states; an item a quest of
  the list may have given (finishQuest's reward items, :88-103, giveQuestItem's work items) or taken (collectItemCheck, removeQuestWorkItems,
  :935-947) has no answer without --inventory), checkCombineSkill (:472-514), the npc faction arm (:382-391);
- the markers: WorldMapInstance.addObject (WorldMapInstance.java:109-131) adds the onQuestStart quests of every Npc spawned into the map
  instance to its questIds (map-wide, never removed; QuestEngine is initialized before SpawnEngine.spawnAll, GameServer.java:100, 125); PlayerController.updateNearbyQuests (PlayerController.java:170-177) sends every quest
  passing checkStartConditions(player, id, false, 2, false, false, false) with getLevelRequirementDiff (QuestService.java:806-809), and
  SM_NEARBY_QUESTS (SM_NEARBY_QUESTS.java:14-31) sets bit 17 when that difference is positive (the grey marker). Spawned at startup is
  SpawnEngine.spawnAll/spawnInstance (SpawnEngine.java:119-189): open-world maps only, difficulty 0, no handler groups, temporary groups
  only in spawn time, a pool (0 < pool < spots) spawns pool spots, the other groups every spot in time; gatherables are not Npcs;
- the rewards: QuestService.finishQuest (:77-118), validateAndFixRewardGroup (:123-142), getRewardItems (:144-209), getRewardIndex (:216-218),
  giveReward (:220-244); Rates.XP_QUEST / QUEST_KINAH / AP_QUEST / GP and Rates.get, calcXpRate (Rates.java:29-35, 94-100, 108-114, 129-135,
  153-181) in float arithmetic with membership 0 and the rates of Config.loadProperties (Config.java:81-95: the profile, config/mygs.properties
  by default, over the defaults folders); PlayerCommonData.addExp (PlayerCommonData.java:167-222: no exp in world 301200000, repose energy
  from level 10, salvation from level 15), setExp (:273-291: the level after the reward) and setDp (:463-474: a starting class gets no DP);
- the follow-up: AbstractQuestHandler.sendQuestEndDialog (AbstractQuestHandler.java:413-474) at the level after the reward - the onTalkEvent
  loop over quests in REWARD (a report_on_levelup quest ReportOnLevelUp started at enter world or at the level change, ReportOnLevelUp.java:
  52-65), then isAcceptableQuest (:476-484) over the end npc's QuestNpc.onQuestStart, a HashSet<>(0) (QuestNpc.java:27) filled in
  registration order; a follow-up handler that answers false makes DialogService send the next page (DialogService.java:282-291);
- the wire order of SM_NEARBY_QUESTS: the buckets of updateNearbyQuests' new HashMap<>() (java.util.HashMap putVal/resize/treeifyBin).
"""

from __future__ import annotations

import re
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field, replace
from pathlib import Path

from staticdata_oracle import OracleError

from m5a.creation import RACES, JavaEnums, creation_report, learn_new_skills
from m5a.data import StaticData, java_int
from m5a.javafloat import f32, to_long
from m5a.spawns import GameClock, is_gatherable, load_groups
from .javasrc import (GS, ConfigValue, DialogTables, JaxbModel, JObj, check_registration_order, float_array, hash_bucket_groups,
                      hash_iteration_order, java_enum_constants, java_integer, read_config, read_source, scan_java_handlers, strip_comments)
from .templates import CRAFTING_LEVEL_REWARDS, KINAH, KINDS, USED_FIELDS, Flow, Registration, registration, template_classes

RACE_OF_WORLD_TYPE = {"ELYSEA": "ELYOS", "ASMODAE": "ASMODIANS"}
STATUS_START, STATUS_REWARD, STATUS_COMPLETE, STATUS_LOCKED = "START", "REWARD", "COMPLETE", "LOCKED"
UNLIMITED_REPEAT = 255  # QuestState.canRepeat / checkStartConditions: max_repeat_count 255 never runs out
MASTER_SKILL_POINTS = 499  # QuestTemplate.isMaster
ACCEPTABLE_MINLEVEL_NEVER = 99  # AbstractQuestHandler.isAcceptableQuest: a follow-up with minlevel_permitted 99 is never offered
NON_DAEVA_MAX_LEVEL = 10  # PlayerCommonData.setExp: maxLevel 10 for a character that is not a daeva (level at most 9, exp at most level 10's)
DEFAULT_PROFILE = "<config>/mygs.properties"


def _search(path: Path, pattern: str, what: str, flags: int = re.DOTALL) -> re.Match:
	match = re.search(pattern, strip_comments(read_source(path)), flags)
	if not match:
		raise OracleError(f"{path.name}: cannot read {what} (the Java source does not have the shape this oracle was written against)")
	return match


@dataclass(frozen=True)
class QuestRules:
	"""The formula shapes, literals and configuration defaults the report needs, read from the Java sources and config/."""

	config: dict[str, ConfigValue]
	xp_quest_rate: float       # RatesConfig.XP_QUEST_RATES[membership 0]
	kinah_rate: float          # RatesConfig.QUEST_KINAH_RATES[0]
	ap_rate: float             # RatesConfig.AP_QUEST_RATES[0]
	gp_rate: float             # RatesConfig.GP_RATES[0]
	legion_bonus: float        # calcXpRate: `endRate *= 1.1f` for a legion with bonus
	no_exp_world: int          # PlayerCommonData.addExp: `player.getWorldId() == 301200000` gains nothing
	repose_level: int          # isReadyForReposeEnergy: level >= 10
	salvation_level: int       # isReadyForSalvationPoints: level >= 15
	quest_size_limit: int      # CustomConfig.BASIC_QUEST_SIZE_LIMIT
	max_master_crafting: int   # CraftConfig.MAX_MASTER_CRAFTING_SKILLS
	combine_any: list[int]     # checkCombineSkill, combineskill -1: every skill
	combine_any_gathering: list[int]  # ... of which these two are left out for npc factions 12 and 13
	combine_any_excluded_factions: list[int]
	first_rank: int            # AbyssRankEnum's first constant (the rank of a character with 0 AP)
	class_selectable: dict[str, str]  # PlayerClass -> QuestTemplate field (getSelectableRewardByClass)

	@staticmethod
	def read(java_src: Path, config_dir: Path, events_dir: Path | None, profile: Path | None = None) -> "QuestRules":
		"""`profile`: the override file Config.loadProperties reads over the defaults folders (config/mygs.properties), None for none."""
		base = Path(java_src) / GS
		rates = base / "model" / "gameobjects" / "player" / "Rates.java"
		shapes = {
			"XP_QUEST": r"XP_QUEST\s*\{\s*@Override\s*public long calcResult\(Player player, long xp\)\s*\{\s*return \(long\) \(xp \* calcXpRate\("
			            r"player, RatesConfig\.XP_QUEST_RATES, StatEnum\.BOOST_QUEST_XP_RATE\)\);",
			"QUEST_KINAH": r"QUEST_KINAH\s*\{\s*@Override\s*public long calcResult\(Player player, long kinah\)\s*\{\s*return \(long\) \(kinah \* "
			               r"get\(player, RatesConfig\.QUEST_KINAH_RATES\)\);",
			"AP_QUEST": r"AP_QUEST\s*\{\s*@Override\s*public long calcResult\(Player player, long ap\)\s*\{\s*return \(long\) \(ap \* get\(player, "
			            r"RatesConfig\.AP_QUEST_RATES\)\);",
			"GP": r"\bGP\s*\{\s*@Override\s*public long calcResult\(Player player, long gp\)\s*\{\s*return \(long\) \(gp \* get\(player, "
			      r"RatesConfig\.GP_RATES\)\);",
			"get": r"return membershipRates\[Math\.min\(membershipRates\.length - 1, membershipLevel\)\];",
			"toIntExact": r"public int calcResult\(Player player, int value\)\s*\{\s*long result = calcResult\(player, \(long\) value\);\s*try\s*\{\s*"
			              r"return Math\.toIntExact\(result\);",
		}
		for name, pattern in shapes.items():
			_search(rates, pattern, f"Rates.{name}")
		xp_rate = _search(rates, r"float endRate = get\(player, membershipRates\);\s*endRate \*= player\.getGameStats\(\)\.getStat\(boostRate, 100\)"
		                         r"\.getCurrent\(\) / 100f;\s*if \(player\.isLegionMember\(\) && player\.getLegion\(\)\.hasBonus\(\)\)\s*endRate \*= "
		                         r"([0-9.]+)f;\s*return endRate;", "Rates.calcXpRate")
		service = base / "services" / "QuestService.java"
		for what, pattern in {
			"giveReward kinah": r"player\.getInventory\(\)\.increaseKinah\(Rates\.QUEST_KINAH\.calcResult\(player, rewards\.getKinah\(\)\)",
			"giveReward exp": r"player\.getCommonData\(\)\.addExp\(rewards\.getExp\(\), Rates\.XP_QUEST,",
			"giveReward ap": r"if \(DataManager\.QUEST_DATA\.getQuestById\(env\.getQuestId\(\)\)\.getCategory\(\) != QuestCategory\.NON_COUNT\)\s*"
			                 r"ap = Rates\.AP_QUEST\.calcResult\(player, ap\);",
			"giveReward gp": r"GloryPointsService\.addGp\(player\.getObjectId\(\), Rates\.GP\.calcResult\(player, rewards\.getGp\(\)\)\)",
			"giveReward dp": r"player\.getCommonData\(\)\.addDp\(rewards\.getDp\(\)\);",
			"finishQuest extended": r"template\.getExtendedRewards\(\) != null && qs\.getCompleteCount\(\) == template\.getRewardRepeatCount\(\) - 1",
			"nearby quests": r"int levelDiff = template\.getMinlevelPermitted\(\) - allowedDiffToMinLevel - player\.getLevel\(\);\s*if \(levelDiff > 0\)",
		}.items():
			_search(service, pattern, f"QuestService {what}")
		controller = base / "controllers" / "PlayerController.java"
		_search(controller, r"QuestService\.checkStartConditions\(getOwner\(\), questId, false, 2, false, false, false\)", "updateNearbyQuests")
		common = base / "model" / "gameobjects" / "player" / "PlayerCommonData.java"
		no_exp = _search(common, r"player\.getWorldId\(\) == (\d+)\)\s*return;", "addExp's no-exp world")
		repose = _search(common, r"isReadyForReposeEnergy\(\)\s*\{\s*return getLevel\(\) >= (\d+);", "isReadyForReposeEnergy")
		salvation = _search(common, r"isReadyForSalvationPoints\(\)\s*\{\s*return getLevel\(\) >= (\d+);", "isReadyForSalvationPoints")
		_search(common, r"public void setDp\(int dp\)\s*\{\s*if \(playerClass\.isStartingClass\(\)\)\s*return;", "setDp")
		combine = _search(service, r"if \(template\.getCombineSkill\(\) == -1\) \{(.*?)\} else \{", "checkCombineSkill's any-skill list")
		gathering = re.search(r"if \(template\.getNpcFactionId\(\) != (\d+) && template\.getNpcFactionId\(\) != (\d+)\) \{(.*?)\}", combine.group(1),
		                      re.DOTALL)
		if not gathering:
			raise OracleError("QuestService.checkCombineSkill: the npc faction exclusion is not the shape this oracle was written against")
		all_skills = [int(v) for v in re.findall(r"skills\.add\((\d+)\);", combine.group(1))]
		gathering_skills = [int(v) for v in re.findall(r"skills\.add\((\d+)\);", gathering.group(3))]
		first_rank = java_enum_constants(base / "utils" / "stats" / "AbyssRankEnum.java", "AbyssRankEnum")[0]
		template = strip_comments(read_source(base / "model" / "templates" / "QuestTemplate.java"))
		body = re.search(r"getSelectableRewardByClass\(PlayerClass playerClass\)\s*\{(.*?)return Collections\.emptyList\(\);\s*\}", template, re.DOTALL)
		if not body:
			raise OracleError("QuestTemplate.getSelectableRewardByClass does not have the shape this oracle was written against")
		class_selectable = dict(re.findall(r"case\s+(\w+)\s*:\s*return\s+(\w+)\s*==\s*null", body.group(1)))
		config = {field_name: read_config(java_src, config_dir, events_dir, ("main", class_name), field_name, profile) for field_name, class_name in (
			("XP_QUEST_RATES", "RatesConfig.java"), ("QUEST_KINAH_RATES", "RatesConfig.java"), ("AP_QUEST_RATES", "RatesConfig.java"),
			("GP_RATES", "RatesConfig.java"), ("BASIC_QUEST_SIZE_LIMIT", "CustomConfig.java"), ("MAX_MASTER_CRAFTING_SKILLS", "CraftConfig.java"))}
		for value in config.values():
			if value.event_overrides:
				raise OracleError(f"{value.key} is overridden by timed events ({value.event_overrides}): the reward would depend on the date")

		def rate(name: str) -> float:
			return f32(float(float_array(config[name])[0]))  # Rates.get with membership 0: the first value

		return QuestRules(config, rate("XP_QUEST_RATES"), rate("QUEST_KINAH_RATES"), rate("AP_QUEST_RATES"), rate("GP_RATES"),
		                  f32(float(xp_rate.group(1))), int(no_exp.group(1)), int(repose.group(1)), int(salvation.group(1)),
		                  java_int(config["BASIC_QUEST_SIZE_LIMIT"].effective, "BASIC_QUEST_SIZE_LIMIT"),
		                  java_int(config["MAX_MASTER_CRAFTING_SKILLS"].effective, "MAX_MASTER_CRAFTING_SKILLS"),
		                  all_skills, gathering_skills, [int(gathering.group(1)), int(gathering.group(2))],
		                  java_int((first_rank[1] or "").split(",")[0], "AbyssRankEnum first id"), class_selectable)

	def xp_end_rate(self) -> float:
		"""Rates.calcXpRate for membership 0, no BOOST_QUEST_XP_RATE stat function (its getCurrent() is the base 100) and no legion bonus."""
		return f32(self.xp_quest_rate * f32(100 / f32(100.0)))

	def quest_exp(self, exp: int) -> int:
		"""Rates.XP_QUEST.calcResult: (long) (xp * endRate), float arithmetic (a long times a float is a float)."""
		return to_long(f32(f32(float(exp)) * self.xp_end_rate()))

	def quest_kinah(self, kinah: int) -> int:
		"""Rates.QUEST_KINAH.calcResult: (long) (kinah * rate)."""
		return to_long(f32(f32(float(kinah)) * self.kinah_rate))

	def int_rate(self, value: int, rate: float, what: str) -> int:
		"""Rates.calcResult(Player, int): the long result must fit an int, else Java logs an error and keeps the template value."""
		result = to_long(f32(f32(float(value)) * rate))
		if not -2**31 <= result < 2**31:
			return value
		return result

	def as_json(self) -> dict:
		return {
			"membership": 0,
			"xpQuestRate": self.xp_quest_rate, "questKinahRate": self.kinah_rate, "apQuestRate": self.ap_rate, "gpRate": self.gp_rate,
			"xpEndRate": self.xp_end_rate(),
			"config": {name: value.as_json() for name, value in self.config.items()},
			"basicQuestSizeLimit": self.quest_size_limit, "maxMasterCraftingSkills": self.max_master_crafting,
		}


@dataclass
class XmlQuest:
	quest_id: int
	tag: str
	data_class: str
	obj: JObj
	file: str


class QuestWorld:
	"""The quest data of one static_data directory and Java tree: templates, XML quests, Java handlers, registrations."""

	def __init__(self, data: StaticData, java_src: Path, handlers_dir: Path | None = None, config_dir: Path | None = None,
	             profile: Path | str | None = DEFAULT_PROFILE):
		"""`profile`: DEFAULT_PROFILE reads <config>/mygs.properties when it is there (Config.java:91, a missing file is no error), None reads
		no profile, a path names a profile that must exist."""
		java_src = Path(java_src)
		self.data = data
		self.java_src = java_src
		self.jaxb = JaxbModel(java_src)
		self.dialogs = DialogTables.read(java_src)
		check_registration_order(java_src)
		self.template_classes = template_classes(self.jaxb)
		config_dir = Path(config_dir) if config_dir else java_src.parent / "config"
		if profile == DEFAULT_PROFILE:
			profile = config_dir / "mygs.properties" if (config_dir / "mygs.properties").is_file() else None
		self.rules = QuestRules.read(java_src, config_dir, data.dir / "events" / "timed_events", Path(profile) if profile is not None else None)
		self._template_elements: dict[int, ET.Element] = {}
		self._templates: dict[int, JObj] = {}
		self.template_duplicates: list[int] = []
		for element in data.children("quests", "quest"):
			quest_id = java_int(element.get("id"), "quest id")
			if quest_id in self._template_elements:
				self.template_duplicates.append(quest_id)
			self._template_elements[quest_id] = element
		self.xml: dict[int, XmlQuest] = {}
		self.xml_insertion: list[int] = []
		self.xml_duplicates: list[str] = []
		self.ignored_script_elements: list[str] = []
		for path in data.files("quest_scripts"):
			try:
				root = ET.parse(path).getroot()
			except ET.ParseError as e:
				raise OracleError(f"{path}: {e}") from e
			rel = path.relative_to(data.dir).as_posix()
			for element in root:
				data_class = self.jaxb.quest_kinds.get(element.tag)
				if data_class is None:
					self.ignored_script_elements.append(f"{rel}: <{element.tag}>")  # JAXB drops a tag @XmlElements does not name
					continue
				obj = self.jaxb.unmarshal(element, data_class, f"{rel} <{element.tag}>")
				quest_id = obj["id"]
				if quest_id in self.xml:
					self.xml_duplicates.append(f"{quest_id}: {self.xml[quest_id].file} and {rel}")
				else:
					self.xml_insertion.append(quest_id)
				self.xml[quest_id] = XmlQuest(quest_id, element.tag, data_class, obj, rel)
		self.handlers_dir = Path(handlers_dir) if handlers_dir else java_src.parent / "data" / "handlers" / "quest"
		self.java, self.java_duplicates = scan_java_handlers(self.handlers_dir)
		self._registrations: dict[int, Registration] | None = None
		self._inventory_cache: dict[tuple[str, str], tuple[set[int], dict]] = {}
		self._skills_cache: dict[tuple[str, str, int], dict[int, int]] = {}
		self._starting_items: dict[str, list[int]] | None = None
		self._npc_index: tuple[dict[int, dict], dict[int, dict[int, int]]] | None = None
		self._spawn_maps: list[ET.Element] | None = None
		self._xp_sources: list[str] | None = None
		self._experience: list[int] | None = None

	# --- the data -------------------------------------------------------------------------------------------------------------------------

	def template(self, quest_id: int) -> JObj | None:
		if quest_id not in self._templates:
			element = self._template_elements.get(quest_id)
			if element is None:
				return None
			self._templates[quest_id] = self.jaxb.unmarshal(element, "QuestTemplate", f"quest {quest_id}")
		return self._templates[quest_id]

	@property
	def template_ids(self) -> list[int]:
		return list(self._template_elements)

	def work_items(self, t: JObj) -> list[dict] | None:
		container = t["questWorkItems"]
		if container is None:
			return None
		return [{"itemId": i["itemId"], "count": i["count"]} for i in container["questWorkItem"] or []]

	def action_items(self, t: JObj) -> list[int]:
		"""AbstractQuestHandler.loadActionItems: the quest_drop npcs with npcId / 100000 == 7 (a HashSet; reported in document order)."""
		result = []
		for drop in t["questDrop"] or []:
			if drop["npcId"] is None:
				raise OracleError(f"quest {t['id']}: a <quest_drop> without npc_id (AbstractQuestHandler.loadActionItems throws NullPointerException)")
			if drop["npcId"] // 100000 == 7 and drop["npcId"] not in result:
				result.append(drop["npcId"])
		return result

	def collect_items(self, t: JObj) -> list[dict] | None:
		container = t["collectItems"]
		if container is None:
			return None
		items = []
		for c in container["collectItem"] or []:
			if c["itemId"] is None or c["count"] is None:
				raise OracleError(f"quest {t['id']}: a <collect_item> without item_id or count (collectItemCheck unboxes null)")
			items.append({"itemId": c["itemId"], "count": c["count"]})
		return items

	def drops(self, t: JObj) -> list[dict]:
		"""The <quest_drop>s: QuestService.getQuestDrop/isQuestDrop (QuestService.java:666-796) drop one item per kill with Rnd.chance() <
		chance (percent, default 100) for a player with the quest in START, at var0 == collecting_step when that is not 0, while the collect
		item count is below the required one."""
		return [{"npcId": d["npcId"], "itemId": d["itemId"], "chance": 100 if d["chance"] is None else d["chance"],
		         "dropEachMember": d["dropEachMember"], "collectingStep": d["collecting_step"], "questObject": d["npcId"] // 100000 == 7}
		        for d in t["questDrop"] or []]

	# --- the registry ---------------------------------------------------------------------------------------------------------------------

	def registrations(self) -> dict[int, Registration]:
		"""Every XML quest's registration, in XMLQuests.getAllQuests order (QuestEngine.init). Java would stop at the first template whose
		constructor throws, so a quest without a quest_data template (the template handlers dereference it) is refused for the whole
		registry."""
		if self._registrations is None:
			order = hash_iteration_order(self.xml_insertion)
			result = {}
			for quest_id in order:
				xml = self.xml[quest_id]
				template = self.template(quest_id)
				if template is None:
					raise OracleError(f"XML quest {quest_id} ({xml.file}) has no quest_data template: its template handler throws "
					                  "NullPointerException in QuestEngine.init")
				result[quest_id] = registration(self, xml.tag, quest_id, xml.obj, template)
			self._registrations = result
		return self._registrations

	def xml_start_npcs(self) -> dict[int, list[int]]:
		"""npc -> the XML quests registering onQuestStart there, in registration order."""
		index: dict[int, list[int]] = {}
		for quest_id, reg in self.registrations().items():
			for npc in reg.quest_start:
				index.setdefault(npc, []).append(quest_id)
		return index

	def xml_talk_npcs(self) -> dict[int, list[int]]:
		"""npc -> the XML quests registering onTalkEvent there, in registration order (QuestNpc.onTalkEvent is an ArrayList)."""
		index: dict[int, list[int]] = {}
		for quest_id, reg in self.registrations().items():
			for npc in reg.talk:
				index.setdefault(npc, []).append(quest_id)
		return index

	def java_start_npcs(self) -> dict[int, list[int]]:
		index: dict[int, list[int]] = {}
		for quest_id, handler in self.java.items():
			for npc in handler.start_npcs:
				index.setdefault(npc, []).append(quest_id)
		return index

	def registry_of(self, quest_id: int) -> str:
		"""QuestEngine's handler for the id in the Java server: a Java handler first, then the XML template (putIfAbsent)."""
		if quest_id in self.java:
			return "java"
		if quest_id in self.xml:
			return "xml"
		return "none"

	def census(self) -> dict:
		by_kind: dict[str, int] = {}
		for xml in self.xml.values():
			by_kind[xml.tag] = by_kind.get(xml.tag, 0) + 1
		templates = set(self._template_elements)
		both = sorted(set(self.xml) & set(self.java))
		return {
			"questTemplates": len(templates),
			"xmlQuests": len(self.xml),
			"xmlByKind": dict(sorted(by_kind.items(), key=lambda kv: (-kv[1], kv[0]))),
			"xmlFiles": len(self.data.files("quest_scripts")),
			"xmlDuplicateIds": self.xml_duplicates,
			"xmlIgnoredElements": self.ignored_script_elements,
			"xmlWithoutTemplate": sorted(set(self.xml) - templates),
			"javaHandlers": len(self.java),
			"javaDuplicateIds": self.java_duplicates,
			"javaWithoutTemplate": sorted(set(self.java) - templates),
			"both": both,
			"neither": len(templates - set(self.xml) - set(self.java)),
		}

	# --- the character --------------------------------------------------------------------------------------------------------------------

	def experience(self) -> list[int]:
		"""PlayerExperienceTable.experience: the start exp of level i + 1 at index i (getStartExpForLevel), getMaxLevel() is its length."""
		if self._experience is None:
			self._experience = [java_integer(e.text or "", 64, "player_experience_table exp")
			                    for e in self.data.children("player_experience_table", "exp")]
			if not self._experience:
				raise OracleError("player_experience_table has no <exp>")
		return self._experience

	def starting_item_ids(self) -> dict[str, list[int]]:
		"""player_initial_data: class -> the item ids of <player_data class=...><items>, read without the item templates."""
		if self._starting_items is None:
			result: dict[str, list[int]] = {}
			for element in self.data.children("player_initial_data", "player_data"):
				result[element.get("class")] = [java_int(i.get("id"), "player_data item id") for i in element.iter("item")]
			self._starting_items = result
		return self._starting_items

	def creation_inventory(self, race: str, player_class: str) -> set[int]:
		"""The items of a new character that are in the cube, not equipped (m5a-creation: every armor or weapon is equipped; kinah is the
		inventory's kinah item, which getFirstItemByItemId does not see)."""
		key = (race, player_class)
		if key not in self._inventory_cache:
			report = creation_report(self.data, self.java_src, race, player_class)
			self._inventory_cache[key] = ({i["itemId"] for i in report["items"] if not i["equipped"] and not i["kinah"]}, report)
		return self._inventory_cache[key][0]

	def quest_xp_stat_sources(self) -> list[str]:
		"""The item and skill files that name BOOST_QUEST_XP_RATE, the stat calcXpRate multiplies the quest rate by (a raw text search: none
		today, so no item, passive or buff changes the quest exp of any character)."""
		if self._xp_sources is None:
			found = []
			for holder in ("item_templates", "skill_data"):
				for path in self.data.files(holder):
					if b"BOOST_QUEST_XP_RATE" in path.read_bytes():
						found.append(path.relative_to(self.data.dir).as_posix())
			self._xp_sources = found
		return self._xp_sources

	def spawn_maps(self) -> list[ET.Element]:
		"""The <spawn_map>s of the spawns holder, parsed once."""
		if self._spawn_maps is None:
			self._spawn_maps = list(self.data.children("spawns", "spawn_map"))
		return self._spawn_maps

	def npc_index(self) -> tuple[dict[int, dict], dict[int, dict[int, int]]]:
		"""(npc id -> name/level/ai of its template, npc id -> map id -> regular spawn spots), built once."""
		if self._npc_index is None:
			templates = {}
			for element in self.data.children("npc_templates", "npc_template"):
				npc_id = java_int(element.get("npc_id"), "npc_template npc_id")
				templates[npc_id] = {"name": element.get("name"), "level": java_int(element.get("level"), "level", 0), "ai": element.get("ai"),
				                     "template": True}
			spawns: dict[int, dict[int, int]] = {}
			for spawn_map in self.spawn_maps():
				map_id = java_int(spawn_map.get("map_id"), "spawn_map map_id")
				for spawn in spawn_map.findall("spawn"):
					per_map = spawns.setdefault(java_int(spawn.get("npc_id"), "spawn npc_id"), {})
					per_map[map_id] = per_map.get(map_id, 0) + len(spawn.findall("spot"))
			self._npc_index = (templates, spawns)
		return self._npc_index

	def autolearn_skills(self, race: str, player_class: str, level: int) -> dict[int, int]:
		key = (race, player_class, level)
		if key not in self._skills_cache:
			self._skills_cache[key] = learn_new_skills(self.data, JavaEnums(self.java_src), race, player_class, 1, level)
		return self._skills_cache[key]


@dataclass
class QuestStateInfo:
	status: str
	complete_count: int = 0
	reward_group: int | None = None
	time_based_pending: bool = False  # a time based quest just completed: nextRepeatTime is the next 9:00, canRepeat is false until then


@dataclass
class Character:
	"""The player checkStartConditions looks at: a character of the given race, class, level and gender with the new character's items
	(plus what `inventory` states), the autolearn skills, no title (-1), abyss rank 1, no npc faction, no repose energy or salvation points and
	the quest list `quests`."""

	world: QuestWorld
	race: str
	player_class: str
	starting_class: str
	level: int
	gender: str | None
	quests: dict[int, QuestStateInfo] = field(default_factory=dict)
	title_id: int = -1
	daeva: bool = False
	inventory: dict[int, int] = field(default_factory=dict)  # --inventory: item -> count in the cube, 0 when the caller states it is not there

	def has_starting_item(self, item_id: int) -> bool:
		"""Whether the new character's cube holds the item (m5a-creation: the items that are not equipped, not kinah)."""
		if item_id not in self.world.starting_item_ids().get(self.starting_class, []):
			return False
		return item_id in self.world.creation_inventory(self.race, self.starting_class)

	def skill_level(self, skill_id: int) -> int | None:
		if self.level >= 10:
			return None  # SkillLearnService from level 10 depends on the daeva state (m5a learn_new_skills refuses it)
		return self.world.autolearn_skills(self.race, self.player_class, self.level).get(skill_id, 0)

	def as_json(self) -> dict:
		return {"race": self.race, "playerClass": self.player_class, "level": self.level, "gender": self.gender, "titleId": self.title_id,
		        "abyssRank": self.world.rules.first_rank, "npcFactions": [], "membership": 0, "reposeEnergy": 0, "salvationPercent": 0,
		        "quests": {str(q): {"status": s.status, "completeCount": s.complete_count, "rewardGroup": s.reward_group}
		                   for q, s in sorted(self.quests.items())},
		        "inventory": {"stated": {str(i): c for i, c in sorted(self.inventory.items())},
		                      "otherwise": "the new character's items that are not equipped (m5a-creation); an item a quest of the list may "
		                                   "have given or taken must be stated with --inventory"},
		        "skills": "SkillLearnService.learnNewSkills(1, level) (m5a-creation)"}


def make_character(world: QuestWorld, race: str, player_class: str, level: int, gender: str | None) -> Character:
	if race not in RACES:
		raise OracleError(f"race must be one of {RACES}")
	enums_classes = world.jaxb.enum_names["PlayerClass"]
	if player_class not in enums_classes:
		raise OracleError(f"unknown player class {player_class}")
	if gender is not None and gender not in world.jaxb.enum_names["Gender"]:
		raise OracleError(f"unknown gender {gender}")
	enums = JavaEnums(world.java_src)
	starting = enums.classes[player_class][0]
	# PlayerExperienceTable.getLevelForExp and PlayerCommonData.setExp: the level is at most getMaxLevel() - 1 (the table's length - 1)
	max_level = len(world.experience()) - 1
	if not 1 <= level <= max_level:
		raise OracleError(f"level {level} is outside 1..{max_level} (player_experience_table has {max_level + 1} <exp>, setExp caps the level "
		                  "at getMaxLevel() - 1)")
	if starting and level > NON_DAEVA_MAX_LEVEL - 1:
		raise OracleError(f"level {level}: {player_class} is a starting class, and a character that is not a daeva is capped at level 9 "
		                  "(PlayerCommonData.setExp)")
	if not starting and level < NON_DAEVA_MAX_LEVEL:
		raise OracleError(f"level {level}: {player_class} is a daeva class, which a character only has from the ascension at level 10")
	return Character(world, race, player_class, enums.starting_classes[player_class], level, gender, daeva=not starting)


def exp_state(world: QuestWorld, char: Character, exp: int) -> tuple[int, int]:
	"""PlayerCommonData.setExp (PlayerCommonData.java:273-291) for an online character: exp capped at getStartExpForLevel(maxLevel) and the
	level min(getLevelForExp(exp), maxLevel - 1), maxLevel the table's length for a daeva, 10 otherwise. (exp, level)."""
	table = world.experience()
	max_level = len(table) if char.daeva else NON_DAEVA_MAX_LEVEL
	exp = min(exp, table[max_level - 1])
	level = 0
	for i in range(len(table), 0, -1):  # PlayerExperienceTable.getLevelForExp
		if exp >= table[i - 1]:
			level = i
			break
	level = min(level, len(table) - 1)
	return exp, min(level, max_level - 1)


def apply_inventory(char: Character, items=()) -> None:
	"""--inventory ITEM[:COUNT]: the cube holds COUNT (default 1) of ITEM, 0 states that it does not (QuestService.inventoryItemCheck only asks
	Inventory.getFirstItemByItemId, so any count above 0 passes)."""
	for text in items:
		item, _, count = str(text).partition(":")
		item_id = java_int(item, "--inventory item id")
		number = java_int(count, "--inventory count") if count else 1
		if number < 0:
			raise OracleError(f"--inventory {text}: a negative count")
		if item_id in char.inventory:
			raise OracleError(f"--inventory {item_id} is given twice")
		char.inventory[item_id] = number


def quest_item_history(world: QuestWorld, char: Character) -> tuple[dict[int, list[str]], dict[int, list[str]], list[str]]:
	"""What the quests of the character's list may have done to its cube, which the oracle does not track: (item -> how a quest may have put
	it in, item -> how a quest may have taken it out, the completed quests with a random <bonus> item). A COMPLETE quest paid its reward items
	(QuestService.finishQuest -> ItemService.addItem, QuestService.java:88-103: any group, the selectable, extended and class lists) and its
	work order components, took its collect items (collectItemCheck(env, true)), its work items (removeQuestWorkItems, :935-947) and a
	report_to_many start item; its work items are listed as given too (a handler may give more than it takes). A quest in START or REWARD
	holds its work items (giveQuestItem), start item, work order components and possibly its quest drops and collect items."""
	gives: dict[int, list[str]] = {}
	takes: dict[int, list[str]] = {}
	bonus: list[str] = []

	def add(table: dict[int, list[str]], items, why: str) -> None:
		for item in items:
			if item and item != KINAH:
				table.setdefault(item, []).append(why)

	for quest_id, qs in sorted(char.quests.items()):
		t = world.template(quest_id)
		if t is None:
			continue
		xml = world.xml.get(quest_id)
		q = xml.obj if xml is not None else None
		work = [i["itemId"] for i in world.work_items(t) or []]
		collect = [c["itemId"] for c in world.collect_items(t) or []]
		components = [c["itemId"] for c in q["giveComponents"] or []] if xml is not None and xml.tag == "work_order" else []
		start_item = [q["startItemId"]] if xml is not None and xml.tag == "report_to_many" and q["startItemId"] else []
		if qs.status == STATUS_COMPLETE:
			rewards = [r for r in (t["rewards"] or []) + [t["extendedRewards"]] if r is not None]
			add(gives, [i["itemId"] for r in rewards for i in (r["rewardItem"] or []) + (r["selectableRewardItem"] or [])],
			    f"{quest_id} (COMPLETE) pays it as a reward item")
			add(gives, [i["itemId"] for f in world.rules.class_selectable.values() for i in t[f] or []],
			    f"{quest_id} (COMPLETE) pays it as a class-selectable reward")
			add(gives, work, f"{quest_id} (COMPLETE) gave it as a work item")
			add(gives, components, f"{quest_id} (COMPLETE) gave it as a work order component")
			if t["bonus"] is not None:
				bonus.append(f"{quest_id} (COMPLETE) paid a random <bonus type={t['bonus']['type']}> item")
			add(takes, collect, f"{quest_id} (COMPLETE) took it as a collect item")
			add(takes, work, f"{quest_id} (COMPLETE) took it as a work item")
			add(takes, start_item, f"{quest_id} (COMPLETE) took its start item")
		elif qs.status in (STATUS_START, STATUS_REWARD):
			add(gives, work, f"{quest_id} ({qs.status}) gave it as a work item")
			add(gives, start_item + components, f"{quest_id} ({qs.status}) started with it")
			add(gives, [d["itemId"] for d in t["questDrop"] or []], f"{quest_id} ({qs.status}) drops it")
			add(gives, collect, f"{quest_id} ({qs.status}) collects it")
	return gives, takes, bonus


def inventory_item_state(world: QuestWorld, char: Character, item_id: int) -> tuple[bool | None, str | None]:
	"""Whether Inventory.getFirstItemByItemId finds the item in the cube (QuestService.inventoryItemCheck, QuestService.java:602-620): what
	--inventory states, else the new character's cube - unless a quest of the character's list may have given the missing item or taken the
	starting one (quest_item_history), which makes the answer None with the reason."""
	if item_id in char.inventory:
		return char.inventory[item_id] > 0, None
	has = char.has_starting_item(item_id)
	gives, takes, bonus = quest_item_history(world, char)
	if not has and (item_id in gives or bonus):
		return None, (f"inventory item {item_id}: " + "; ".join(gives.get(item_id, []) + bonus) +
		              f" (state the cube with --inventory {item_id} or {item_id}:0)")
	if has and item_id in takes:
		return None, (f"inventory item {item_id} (a starting item): " + "; ".join(takes[item_id]) +
		              f" (state the cube with --inventory {item_id} or {item_id}:0)")
	return has, None


def apply_quest_list(world: QuestWorld, char: Character, completed=(), started=()) -> None:
	"""The character's quest list: `completed` quests (ID or ID:GROUP) COMPLETE once, with the reward group finishQuest used (GROUP, else
	validateAndFixRewardGroup's 0 when the quest has reward groups); `started` quests in START."""
	for text in completed:
		quest, _, group = str(text).partition(":")
		quest_id = java_int(quest, "--completed quest id")
		t = world.template(quest_id)
		if t is None:
			raise OracleError(f"--completed {quest_id}: no quest template")
		reward_group = java_int(group, "--completed reward group") if group else (0 if t["rewards"] else None)
		char.quests[quest_id] = QuestStateInfo(STATUS_COMPLETE, 1, reward_group, time_based_pending=t["repeatCycle"] is not None)
	for quest_id in started:
		if world.template(quest_id) is None:
			raise OracleError(f"--started {quest_id}: no quest template")
		if quest_id in char.quests:
			raise OracleError(f"quest {quest_id} is both completed and started")
		char.quests[quest_id] = QuestStateInfo(STATUS_START)


# --- QuestService.checkStartConditions ---------------------------------------------------------------------------------------------------


def can_repeat(t: JObj, qs: QuestStateInfo) -> bool:
	"""QuestState.canRepeat (QuestState.java:121-131)."""
	if qs.complete_count >= t["maxRepeatCount"] and t["maxRepeatCount"] != UNLIMITED_REPEAT:
		return False
	if t["repeatCycle"] is not None and qs.time_based_pending:
		return False
	return True


def required_condition_count(world: QuestWorld, t: JObj) -> int:
	"""QuestTemplate.getRequiredConditionCount (QuestTemplate.java:170-188)."""
	groups = t["startConds"] or []
	if not groups:
		return 0
	optional = sum(1 for g in groups if g["finished"])
	result = min(1, optional) + (len(groups) - optional)
	if t["combineSkillpoint"] == MASTER_SKILL_POINTS:
		result += 1 - world.rules.max_master_crafting
	return result


def start_condition_passes(world: QuestWorld, char: Character, group: JObj, warn: bool) -> bool:
	"""XMLStartCondition.check (XMLStartCondition.java:129-133) and its six checks (:51-127)."""
	for fqc in group["finished"] or []:
		qs = char.quests.get(fqc["questId"])
		if qs is None or qs.status != STATUS_COMPLETE or (fqc["reward"] >= 0 and (qs.reward_group is None or fqc["reward"] != qs.reward_group)):
			return False
		other = world.template(fqc["questId"])
		if other is not None and other["maxRepeatCount"] > 1:
			if other["maxRepeatCount"] != UNLIMITED_REPEAT and qs.complete_count != other["maxRepeatCount"]:
				return False
	for quest_id in group["unfinished"] or []:
		qs = char.quests.get(quest_id)
		if qs is not None and qs.status == STATUS_COMPLETE:
			return False
	for quest_id in group["acquired"] or []:
		qs = char.quests.get(quest_id)
		if qs is None or qs.status == STATUS_LOCKED:
			return False
	for quest_id in group["noacquired"] or []:
		qs = char.quests.get(quest_id)
		if qs is not None and qs.status in (STATUS_START, STATUS_REWARD):
			return False
	if warn and group["equipped"]:
		raise OracleError("an <equipped> start condition with warn = true (the dialog path) is not modelled")
	if group["requiredTitle"] != 0 and char.title_id != group["requiredTitle"]:
		return False
	return True


def check_start_conditions(world: QuestWorld, char: Character, quest_id: int, warn: bool = False, allowed_diff: int = 0,
                           skip_started: bool = False, skip_repeat: bool = False, skip_xml: bool = False) -> tuple[bool | None, str | None]:
	"""QuestService.checkStartConditions (QuestService.java:302-398): (result, the check that failed); None when the answer depends on
	something the caller did not give (the gender, an inventory item a quest of the list may have given or taken) or the oracle does not know
	(a crafting skill level from level 10 on)."""
	qs = char.quests.get(quest_id)
	t = world.template(quest_id)
	if qs is not None:
		if not skip_started and qs.status in (STATUS_START, STATUS_REWARD):
			return False, "started"
		if not skip_repeat and qs.status == STATUS_COMPLETE and (t is None or not can_repeat(t, qs)):
			return False, "not repeatable"
	if t is None:
		return False, "no quest template (NullPointerException, caught)"
	race = t["racePermitted"]
	if race is not None and race != "PC_ALL" and race != char.race:
		return False, "race"
	if t["minlevelPermitted"] - allowed_diff - char.level > 0:
		return False, "minLevel"
	if t["maxlevelPermitted"] != 0 and char.level > t["maxlevelPermitted"]:
		return False, "maxLevel"
	classes = t["classPermitted"] or []
	if classes and char.player_class not in classes:
		return False, "class"
	# Java stops at the first failing check; the result is the conjunction, so an input the oracle lacks (the gender, a crafting skill
	# level from level 10 on) only matters when no later check fails
	unknown = None
	if t["genderPermitted"] is not None:
		if char.gender is None:
			unknown = unknown or "gender (pass --gender)"
		elif t["genderPermitted"] != char.gender:
			return False, "gender"
	if t["rank"] != 0 and world.rules.first_rank < t["rank"]:
		return False, "abyssRank"
	if not skip_xml:
		fulfilled = sum(1 for g in t["startConds"] or [] if start_condition_passes(world, char, g, warn))
		if fulfilled < required_condition_count(world, t):
			return False, "startConditions"
	inventory = t["inventoryItems"]
	for item in (inventory["inventoryItems"] or []) if inventory is not None else []:
		has, why = inventory_item_state(world, char, item["itemId"])
		if has is None:
			unknown = unknown or why
		elif not has:
			return False, "inventoryItems"
	if t["npcFactionId"] != 0:
		return False, "npcFaction (a character without an active npc faction)"
	combine = t["combineskill"]
	if combine != 0:
		if combine == -1:
			skills = [s for s in world.rules.combine_any if s not in world.rules.combine_any_gathering
			          or t["npcFactionId"] not in world.rules.combine_any_excluded_factions]
		else:
			skills = [combine]
		result: bool | None = False
		for skill in skills:
			level = char.skill_level(skill)
			if level is None:
				result = None
				break
			if level > 0 and level >= t["combineSkillpoint"]:
				if t["category"] == "TASK" and level - 40 > t["combineSkillpoint"]:
					continue
				result = True
				break
		if result is None:
			unknown = unknown or "combineSkill (the skill levels of a level 10+ character are not modelled)"
		elif not result:
			return False, "combineSkill"
	if unknown:
		return None, unknown
	return True, None


def is_acceptable_quest(world: QuestWorld, t: JObj) -> bool:
	"""AbstractQuestHandler.isAcceptableQuest (AbstractQuestHandler.java:476-484)."""
	if t["minlevelPermitted"] == ACCEPTABLE_MINLEVEL_NEVER:
		return False
	if not (t["rewards"] or []) and t["extendedRewards"] is None and t["bonus"] is None and not (t["questDrop"] or []) and \
			all(not t[f] for f in world.rules.class_selectable.values()):
		return False
	return True


# --- the rewards -------------------------------------------------------------------------------------------------------------------------


def _items(values) -> list[dict]:
	return [{"itemId": i["itemId"], "count": i["count"]} for i in values or []]


def reward_block(world: QuestWorld, t: JObj, r: JObj, index: int | None) -> dict:
	"""One <rewards> (or <extended_rewards>) as giveReward pays it (QuestService.java:220-244)."""
	rules = world.rules
	non_count = t["category"] == "NON_COUNT"
	block = {
		"index": index,
		"page": world.dialogs.reward_page(index) if index is not None else None,
		"kinah": {"template": r["kinah"], "paid": rules.quest_kinah(r["kinah"]) if r["kinah"] != 0 else 0},
		"exp": {"template": r["exp"], "paid": rules.quest_exp(r["exp"]) if r["exp"] != 0 else 0},
		"title": r["title"],
		"ap": {"template": r["abyssPoints"], "paid": r["abyssPoints"] if non_count else rules.int_rate(r["abyssPoints"], rules.ap_rate, "ap"),
		       "rated": not non_count},
		"dp": {"template": r["divinePoints"], "paid": r["divinePoints"], "startingClassGetsNone": True,
		       "note": "addDp -> PlayerCommonData.setDp (PlayerCommonData.java:463-474): nothing for a starting class; a daeva's DP is capped at "
		               "its max DP (PlayerGameStats.getMaxDp), which the oracle does not compute"},
		"gp": {"template": r["gloryPoints"], "paid": rules.int_rate(r["gloryPoints"], rules.gp_rate, "gp")},
		"extendInventory": r["extendInventory"],
		"items": _items(r["rewardItem"]),
		"selectableItems": [dict(item, action=f"SELECTED_QUEST_REWARD{n}", actionId=world.dialogs.actions[f"SELECTED_QUEST_REWARD{n}"])
		                    for n, item in enumerate(_items(r["selectableRewardItem"]), 1)],
		"unusedByJava": {"extendStigma": r["extendStigma"], "ccheck": r["collectItemChecks"], "icheck": r["inventoryItemCheck"]},
	}
	return block


def rewards_report(world: QuestWorld, quest_id: int, t: JObj, kind: str | None, reward_group: int | None) -> dict:
	"""The template's rewards and what finishQuest pays at the first completion with SELECTED_QUEST_NOREWARD (23)."""
	groups = [reward_block(world, t, r, i) for i, r in enumerate(t["rewards"] or [])]
	extended = reward_block(world, t, t["extendedRewards"], None) if t["extendedRewards"] is not None else None
	class_lists = {}
	for player_class, field_name in world.rules.class_selectable.items():
		items = _items(t[field_name])
		if items:
			class_lists[player_class] = items
	bonus = {"type": t["bonus"]["type"], "level": t["bonus"]["level"]} if t["bonus"] is not None else None
	# finishQuest with completeCount 0: the extended rewards only when reward_repeat_count is 1 (completeCount == rewardRepeatCount - 1)
	extended_paid = extended is not None and t["rewardRepeatCount"] == 1
	pays_group = (bool(t["rewards"]) or bonus is not None) and reward_group is not None
	group = groups[reward_group] if pays_group and reward_group is not None and reward_group < len(groups) else None
	order = []
	items = []
	if extended_paid:
		items += extended["items"]
	if group is not None:
		items += group["items"]
	for item in items:
		order.append({"item": item["itemId"], "count": item["count"]})
	for block in [group, extended if extended_paid else None]:
		if block is None:
			continue
		for key in ("kinah", "exp"):
			if block[key]["paid"]:
				order.append({key: block[key]["paid"]})
		if block["title"]:
			order.append({"title": block["title"]})
		for key in ("ap", "dp", "gp"):
			if block[key]["template"]:
				order.append({key: block[key]["paid"]})
		if block["extendInventory"] in (1, 2):
			order.append({"extendInventory": block["extendInventory"]})
	not_modelled = []
	if bonus is not None:
		not_modelled.append(f"the <bonus type={bonus['type']}> item: BonusService.getQuestBonus picks a random item of a random group (and "
		                    "QuestEngine.onBonusApplyEvent may veto it); it is paid after the listed items")
	if t["useClassReward"] in (1, 2) and class_lists:
		not_modelled.append("use_class_reward: SELECTED_QUEST_REWARDn pays the class list's item n - 1 instead of the group's selectable item "
		                    "(every repeat for 1, the last one for 2); SELECTED_QUEST_NOREWARD pays the item of the client's extended reward "
		                    "index - 8")
	if extended_paid and extended["selectableItems"]:
		not_modelled.append("the extended selectable item: SELECTED_QUEST_NOREWARD pays the one at the client's extended reward index - 8 "
		                    "(or - 1), which is not static data")
	if len(t["rewards"] or []) > 1 and kind not in (None, "relic_rewards"):
		not_modelled.append(f"{len(t['rewards'])} reward groups and a template handler that never sets one: validateAndFixRewardGroup "
		                    "pays group 0 and logs 'possibly rewarded the wrong reward group'")
	return {
		"groups": groups,
		"rewardGroupPaid": reward_group,
		"extended": extended,
		"rewardRepeatCount": t["rewardRepeatCount"],
		"extendedPaidAtFirstCompletion": extended_paid,
		"classSelectable": class_lists,
		"useClassReward": t["useClassReward"],
		"bonus": bonus,
		"firstCompletion": {
			"action": world.dialogs.action("SELECTED_QUEST_NOREWARD"),
			"payments": order,
			"kinah": (group["kinah"]["paid"] if group else 0) + (extended["kinah"]["paid"] if extended_paid else 0),
			"exp": (group["exp"]["paid"] if group else 0) + (extended["exp"]["paid"] if extended_paid else 0),
			# one addExp (and setExp, and possibly a level change) per giveReward: the group's, then the extended rewards'
			"expPayments": [p for p in ((group["exp"]["paid"] if group else 0), (extended["exp"]["paid"] if extended_paid else 0)) if p],
			"items": items,
			"selectable": "SELECTED_QUEST_REWARDn (n = 1..15) adds the group's selectable item n - 1",
		},
		"expNotes": [f"Rates.XP_QUEST has no cap (XP_HUNTING caps at expNeed * 0.2f); exact while the character is below level "
		             f"{world.rules.repose_level} (no repose energy) and not in world {world.rules.no_exp_world} (no exp at all); a legion "
		             f"with bonus multiplies the rate by {world.rules.legion_bonus}",
		             "BOOST_QUEST_XP_RATE sources in the item and skill data: " + (", ".join(world.quest_xp_stat_sources()) or "none"),
		             "a character that is not a daeva keeps at most the start exp of level 10 (PlayerCommonData.setExp); the gained amount on "
		             "the wire (STR_GET_EXP) is the paid value"],
		"notModelled": not_modelled,
	}


# --- the quest report --------------------------------------------------------------------------------------------------------------------


def _jaxb_values(world: QuestWorld, obj: JObj) -> dict:
	result = {}
	for (node, xml_name), b in world.jaxb.bindings(obj.cls).items():
		value = obj.values[b.field]
		if isinstance(value, JObj):
			value = _jaxb_values(world, value)
		elif isinstance(value, list):
			value = [_jaxb_values(world, v) if isinstance(v, JObj) else v for v in value]
		result[xml_name] = value
	return result


def prerequisites_report(world: QuestWorld, t: JObj) -> dict:
	groups = []
	for g in t["startConds"] or []:
		groups.append({"optional": bool(g["finished"]),
		               "finished": [{"questId": f["questId"], "reward": f["reward"]} for f in g["finished"] or []],
		               "unfinished": g["unfinished"] or [], "acquired": g["acquired"] or [], "noacquired": g["noacquired"] or [],
		               "equipped": g["equipped"] or [], "requiredTitle": g["requiredTitle"]})
	inventory = t["inventoryItems"]
	return {
		"race": t["racePermitted"],
		"minLevel": t["minlevelPermitted"],
		"maxLevel": t["maxlevelPermitted"],
		"classes": t["classPermitted"] or [],
		"gender": t["genderPermitted"],
		"abyssRank": t["rank"],
		"combineSkill": {"skill": t["combineskill"], "points": t["combineSkillpoint"]} if t["combineskill"] else None,
		"npcFaction": t["npcFactionId"],
		"inventoryItems": [{"itemId": i["itemId"], "count": i["count"]} for i in (inventory["inventoryItems"] or [])] if inventory else [],
		"startConditions": groups,
		"requiredConditionCount": required_condition_count(world, t),
		"previousQuests": sorted({f["questId"] for g in t["startConds"] or [] for f in g["finished"] or []}),
		"repeat": {"maxRepeatCount": t["maxRepeatCount"], "repeatable": t["maxRepeatCount"] > 1, "repeatCycle": t["repeatCycle"],
		           "rewardRepeatCount": t["rewardRepeatCount"]},
	}


def start_select_answer(world: QuestWorld, quest_id: int) -> dict:
	"""What the XML template of a quest answers to QUEST_SELECT at its start npc before the quest is started (the follow-up call of
	sendQuestEndDialog): {"handled": True, "page": P} (sendQuestDialog(env, P)), {"handled": False} (the handler answers false) or
	{"handled": None, "reason": ...} (not modelled)."""
	xml = world.xml[quest_id]
	q, dd = xml.obj, bool(world.template(quest_id)["dataDriven"])
	default = 4762 if dd else 1011
	tag = xml.tag
	if tag == "report_to_many" and q["startItemId"]:
		return {"handled": None, "reason": f"ReportToMany answers only with the start item {q['startItemId']} in the cube (ReportToMany.java:76-78)"}
	if tag in ("report_to", "monster_hunt", "item_collecting", "report_to_many", "kill_in_world"):
		return {"handled": True, "page": q["startDialogId"] or default}
	if tag in ("kill_spawned", "kill_in_zone", "mentor_monster_hunt"):
		return {"handled": True, "page": default}
	if tag == "crafting_rewards":
		if q["levelReward"] not in CRAFTING_LEVEL_REWARDS:  # canLearn throws, QuestEngine.onDialog catches it and answers false
			return {"handled": False, "java": "CraftingRewards.java:53, 84-90; QuestEngine.java:152-181"}
		return {"handled": True, "page": default, "assumes": "CraftSkillUpdateService.canLearnMore*CraftingSkill is true (CraftingRewards.java:53)"}
	if tag == "skill_use":
		return {"handled": True, "page": 4762}
	if tag == "work_order":
		return {"handled": True, "page": world.dialogs.pages["ASK_QUEST_ACCEPT_WINDOW"]}
	if tag == "xml_quest":
		return {"handled": None, "reason": "XmlQuest runs its on_talk_event operations first (XmlQuest.java:72-76), which are not modelled"}
	if tag in ("relic_rewards", "fountain_rewards", "item_order", "report_on_levelup"):
		# RelicRewards answers only EXCHANGE_COIN, FountainRewards USE_OBJECT and SETPRO1, ItemOrders the accept actions (the rest goes to
		# AbstractQuestHandler.onDialogEvent, which leaves QUEST_SELECT unhandled), ReportOnLevelUp nothing without a quest state
		return {"handled": False, "java": "RelicRewards.java:47-60, FountainRewards.java:45-68, ItemOrders.java:63-77, ReportOnLevelUp.java:37-49"}
	raise OracleError(f"quest {quest_id}: no QUEST_SELECT model for the template kind {tag}")


def auto_started_quests(world: QuestWorld, char: Character, levels: list[int]) -> tuple[list[int], list[str]]:
	"""The report_on_levelup quests the character holds in REWARD without being told: ReportOnLevelUp starts its quest in REWARD at enter
	world and at every level change when it is not in the list and checkStartConditions passes (ReportOnLevelUp.java:52-65, QuestEngine
	onLevelChanged :230-244). (quests started at one of `levels`, the ones whose start depends on what the oracle does not know)."""
	started, unknown = [], []
	for quest_id in world.registrations():
		if world.xml[quest_id].tag != "report_on_levelup" or quest_id in char.quests:
			continue
		for level in levels:
			passes, reason = check_start_conditions(world, replace(char, level=level), quest_id)
			if passes is None:
				unknown.append(f"{quest_id} at level {level}: {reason}")
				break
			if passes:
				started.append(quest_id)
				break
	return started, unknown


def follow_up_report(world: QuestWorld, quest_id: int, char: Character, end_npcs: list[int], finishing_kind: str,
                     levels: list[int]) -> list[dict]:
	"""sendQuestEndDialog after finishQuest (AbstractQuestHandler.java:413-474) over the XML-only registry, for `char` (this quest COMPLETE,
	at its level after the reward; `levels` are the levels it had since entering the world: before the reward and after each addExp).
	First the npc's onTalkEvent quests in REWARD answer USE_OBJECT (:429-437): a report_on_levelup quest ending at the npc that the character
	would hold in REWARD makes the window None (not modelled). Then the npc's onQuestStart set in HashSet order: the first quest passing
	checkStartConditions that names this quest in a <finished> start condition and isAcceptableQuest gets QUEST_SELECT (start_select_answer;
	when its handler answers false, sendQuestEndDialog and the finishing handler answer false and DialogService sends the next page,
	SM_DIALOG_WINDOW(npc, the finishing action's id, this quest), DialogService.java:282-291 - except RelicRewards, which answers true and
	sends nothing); else page 10 when any quest of the set passes, else the window closes (page 0)."""
	index = world.xml_start_npcs()
	talk_index = world.xml_talk_npcs()
	auto, auto_unknown = auto_started_quests(world, char, levels)
	char = replace(char, quests=dict(char.quests))
	for other in auto:
		char.quests[other] = QuestStateInfo(STATUS_REWARD, 0, None)
	result = []
	for npc in end_npcs:
		registered = index.get(npc, [])
		order = hash_iteration_order(registered, initial_capacity=0)
		candidates = []
		chosen = None
		any_new = False
		uncertain = []
		blocked = []
		for candidate in order:
			passes, reason = check_start_conditions(world, char, candidate)
			t = world.template(candidate)
			names = any(f["questId"] == quest_id for g in t["startConds"] or [] for f in g["finished"] or [])
			acceptable = is_acceptable_quest(world, t)
			candidates.append({"questId": candidate, "passes": passes, "failed": reason, "namesThisQuest": names, "acceptable": acceptable})
			if passes is None:
				uncertain.append(candidate)
				if names and acceptable and chosen is None:
					blocked.append(candidate)  # it would be chosen if it passed
				continue
			if passes:
				any_new = True
				if names and acceptable and chosen is None and not blocked:
					chosen = candidate
		java_here = sorted(q for q, h in world.java.items() if npc in h.start_npcs)
		in_reward = [q for q in talk_index.get(npc, []) if q in auto]
		answer = None
		why = None
		if in_reward:
			window = None
			why = (f"report_on_levelup {in_reward} ends at this npc and the character holds it in REWARD: the onTalkEvent loop shows its "
			       "reward dialog (not modelled; pass --completed if the character finished it)")
		elif auto_unknown:
			window = None
			why = f"whether a report_on_levelup quest is in REWARD: {auto_unknown}"
		elif chosen is not None:
			answer = start_select_answer(world, chosen)
			if answer["handled"] is True:
				window = {"page": answer["page"], "questId": chosen}
			elif answer["handled"] is False:
				window = ({"page": None, "questId": None, "sent": False} if finishing_kind == "relic_rewards" else
				          {"page": world.dialogs.actions["SELECTED_QUEST_NOREWARD"], "questId": quest_id, "nextPage": True})
			else:
				window = None
				why = answer["reason"]
		elif blocked:
			window = None
			why = f"candidates {blocked} name this quest and their start check is not known"
		elif any_new:
			window = {"page": 10, "questId": 0}
		elif uncertain:
			window = None
			why = f"page 10 or 0: the start check of {uncertain} is not known"
		else:
			window = {"page": 0, "questId": 0}
		result.append({"npc": npc, "onQuestStartOrder": order, "candidates": candidates, "followUp": chosen, "followUpAnswer": answer,
		               "window": window, "windowUnknownBecause": why, "uncertainBecause": uncertain, "autoStartedInReward": auto,
		               "javaStartQuestsAtNpc": java_here,
		               "nextPageNote": "a window with nextPage is DialogService's answer to the finishing action: page 23 for "
		                               "SELECTED_QUEST_NOREWARD, 8..22 for SELECTED_QUEST_REWARD1..15"})
	return result


def _npc_infos(world: QuestWorld, npc_ids: set[int]) -> dict:
	"""Name, level and ai of each npc template, and its regular spawn spots per map (the <spawn>s directly under <spawn_map>)."""
	templates, spawns = world.npc_index()
	infos = {}
	for npc_id in sorted(npc_ids):
		info = dict(templates.get(npc_id, {"name": None, "level": None, "ai": None, "template": False}))
		info["spawnMaps"] = {str(m): n for m, n in sorted(spawns.get(npc_id, {}).items())}
		infos[str(npc_id)] = info
	return infos


def quest_report(world: QuestWorld, quest_id: int, race: str | None = None, player_class: str = "WARRIOR", level: int | None = None,
                 gender: str | None = None, completed=(), inventory=(), exp: int | None = None) -> dict:
	"""One quest. The follow-up character has `level` and `exp` before the reward (default: the quest's minimum level, at its start exp; or
	the level of `exp`), the quest list `completed` plus this quest and the cube `inventory` states; the follow-up is evaluated at its level
	after the reward."""
	t = world.template(quest_id)
	if t is None:
		raise OracleError(f"quest {quest_id} has no quest_data template")
	registry = world.registry_of(quest_id)
	xml = world.xml.get(quest_id)
	java = world.java.get(quest_id)
	handler = {"registry": registry, "inCppRegistry": xml is not None,
	           "xml": None, "java": {"class": java.class_name, "file": java.file, "startNpcs": java.start_npcs,
	                                  "unresolvedStartNpcs": java.unresolved} if java else None}
	not_modelled: list[str] = []
	steps = None
	targets = None
	reg = None
	reward_group = 0 if t["rewards"] else None
	if xml is not None:
		regs = world.registrations()
		reg = regs[quest_id]
		ignored_by_handler = sorted(xml_name for (node, xml_name), b in world.jaxb.bindings(xml.data_class).items()
		                            if b.field in xml.obj.present and b.field not in USED_FIELDS[xml.tag])
		handler["xml"] = {"kind": xml.tag, "dataClass": xml.data_class, "templateClass": world.template_classes[xml.tag], "file": xml.file,
		                  "parameters": _jaxb_values(world, xml.obj),
		                  "present": sorted(xml_name for (node, xml_name), b in world.jaxb.bindings(xml.data_class).items()
		                                    if b.field in xml.obj.present),
		                  "ignoredByJaxb": xml.obj.ignored, "ignoredByHandler": ignored_by_handler,
		                  "registrationOrderIndex": list(regs).index(quest_id)}
		flow = Flow(world, quest_id, t)
		targets = KINDS[xml.tag](flow, xml.obj, reg)
		not_modelled += flow.not_modelled
		if xml.tag != "xml_quest":
			steps = flow.steps
		if flow.reward_group is not None:
			reward_group = flow.reward_group
	elif java is not None:
		not_modelled.append(f"steps: quest {quest_id} is handled by the Java class {java.class_name} ({java.file}), phase 6; only the "
		                    "template data and QuestService's rewards are modelled, with validateAndFixRewardGroup's group 0 (a Java handler "
		                    "may set another)")
	else:
		not_modelled.append("no handler: neither an XML template nor a Java handler, so the quest never starts")
	if registry == "java" and xml is not None:
		not_modelled.append("both an XML template and a Java handler: the Java server registers the Java handler (putIfAbsent)")
	race = race or (t["racePermitted"] if t["racePermitted"] in RACES else None)
	rewards = rewards_report(world, quest_id, t, xml.tag if xml else None, reward_group)
	follow = None
	character = None
	char = None
	if steps is not None and race is not None:
		if level is not None:
			char_level = level
		elif exp is not None:
			char_level = max(i for i, start in enumerate(world.experience(), 1) if exp >= start) if exp >= 0 else 0
		else:
			char_level = max(1, t["minlevelPermitted"])
		try:
			char = make_character(world, race, player_class, char_level, gender)
		except OracleError as e:
			if level is not None or exp is not None:
				raise
			not_modelled.append(f"followUp: no default character ({e}); pass --class and --level")
	if char is not None:
		apply_quest_list(world, char, completed)
		apply_inventory(char, inventory)
		if quest_id in char.quests:
			raise OracleError(f"quest {quest_id} is in --completed: the follow-up is for its first completion")
		char.quests[quest_id] = QuestStateInfo(STATUS_COMPLETE, 1, reward_group, time_based_pending=t["repeatCycle"] is not None)
		# giveReward -> addExp -> setExp per reward block (PlayerCommonData.java:167-222, 273-291): the follow-up runs after the level changes
		exp_before = world.experience()[char.level - 1] if exp is None else exp
		exp_now, level_now = exp_state(world, char, exp_before)
		if exp_now != exp_before or level_now != char.level:
			raise OracleError(f"--exp {exp_before} is not the exp of a level {char.level} {player_class} (setExp gives exp {exp_now}, level "
			                  f"{level_now})")
		levels = [char.level]
		for paid in rewards["firstCompletion"]["expPayments"]:
			exp_now, level_now = exp_state(world, char, exp_now + paid)
			if level_now != levels[-1]:
				levels.append(level_now)
		char.level = levels[-1]
		character = dict(char.as_json(), levelBeforeReward=levels[0], expBeforeReward=exp_before, expAfterReward=exp_now,
		                 levelsSinceEnterWorld=levels)
		finish_steps = [s for s in steps if s.get("effect", {}).get("finishQuest") and s["effect"].get("followUp")]
		end_npcs = []
		for s in finish_steps:
			for npc in s["at"].get("npcs", []):
				if npc not in end_npcs:
					end_npcs.append(npc)
		follow = follow_up_report(world, quest_id, char, end_npcs, xml.tag, levels)
	elif steps is not None and race is None:
		not_modelled.append("followUp: the quest is not race specific, pass --race to evaluate the follow-up")
	npc_ids = set()
	if reg is not None:
		npc_ids |= set(reg.quest_start) | set(reg.talk) | set(reg.kill) | set(reg.aggro)
	if java is not None:
		npc_ids |= set(java.start_npcs)
	return {
		"format": "aion-m5d-quest",
		"version": 1,
		"questId": quest_id,
		"name": t["name"],
		"nameId": t["nameId"],
		"questZone": t.element.get("quest_zone"),
		"category": t["category"],
		"extraCategory": t["extraCategory"],
		"dataDriven": t["dataDriven"],
		"flags": {"cannotShare": t["cannotShare"], "cannotGiveup": t["cannotGiveup"], "canReport": t["canReport"], "timer": t["timer"],
		          "restricted": t["restricted"], "mentorType": t["mentorType"], "target": t["target"]},
		"ignoredByJaxb": t.ignored,
		"handler": handler,
		"registration": reg.as_json() if reg is not None else None,
		"prerequisites": prerequisites_report(world, t),
		"start": {
			"npcs": reg.quest_start if reg is not None else (java.start_npcs if java else []),
			"items": reg.quest_items if reg is not None else [],
			"onEnterWorld": reg.enter_world if reg else None,
			"onLevelChanged": reg.level_changed if reg else None,
			"onEnterZone": reg.enter_zone if reg else None,
			"onAtDistance": reg.at_distance if reg else [],
			"onAddAggroList": reg.aggro if reg else [],
			"workItems": world.work_items(t),
		},
		"targets": targets,
		"collect": {"items": world.collect_items(t), "startCheck": t["collectItems"]["startCheck"] if t["collectItems"] is not None else None,
		            "drops": world.drops(t)},
		"kills": [{"npcIds": k["npcIds"], "count": k["kill"], "var": k["var"], "seq": k["seq"], "step": k["step"]} for k in t["questKill"] or []],
		"steps": steps,
		"rewards": rewards,
		"followUp": {"character": character, "atEndNpcs": follow} if follow is not None else None,
		"npcs": _npc_infos(world, npc_ids),
		"rates": world.rules.as_json(),
		"dialogTables": DIALOG_TABLES,
		"notModelled": not_modelled,
	}


DIALOG_TABLES = {
	"AbstractQuestHandler.onDialogEvent": "AbstractQuestHandler.java:93-117: ASK_QUEST_ACCEPT (1007) page 4; QUEST_ACCEPT_1 (1002) page 1003 "
	                                      "without a start; QUEST_REFUSE (30) / QUEST_REFUSE_SIMPLE (20001) / QUEST_REFUSE_1 (1003) page 1004, "
	                                      "QUEST_REFUSE_2 1005, QUEST_REFUSE_3 1006, QUEST_REFUSE_4 1007 (at an npc, else the window closes); "
	                                      "FINISH_DIALOG (1008) handled without a packet; anything else unhandled",
	"sendQuestStartDialog": "AbstractQuestHandler.java:373-398: ASK_QUEST_ACCEPT page 4; QUEST_ACCEPT / QUEST_ACCEPT_1 / QUEST_ACCEPT_SIMPLE "
	                        "start the quest; QUEST_REFUSE_1 / QUEST_REFUSE_2 page 1004; QUEST_REFUSE_SIMPLE closes; FINISH_DIALOG page 10 "
	                        "(quest 0); anything else unhandled",
}


# --- the map report ----------------------------------------------------------------------------------------------------------------------


def _map_template(data: StaticData, map_id: int):
	for element in data.children("world_maps", "map"):
		if java_int(element.get("id"), "map id") == map_id:
			return element
	raise OracleError(f"map {map_id} is not in world_maps")


def spawned_npcs(data: StaticData, map_id: int, clock: GameClock, templates) -> dict[int, bool | None]:
	"""SpawnEngine.spawnInstance for difficulty 0 (SpawnEngine.java:135-189): npc id -> True when at least one Npc of it is spawned at
	startup, None when that depends on a game time the caller did not give; npc ids without a template (VisibleObjectSpawner.spawnNpc
	spawns nothing) and gatherables (not Npcs) are left out."""
	present: dict[int, bool | None] = {}
	for group in load_groups(data, map_id):
		if group.difficult_id != 0 or group.handler is not None:
			continue
		if is_gatherable(group.npc_id) or group.npc_id not in templates:
			continue
		in_time = True if group.temporary is None else group.temporary.is_in_spawn_time(clock)
		if in_time is False:
			continue
		if 0 < group.pool < len(group.spots):
			state = in_time
		else:
			spots = [True if s.temporary is None else s.temporary.is_in_spawn_time(clock) for s in group.spots]
			if any(s is True for s in spots):
				state = in_time
			elif all(s is False for s in spots):
				continue
			else:
				state = None
		previous = present.get(group.npc_id, False)
		present[group.npc_id] = True if previous is True or state is True else None
	return present


def _service_spawns(world: QuestWorld, map_id: int) -> dict[str, list[int]]:
	"""The npc ids of the nested siege/base/rift/vortex/mercenary/ahserion spawns of the map, which their services spawn (not spawnAll)."""
	result: dict[str, list[int]] = {}
	for spawn_map in world.spawn_maps():
		if java_int(spawn_map.get("map_id"), "spawn_map map_id") != map_id:
			continue
		for child in spawn_map:
			if child.tag == "spawn":
				continue
			for spawn in child.iter("spawn"):
				ids = result.setdefault(child.tag, [])
				npc_id = java_int(spawn.get("npc_id"), "spawn npc_id")
				if npc_id not in ids:
					ids.append(npc_id)
	return result


def wire_order(ids: list[int], grey: set[int]) -> dict:
	"""SM_NEARBY_QUESTS.writeImpl writes the entries of updateNearbyQuests' new HashMap<>() keyed by quest id, in its iteration order: the
	buckets in order, and inside a bucket the insertion order - the iteration order of the map instance's ConcurrentHashMap key set, which
	the oracle does not model. The written values (bit 17 on the grey ones), grouped by bucket; `exact` when every bucket holds one id."""
	groups = hash_bucket_groups(ids)
	if groups is None:
		return {"buckets": None, "exact": False}
	return {"buckets": [[q | (1 << 17) if q in grey else q for q in g] for g in groups], "exact": all(len(g) == 1 for g in groups)}


def map_report(world: QuestWorld, map_id: int, race: str | None = None, player_class: str = "WARRIOR", level: int = 1, gender: str | None = None,
               clock: GameClock = GameClock(), completed=(), started=(), inventory=()) -> dict:
	world_map = _map_template(world.data, map_id)
	if world_map.get("instance") is not None and world_map.get("instance") not in ("false", "0"):
		raise OracleError(f"map {map_id} is an instance: SpawnEngine.spawnAll does not spawn it at startup, its npcs come with the instance")
	world_type = world_map.get("world_type", "NONE")
	if race is None:
		if world_type not in RACE_OF_WORLD_TYPE:
			raise OracleError(f"map {map_id} has world_type {world_type}: pass --race")
		race = RACE_OF_WORLD_TYPE[world_type]
	char = make_character(world, race, player_class, level, gender)
	apply_quest_list(world, char, completed, started)
	apply_inventory(char, inventory)
	present = spawned_npcs(world.data, map_id, clock, world.npc_index()[0])
	xml_index = world.xml_start_npcs()
	java_index = world.java_start_npcs()
	xml_ids: dict[int, list[int]] = {}
	java_ids: dict[int, list[int]] = {}
	uncertain: dict[int, list[int]] = {}
	for npc, state in sorted(present.items()):
		for quest_id in xml_index.get(npc, []):
			(xml_ids if state is True else uncertain).setdefault(quest_id, []).append(npc)
		for quest_id in java_index.get(npc, []):
			if quest_id not in world.xml:
				(java_ids if state is True else uncertain).setdefault(quest_id, []).append(npc)
	rows = []
	undecided = []
	for quest_id in sorted(set(xml_ids) | set(java_ids) | set(uncertain)):
		t = world.template(quest_id)
		passes, reason = check_start_conditions(world, char, quest_id, allowed_diff=2)
		diff = 99 if t is None else t["minlevelPermitted"] - char.level
		registry = "xml" if quest_id in world.xml and quest_id not in world.java else world.registry_of(quest_id)
		row = {"questId": quest_id, "name": t["name"] if t else None, "registry": registry,
		       "kind": world.xml[quest_id].tag if quest_id in world.xml else None,
		       "javaFile": world.java[quest_id].file if quest_id in world.java else None,
		       "startNpcs": xml_ids.get(quest_id) or java_ids.get(quest_id) or uncertain.get(quest_id),
		       "spawnCertain": quest_id not in uncertain, "minLevel": t["minlevelPermitted"] if t else None,
		       "nearby": passes, "failed": reason, "levelDiff": diff, "grey": passes is True and diff > 0}
		if passes is None and quest_id not in uncertain:
			undecided.append(f"{quest_id}: {reason}")
		rows.append(row)
	if undecided:
		raise OracleError(f"the nearby set depends on what the oracle was not given: {undecided}")

	def listed(registry_filter) -> list[int]:
		return [r["questId"] for r in rows if r["nearby"] is True and r["spawnCertain"] and registry_filter(r)]

	xml_only = listed(lambda r: r["registry"] == "xml")
	with_java = listed(lambda r: True)
	grey = {r["questId"] for r in rows if r["grey"]}
	auto = []
	for quest_id, reg in world.registrations().items():
		if world.xml[quest_id].tag == "report_on_levelup":
			passes, reason = check_start_conditions(world, char, quest_id, allowed_diff=0)
			auto.append({"questId": quest_id, "startsInRewardState": passes, "failed": reason})
	events = []
	events_dir = world.data.dir / "events" / "timed_events"
	for path in sorted(events_dir.glob("*.xml")) if events_dir.is_dir() else []:
		try:
			root = ET.parse(path).getroot()
		except ET.ParseError as e:
			raise OracleError(f"{path}: {e}") from e
		for event in root.iter("event"):
			npcs = sorted({java_int(s.get("npc_id"), "event spawn npc_id") for m in event.iter("spawn_map") if m.get("map_id") == str(map_id)
			               for s in m.iter("spawn") if s.get("npc_id") is not None})
			if npcs:
				quests = sorted({q for npc in npcs for q in xml_index.get(npc, []) + java_index.get(npc, [])})
				events.append({"file": path.name, "event": event.get("name"), "start": event.get("start"), "end": event.get("end"),
				               "npcs": len(npcs), "startQuests": quests})
	event_quests = sorted({q for e in events for q in e["startQuests"]})
	service = _service_spawns(world, map_id)
	for spawn_map in world.data.children("town_spawns_data", "spawn_map"):  # TownService spawns these per town level, not spawnAll
		if java_int(spawn_map.get("map_id"), "town spawn_map map_id") == map_id:
			ids = service.setdefault("town_spawn", [])
			for spawn in spawn_map.iter("spawn"):
				npc_id = java_int(spawn.get("npc_id"), "town spawn npc_id")
				if npc_id not in ids:
					ids.append(npc_id)
	service_quests = sorted({q for ids in service.values() for npc in ids for q in xml_index.get(npc, []) + java_index.get(npc, [])})
	java_unresolved = sorted(q for q, h in world.java.items() if h.unresolved)
	return {
		"format": "aion-m5d-quests",
		"version": 1,
		"map": map_id,
		"worldType": world_type,
		"character": char.as_json(),
		"gameTime": {"hour": clock.hour, "day": clock.day, "month": clock.month, "weekday": clock.weekday},
		"registry": world.census(),
		"spawnedStartNpcs": {str(npc): {"spawned": state, "xml": xml_index.get(npc, []), "java": java_index.get(npc, [])}
		                     for npc, state in sorted(present.items()) if npc in xml_index or npc in java_index},
		"quests": rows,
		"nearby": {
			"xmlOnly": xml_only,
			"xmlOnlyGrey": [q for q in xml_only if q in grey],
			"xmlOnlyWire": sorted(q | (1 << 17) if q in grey else q for q in xml_only),
			"withJava": with_java,
			"withJavaGrey": [q for q in with_java if q in grey],
			"xmlOnlyWireOrder": wire_order(xml_only, grey),
			"withJavaWireOrder": wire_order(with_java, grey),
		},
		"uncertainSpawns": sorted(uncertain),
		"reportOnLevelUp": auto,
		"notModelled": {
			"eventSpawns": events,
			"eventSpawnQuests": event_quests,
			"serviceSpawns": service,
			"serviceSpawnQuests": service_quests,
			"javaUnresolvedStartNpcs": java_unresolved,
			"wireOrder": "SM_NEARBY_QUESTS writes a HashMap<Integer, Integer> in bucket order; inside a bucket the order is the map "
			             "instance's ConcurrentHashMap key set order, which is not modelled: nearby.*WireOrder gives the buckets, a gate can "
			             "assert the order where every bucket holds one id (exact) and otherwise the order of the buckets",
			"runtimeSpawns": "npcs spawned after startup (house npcs of HousingService.spawnHouses, quest and AI spawns, temporary groups "
			                 "reaching their spawn time later) add their quests to the instance's questIds for good",
		},
		# the XML-only set is exact when no start npc's presence depends on the game time and no quest giver comes from a timed event (the
		# M5 gate profiles disable every event) or a service spawn (siege, base, rift, vortex, mercenary, ahserion)
		"exact": not uncertain and not event_quests and not service_quests,
	}
