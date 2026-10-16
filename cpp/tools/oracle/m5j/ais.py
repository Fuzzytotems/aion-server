"""m5j-ais: what the M5j stage-3 gate (m5j-plan.md §10.4 Z12, §18.4 CP4 G-31) spawns for the root AIs of N-01, as the Java server reads it
from the static data and the handler sources.

Java rules, each with the place the value comes from:
- an AI's name is its handler class's @AIName (data/handlers/ai/<Class>.java; AIEngine registers the classes by that name);
- an npc template runs the AI its `ai` attribute names (NpcTemplate.getAiName; AIEngine.setupAI). Of the templates of an AI, the gate spawns the
  lowest npc id whose handleSpawned finds its data: BombAI.handleSpawned reads AI_DATA.getAiTemplate(npcId).getBombs() and SummonerAI's
  handleSpawned getSummons().getPercentage() (BombAI.java:29-34, SummonerAI.java:57-61: a template without that row throws
  NullPointerException), so for "bomb" and "summoner" only npc ids with an <ai npcId> row carrying <bombs> or <summons> (ai_templates) count;
- an AI without a template is reached through `//ai set <name>` on a spawned npc (Ai.java:82-89: World.despawn, the new AI, World.spawn),
  whose info answer is "Npc now has AI " + the new AI's class getSimpleName(); the gate spawns that npc from the lowest "general" template
  (AIEngine's default AI, GeneralNpcAI);
- `//spawn <id>` spawns at the GM's spot (SpawnNpc.java: SpawnEngine.newSpawn with the admin's position, a temporary spawn);
- stage 4 (§18.5 CP2, W-13): auto_group.xml's rows by mask id - the instance map and the level range AutoGroupType.isInLvlRange reads
  (AutoGroup's min_lvl / max_lvl, 0 when absent).
"""

from __future__ import annotations

import re
from pathlib import Path

from staticdata_oracle import OracleError

from m5a.data import java_int
from m5j.social import _read, _squeeze

# N-01's M5j root AIs (m5j-plan.md §18.4: CP1's fifteen and CP2's eight)
AI_CLASSES = (
	"AggressiveNoLootNpcAI", "BookAI", "BubblegutAI", "FirecrackerAI", "NeutralGuardAI", "NoDmgNoActionAI", "NoInteractionAI", "OneDmgAI",
	"OneDmgNoActionAI", "ShifterAI", "AggressiveBossSummonNpcAI", "UseSkillAndDieAI", "BombAI", "PlatinumFountainAI", "SpeakerAI",
	"ChestAI", "HiddenTeleportNpcAI", "SummonerAI", "SkillCooltimeResetAI", "ConquestOfferingAggressiveAI", "ConquestOfferingBuffNpcAI",
	"ConquestOfferingPortalAI", "ConquestOfferingSpawnerAI",
)

# the AI data each handleSpawned reads (the <ai> child that must exist)
NEEDS_AI_DATA = {"bomb": "bombs", "summoner": "summons"}

BASE_AI = "general"

AI_NAME = re.compile(r'@AIName\("([^"]+)"\)')

# statements the rules above were written against (whitespace-insensitive): a change in the Java source fails the oracle
STATEMENTS = (
	("../data/handlers/admincommands/Ai.java", 'sendInfo(admin, "Npc now has AI " + newAi.getClass().getSimpleName());'),
	("../data/handlers/admincommands/SpawnNpc.java",
	 "SpawnTemplate st = SpawnEngine.newSpawn(admin.getWorldId(), npcId, admin.getX(), admin.getY(), admin.getZ(), admin.getHeading(), respawnTime);"),
	("../data/handlers/ai/BombAI.java", "template = DataManager.AI_DATA.getAiTemplate(getNpcId()).getBombs().getBombTemplate();"),
	("../data/handlers/ai/SummonerAI.java", "percentage = new ArrayList<>(DataManager.AI_DATA.getAiTemplate(getNpcId()).getSummons().getPercentage());"),
)


def check_statements(java_src: Path) -> None:
	for rel, statement in STATEMENTS:
		text = _squeeze(_read(java_src / rel))
		if _squeeze(statement) not in text:
			raise OracleError(f"{rel}: the statement this oracle models is gone: {statement}")


def ai_name(java_src: Path, cls: str) -> str:
	source = _read(java_src.parent / "data" / "handlers" / "ai" / f"{cls}.java")
	found = AI_NAME.findall(source)
	if len(found) != 1:
		raise OracleError(f"{cls}.java: expected one @AIName, found {len(found)}")
	return found[0]


def ai_data_rows(data) -> dict[str, set[int]]:
	"""npc ids of the <ai npcId> rows that carry each child the handlers need"""
	rows: dict[str, set[int]] = {child: set() for child in NEEDS_AI_DATA.values()}
	for ai in data.stream("ai_templates", "ai"):
		npc_id = java_int(ai.get("npcId"), "ai npcId")
		for child in rows:
			if ai.find(child) is not None:
				rows[child].add(npc_id)
	return rows


def lowest_templates(data) -> dict[str, int]:
	"""the lowest npc id of every ai name, and per needed-data name the lowest one with its row (keys 'name' and 'name+data')"""
	lowest: dict[str, list[int]] = {}
	for template in data.stream("npc_templates", "npc_template"):
		name = template.get("ai")
		if name is None:
			continue
		lowest.setdefault(name, []).append(java_int(template.get("npc_id"), "npc_id"))
	return {name: sorted(ids) for name, ids in lowest.items()}


def ais_report(data, java_src: Path) -> dict:
	check_statements(java_src)
	templates = lowest_templates(data)
	rows = ai_data_rows(data)
	ais = []
	for cls in AI_CLASSES:
		name = ai_name(java_src, cls)
		ids = templates.get(name, [])
		needed = NEEDS_AI_DATA.get(name)
		if needed is not None:
			ids = [i for i in ids if i in rows[needed]]
		ais.append({"class": cls, "name": name, "npcId": ids[0] if ids else None, "templates": len(templates.get(name, []))})
	base = templates.get(BASE_AI)
	if not base:
		raise OracleError(f'no npc template has ai="{BASE_AI}"')
	auto_groups = {}
	for row in data.stream("auto_groups", "auto_group"):
		mask = java_int(row.get("id"), "auto_group id")
		auto_groups[str(mask)] = {"instanceId": java_int(row.get("instanceId"), "auto_group instanceId"),
		                          "minLvl": java_int(row.get("min_lvl"), "min_lvl", 0), "maxLvl": java_int(row.get("max_lvl"), "max_lvl", 0)}
	return {
		"ais": ais,
		"autoGroups": auto_groups,
		"aiSet": {"baseNpcId": base[0], "baseAi": BASE_AI, "answerPrefix": "Npc now has AI "},
	}
