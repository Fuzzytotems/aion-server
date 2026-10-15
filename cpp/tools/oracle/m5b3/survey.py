"""m5b3-item --survey: the effect classes the items, godstones and skill materials of m5b3-plan.md §2.5-§2.6 reach, re-derived from the data
(G-01: "re-derives every number of §2.4-§2.6"; §2.4 is m5b3-drops --survey, the single items are m5b3-item --item).

Three scopes, each a set of skills whose <effects> children give the leaf classes (Effects.java's @XmlElements: tag -> class, the m5b2 oracle's
JavaSkillRules), with the LAST <skill_template> of an id kept (SkillData.afterUnmarshal, SkillData.java:33-39) and the superclasses up to
EffectTemplate added for `classChain` (the "closed under extends" of the plan):
- `skilluse`: the items of the starter inventories (player_initial_data.xml, every class's <player_data><items>, PlayerInitialData) and the
  items droppable on the surveyed maps (m5b3-drops --survey's `distinctDroppableItems`: the rule and custom drop candidates and the quest drop
  items, for the map's default killer), those with a <skilluse> action (SkillUseAction), the skills of those actions. Per leaf class:
  `items` (the items whose skill has an effect of the class), `effects` (the effects of the class over those items' skills, one skill per
  item - m5b3-plan.md §2.5's table counts these: StatupEffect's 20 are 12 items) and `skills`;
- `godstones`: every <item_template> with a <godstone> (ItemData: a later template of an id replaces the earlier one), the skills of their
  `skillid` (CreatureController.applyGodStoneEffect -> GodStone.tryActivate -> getSkill). Per leaf class `skills` and `items`; per surveyed
  map the godstones droppable there and their classes;
- `materials`: the <skill>s of every <material> of material_templates.xml (MaterialData; AbstractMaterialSkillActor applies them). Per leaf
  class `skills`. Which materials a map places is m5b3-material's.

Whether a class is ported is the C++ tree's state, not the data's: the census answers it (tools/porting/census.py), not this oracle.
"""

from __future__ import annotations

from pathlib import Path

from staticdata_oracle import OracleError

from m5a.data import StaticData, java_int
from m5b2.skills import JavaSkillRules, SkillTemplateInfo
from m5b3.items import JavaItemRules, load_skill_templates_counted
from m5b3.materials import material_templates


def starter_items(data: StaticData) -> set[int]:
	"""player_initial_data.xml: the item ids of every class's <player_data><items> (PlayerInitialData.PlayerCreationData.getItems)"""
	result: set[int] = set()
	for element in data.children("player_initial_data", "player_data"):
		items = element.find("items")
		if items is None:
			continue
		for item in items:
			if item.tag == "item":
				result.add(java_int(item.get("id"), f"player_initial_data {element.get('class')} item id"))
	return result


def _effect_classes(template: SkillTemplateInfo, skill_rules: JavaSkillRules) -> list[str]:
	"""the class of every <effects> child, in order (one per effect: a class twice is two effects)"""
	classes = []
	for tag, *_ in template.effects:
		cls = skill_rules.effect_classes.get(tag)
		if cls is None:
			raise OracleError(f"skill {template.skill_id}: effect <{tag}> is not bound by Effects.java")
		classes.append(cls)
	return classes


def _class_chain(classes, skill_rules: JavaSkillRules) -> list[str]:
	return sorted({c for cls in classes for c in skill_rules.class_chain(cls)})


def item_survey(data: StaticData, java_src: Path, droppable_by_map: dict[int, set[int]], killers: dict[int, dict] | None = None) -> dict:
	"""The three scopes for the starter items and the given droppable item ids per map (droppable_items computes them from the drop data)."""
	rules = JavaItemRules.read(java_src)
	skill_rules = JavaSkillRules.read(java_src)
	starter = starter_items(data)
	droppable = set().union(*droppable_by_map.values()) if droppable_by_map else set()
	scope = starter | droppable

	# one pass over the item templates: the scope's skilluse actions and every godstone (a later template of an id replaces the earlier one)
	names: dict[int, str | None] = {}
	skilluse: dict[int, list[int]] = {}
	godstones: dict[int, int] = {}
	for element in data.stream("item_templates", "item_template"):
		item_id = java_int(element.get("id"), "item_template id")
		godstone = element.find("godstone")
		in_scope = item_id in scope
		if godstone is None and not in_scope and item_id not in godstones:
			continue
		names[item_id] = element.get("name")
		godstones.pop(item_id, None)
		if godstone is not None:
			godstones[item_id] = java_int(godstone.get("skillid"), f"item {item_id} godstone skillid")
		if in_scope:
			skilluse.pop(item_id, None)
			actions = element.find("actions")
			skills = []
			for action in actions if actions is not None else []:
				if action.tag not in rules.action_classes:
					raise OracleError(f"item {item_id}: action <{action.tag}> is not bound by ItemActions.java")
				if action.tag == "skilluse":
					skills.append(java_int(action.get("skillid"), f"item {item_id} skilluse skillid"))
			if skills:
				skilluse[item_id] = skills
	missing = sorted(scope - names.keys())
	if missing:
		raise OracleError(f"no item_template for item id(s) {missing}")

	materials = material_templates(data)
	material_skills: dict[int, list[int]] = {}
	for material_id, skills in sorted(materials.items()):
		for skill in skills:
			material_skills.setdefault(skill["skillId"], []).append(material_id)

	wanted = {s for skills in skilluse.values() for s in skills} | set(godstones.values()) | set(material_skills)
	templates, counts = load_skill_templates_counted(data, wanted) if wanted else ({}, {})
	classes_of = {skill_id: _effect_classes(template, skill_rules) for skill_id, template in templates.items()}

	# skilluse
	by_class: dict[str, dict] = {}
	rows = []
	for item_id in sorted(skilluse):
		item_classes: set[str] = set()
		for skill_id in skilluse[item_id]:
			for cls in classes_of[skill_id]:
				entry = by_class.setdefault(cls, {"items": set(), "effects": 0, "skills": set()})
				entry["effects"] += 1
				entry["skills"].add(skill_id)
				item_classes.add(cls)
		for cls in item_classes:
			by_class[cls]["items"].add(item_id)
		rows.append({"itemId": item_id, "name": names[item_id], "skillIds": skilluse[item_id], "classes": sorted(item_classes),
		             "starter": item_id in starter, "droppableOn": sorted(m for m, ids in droppable_by_map.items() if item_id in ids)})
	skilluse_section = {
		"starterItems": len(starter),
		"droppableItems": {str(m): len(ids) for m, ids in sorted(droppable_by_map.items())},
		"items": len(skilluse),
		"fromStarter": sum(1 for i in skilluse if i in starter),
		"droppableByMap": {str(m): sum(1 for i in skilluse if i in ids) for m, ids in sorted(droppable_by_map.items())},
		"skills": len({s for skills in skilluse.values() for s in skills}),
		"leafClasses": {cls: {"items": len(v["items"]), "effects": v["effects"], "skills": len(v["skills"])} for cls, v in sorted(by_class.items())},
		"classChain": _class_chain(by_class, skill_rules),
		"itemRows": rows,
	}

	# godstones
	stone_classes: dict[str, dict] = {}
	for item_id, skill_id in godstones.items():
		for cls in set(classes_of[skill_id]):
			entry = stone_classes.setdefault(cls, {"skills": set(), "items": 0})
			entry["skills"].add(skill_id)
			entry["items"] += 1
	by_map = {}
	for map_id, ids in sorted(droppable_by_map.items()):
		dropped = sorted(i for i in godstones if i in ids)
		by_map[str(map_id)] = {"items": len(dropped), "skills": len({godstones[i] for i in dropped}),
		                       "leafClasses": sorted({c for i in dropped for c in classes_of[godstones[i]]}), "itemIds": dropped}
	godstone_section = {
		"items": len(godstones),
		"skills": len(set(godstones.values())),
		"skillsWithSeveralTemplates": sorted(s for s in set(godstones.values()) if counts[s] > 1),
		"leafClasses": {cls: {"skills": len(v["skills"]), "items": v["items"]} for cls, v in sorted(stone_classes.items())},
		"classChain": _class_chain(stone_classes, skill_rules),
		"droppableByMap": by_map,
	}

	# materials
	material_classes: dict[str, set[int]] = {}
	material_rows = []
	for skill_id in sorted(material_skills):
		for cls in classes_of[skill_id]:
			material_classes.setdefault(cls, set()).add(skill_id)
		material_rows.append({"skillId": skill_id, "name": templates[skill_id].attrs.get("name"), "materials": material_skills[skill_id],
		                      "classes": sorted(set(classes_of[skill_id]))})
	material_section = {
		"materialsWithSkills": sum(1 for skills in materials.values() if skills),
		"skills": len(material_skills),
		"leafClasses": {cls: {"skills": len(v)} for cls, v in sorted(material_classes.items())},
		"classChain": _class_chain(material_classes, skill_rules),
		"skillRows": material_rows,
	}
	return {
		"format": "aion-m5b3-item-survey",
		"version": 1,
		"maps": sorted(droppable_by_map),
		"killers": {str(m): k for m, k in sorted((killers or {}).items())},
		"skilluse": skilluse_section,
		"godstones": godstone_section,
		"materials": material_section,
		"assumptions": [
			"the droppable items of a map are m5b3-drops --survey's for the map's default killer (race by world type, level 1, the default drop "
			"rate): every rule and custom drop candidate and every quest drop item, whatever its chance",
			"the last <skill_template> of an id is the one used (SkillData.afterUnmarshal)",
		],
	}


def droppable_items(drop_data, map_ids: list[int]) -> tuple[dict[int, set[int]], dict[int, dict]]:
	"""The droppable item ids of each map and the killer each survey assumed (m5b3/drops.py map_survey_with_items)."""
	from m5b3.drops import map_survey_with_items

	droppable: dict[int, set[int]] = {}
	killers: dict[int, dict] = {}
	for map_id in map_ids:
		document, ids = map_survey_with_items(drop_data, map_id)
		if document["refused"]:
			raise OracleError(f"map {map_id}: the drop survey refused npc(s) {[r['npcId'] for r in document['refused']]} - their drops are unknown")
		droppable[map_id] = ids
		killers[map_id] = document["killer"]
	return droppable, killers
