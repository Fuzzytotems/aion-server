"""m5f-travel --census (m5f-plan.md §2.8 W-14, W-21; §12): which effect classes the M5f paths reach that the C++ tree has not ported.

The data side is the Java's, the "ported" side the C++ tree's (the one place this oracle reads it):
- W-14: every advanced class seeded as a level-16 Daeva learns at enter world what SkillLearnService.learnNewSkills(2..16) teaches (the m5e
  progression model: the class's autolearn rows <= 16 and its starting class's rows below 10, m5e/progression.py); its PASSIVE skills are applied
  by activatePassiveSkillEffects at enter world, so their effect classes must be ported or the character cannot enter (m5b2-plan.md D11);
- W-21: the npcs of spawns/Instances/300200000_Haramel.xml (every spawn group of the map), their npc_skills lists (NpcSkillData: the first list
  naming an npc wins, the m5b2 loader), the skills' <effects> (the last skill_template of an id, SkillData.afterUnmarshal) and the class
  Effects.java's @XmlElements binds each tag to (m5b2's JavaSkillRules).
A class is ported when its C++ source skillengine/effect/<Class>.cpp exists without AION_UNPORTED(, or when it has no .cpp but a header in src
or generated/ (a generated data-only class such as SpellAttackInstantEffect, §12). A class with neither is "missing" and listed as unported.
"""

from __future__ import annotations

from pathlib import Path

from staticdata_oracle import OracleError

from m5a.data import StaticData, java_int

RACES = ("ELYOS", "ASMODIANS")
HARAMEL = 300200000


class CppEffects:
	"""the C++ tree's effect classes: src/aion/gameserver/skillengine/effect and generated/aion/gameserver/skillengine/effect"""

	def __init__(self, cpp_dir: Path):
		cpp_dir = Path(cpp_dir)
		if (cpp_dir / "src" / "aion").is_dir():
			root = cpp_dir
			self.src = cpp_dir / "src" / "aion" / "gameserver" / "skillengine" / "effect"
		elif (cpp_dir / "aion").is_dir():
			root = cpp_dir.parent
			self.src = cpp_dir / "aion" / "gameserver" / "skillengine" / "effect"
		else:
			raise OracleError(f"--cpp-src {cpp_dir}: neither DIR/src/aion nor DIR/aion exists (pass cpp/game-server or cpp/game-server/src)")
		if not self.src.is_dir():
			raise OracleError(f"{self.src} is not a directory")
		self.generated = root / "generated" / "aion" / "gameserver" / "skillengine" / "effect"

	def state(self, cls: str) -> str:
		"""'ported', 'unported', 'partial' (ported with an AION_PARTIAL site) or 'missing'"""
		cpp = self.src / f"{cls}.cpp"
		if cpp.is_file():
			text = cpp.read_text(encoding="utf-8", errors="replace")
			if "AION_UNPORTED(" in text:
				return "unported"
			return "partial" if "AION_PARTIAL(" in text else "ported"
		if (self.src / f"{cls}.h").is_file() or (self.generated / f"{cls}.h").is_file():
			return "ported"
		return "missing"

	def unported_sites(self, cls: str) -> int:
		cpp = self.src / f"{cls}.cpp"
		return cpp.read_text(encoding="utf-8", errors="replace").count("AION_UNPORTED(") if cpp.is_file() else 0


def _skill_classes(templates: dict, skill_rules, skill_id: int) -> tuple[list[str], list[str]]:
	"""(bound classes in order, unbound tags) of a skill's <effects>"""
	template = templates.get(skill_id)
	if template is None:
		return [], []
	classes, unbound = [], []
	for tag, *_ in template.effects:
		cls = skill_rules.effect_classes.get(tag)
		if cls is None:
			unbound.append(tag)  # JAXB drops an element @XmlElements does not name
		else:
			classes.append(cls)
	return classes, unbound


def _load_templates(data: StaticData, wanted: set[int]) -> dict:
	from m5b2.skills import SkillTemplateInfo
	templates = {}
	for element in data.stream("skill_data", "skill_template"):
		skill_id = java_int(element.get("skill_id"), "skill_template skill_id")
		if skill_id in wanted:
			templates[skill_id] = SkillTemplateInfo.of(element)  # the last template of an id is kept
	return templates


def census_report(data: StaticData, java_src: Path, handlers: Path, cpp_dir: Path, spawn_groups: dict) -> dict:
	from m5a.creation import JavaEnums
	from m5b2.skills import JavaSkillRules, npc_skill_lists
	from m5e.progression import Character, Progression, ProgressionData, ProgressionRules, _class_ids

	cpp = CppEffects(cpp_dir)
	skill_rules = JavaSkillRules.read(java_src)
	enums = JavaEnums(java_src)
	enums.class_ids = _class_ids(java_src)
	pdata = ProgressionData(data)
	progression = Progression(pdata, ProgressionRules.read(java_src, handlers), enums)

	# W-14: the passives of a level-16 Daeva seed of every advanced class (both races; the union)
	passives_by_class: dict[str, set[int]] = {}
	for cls in sorted(enums.classes):
		if progression.is_starting(cls) or cls not in enums.starting_classes or enums.starting_classes[cls] == cls:
			continue
		ids: set[int] = set()
		for race in RACES:
			ch = Character(race, cls, 1, daeva=True)
			events: list[dict] = []
			progression.learn_new_skills(ch, 1, 16, events)  # creation (1) and the seed's onLevelChange(1, 16) together
			ch.level = 16
			ids |= {sid for sid in ch.skills if pdata.skills.get(sid) is not None and pdata.skills[sid].activation == "PASSIVE"}
		passives_by_class[cls] = ids

	# W-21: Haramel's npcs and their skills
	groups = spawn_groups.get(HARAMEL)
	if groups is None:
		raise OracleError(f"no spawn_map for {HARAMEL} (spawns/Instances/300200000_Haramel.xml)")
	haramel_npcs = sorted({g.npc_id for g in groups})
	npc_lists = npc_skill_lists(data, set(haramel_npcs))
	haramel_skills = {npc: sorted({row["skillId"] for row in rows}) for npc, rows in npc_lists.items()}

	wanted = set().union(*passives_by_class.values()) | {s for skills in haramel_skills.values() for s in skills}
	templates = _load_templates(data, wanted)

	states: dict[str, str] = {}

	def state(cls: str) -> str:
		if cls not in states:
			states[cls] = cpp.state(cls)
		return states[cls]

	def chain_unported(classes) -> list[str]:
		bases = set()
		for cls in classes:
			try:
				chain = skill_rules.class_chain(cls)[1:]
			except OracleError:
				continue
			bases |= {b for b in chain if state(b) in ("unported", "missing")}
		return sorted(bases)

	w14 = {}
	for cls, ids in sorted(passives_by_class.items()):
		classes: set[str] = set()
		unbound: set[str] = set()
		by_skill = {}
		for sid in sorted(ids):
			c, u = _skill_classes(templates, skill_rules, sid)
			classes |= set(c)
			unbound |= set(u)
			bad = sorted({x for x in c if state(x) in ("unported", "missing")})
			if bad:
				by_skill[str(sid)] = bad
		w14[cls] = {"passives": sorted(ids), "effectClasses": sorted(classes),
		            "unported": sorted(c for c in classes if state(c) in ("unported", "missing")),
		            "partial": sorted(c for c in classes if state(c) == "partial"),
		            "unportedBases": chain_unported(classes), "unboundTags": sorted(unbound), "unportedBySkill": by_skill,
		            "missingTemplates": sorted(s for s in ids if s not in templates)}

	all_classes: set[str] = set()
	unported_by: dict[str, list[dict]] = {}
	unbound_all: set[str] = set()
	for npc, skills in sorted(haramel_skills.items()):
		for sid in skills:
			c, u = _skill_classes(templates, skill_rules, sid)
			all_classes |= set(c)
			unbound_all |= set(u)
			for cls in sorted(set(c)):
				if state(cls) in ("unported", "missing"):
					unported_by.setdefault(cls, []).append({"npcId": npc, "skillId": sid})
	haramel = {"world": HARAMEL, "npcIds": haramel_npcs, "npcSkills": {str(k): v for k, v in sorted(haramel_skills.items())},
	           "npcsWithoutSkills": [n for n in haramel_npcs if n not in haramel_skills],
	           "effectClasses": sorted(all_classes),
	           "unportedEffectClasses": sorted(unported_by), "unportedBy": unported_by,
	           "partialEffectClasses": sorted(c for c in all_classes if state(c) == "partial"),
	           "unportedBases": chain_unported(all_classes), "unboundTags": sorted(unbound_all),
	           "missingTemplates": sorted({s for v in haramel_skills.values() for s in v if s not in templates})}
	return {"cppEffectDir": str(cpp.src).replace("\\", "/"),
	        "unportedSites": {cls: cpp.unported_sites(cls) for cls, st in sorted(states.items()) if st == "unported"},
	        "w14": w14, "haramel": haramel}
