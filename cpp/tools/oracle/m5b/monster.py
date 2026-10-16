"""m5b-monster: what the M5b gate needs to predict a fight against one npc id on one map (m5b-plan.md G-01, D7, D11).

Java rules, each with the method the value comes from:
- the template: NpcTemplate's level, <stats maxHp>, rank, rating, srange (aggroRange), sangle (aggroAngle, default 360), arange (attackRange),
  attack_speed (default 2000), race (default NONE), tribe, ai and <bound_radius> (NpcTemplate.java:54-110, 185-256). A spot may override the ai
  name (Creature.java:64-66);
- the spots: SpawnsData.getSpawnsByWorldId through m5a/spawns.py (the custom, pool, difficulty, handler and temporary rules live there and are not
  repeated here), plus the spot's static_id and the group's respawn_time. A spot with static_id != 0 makes Npc.hasStatic() true, which the gate
  does not want (m5b-plan.md D11), so the report marks the nearest spot whose static id is 0 and which is *fixed* - not a pool, not a walker and
  not randomly walking, i.e. the OracleSpot::isPinnedToFixedSpots() condition of the M5a gate;
- the experience: StatFunctions.calculateBaseExp (round(maxHp * (ratingMultiplier + rank.ordinal() * 0.2f))), calculateExperienceReward
  (round(baseExp * instanceHandler.getExpMultiplier() * (XPRewardEnum.xpRewardFrom(npcLevel - playerLevel) / 100f))) and
  Rates.XP_HUNTING.calcResult ((long) min(xp * rate, expNeed * 0.2f)) with expNeed = PlayerCommonData.getExpNeed()
  (StatFunctions.java:49-90; Rates.java:13-18, 175-190; PlayerCommonData.java:105-115);
- the two ranges of the first hit: attackRange = 1 + PlayerGameStats.getAttackRange().getCurrent() / 1000f (PlayerController.java:400-402,
  PlayerGameStats.java:127-138) plus, because PositionUtil.isInAttackRange calls isInRange(..., centerToCenter = false), both bound radii
  (PositionUtil.java:243-251); toleranceRange = attackRange + PositionUtil.calculateMaxCoveredDistance(player, 100) = the band that
  PlayerController.attackTarget adds while the target does not hate the player yet (PlayerController.java:404-405, PositionUtil.java:289-294).

What the oracle does NOT model, and raises OracleError for instead of guessing: instance maps (their exp multiplier is 1.5f and
calculateExperienceReward multiplies by the instance's maxPlayers, which is not static data), a map with a registered @InstanceID handler (it may
override getExpMultiplier, as BeshmundirInstance and RaksangRuinsInstance do), a template without a rating (Java's switch would throw), and a
player with stat modifiers - like m5a-creation, this oracle computes the values of a *fresh* character, whose starting gear adds no
ATTACK_RANGE, ATTACK_SPEED, SPEED or BOOST_HUNTING_XP_RATE modifier. Its passive skills apply at enter world since M5b-2 part 3 (m5b2-plan.md
D2); m5a-creation's passiveStatFunctions lists what they register, and a passive that changes one of those four stats is refused.

The constructor data of the Java enums and the literals of the four formulas (the rating multipliers, the rank step, the exp multipliers, the
player's bound radius and run speed, the attack range and attack speed bases) are read from the Java sources, so a change in the Java tree is a
test failure here and not a silently wrong expectation.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path

from staticdata_oracle import OracleError

from m5a.creation import JavaEnums, creation_report, enum_constants
from m5a.data import StaticData, java_int
from m5a.javafloat import distance, f32, parse_float, round_to_int, to_long
from m5a.spawns import GameClock, evaluate, load_groups, load_npc_templates

# the player stats the report computes as if no stat function changed them (see the module docstring)
UNMODELLED_PLAYER_STATS = ("ATTACK_RANGE", "ATTACK_SPEED", "SPEED", "BOOST_HUNTING_XP_RATE")

# WorldMapTemplate.worldType -> the race whose spawn point the distances are measured from (world/WorldType.java, model/Race.java)
RACE_OF_WORLD_TYPE = {"ELYSEA": "ELYOS", "ASMODAE": "ASMODIANS"}


def _read(source: Path) -> str:
	try:
		return source.read_text(encoding="utf-8")
	except OSError as e:
		raise OracleError(f"{source}: {e}") from e


def _java_long(text: str | None, what: str) -> int:
	"""A JAXB long element (PlayerExperienceTable.experience is a long[], and level 63 alone is above the int range)."""
	if text is None:
		raise OracleError(f"missing required value {what}")
	try:
		number = int(text.strip(), 10)
	except ValueError as e:
		raise OracleError(f"{what}={text!r} is not a long") from e
	if not -9223372036854775808 <= number <= 9223372036854775807:
		raise OracleError(f"{what}={text!r} is out of the long range")
	return number


def _search(source: Path, pattern: str, what: str) -> re.Match:
	match = re.search(pattern, _read(source))
	if not match:
		raise OracleError(f"{source}: cannot read {what} (the Java source does not have the shape this oracle was written against)")
	return match


@dataclass(frozen=True)
class JavaCombatRules:
	"""The literals and enum tables of the combat formulas, read from the Java sources."""

	rating_multipliers: dict[str, float]  # StatFunctions.calculateBaseExp, the switch over NpcRating
	rank_step: float                      # StatFunctions.calculateBaseExp, `multiplier += npc.getRank().ordinal() * 0.2f`
	rank_ordinals: dict[str, int]         # NpcRank, in declaration order
	xp_reward_percent: dict[int, int]     # XPRewardEnum(levelDifference, xpRewardPercent)
	exp_multiplier_instance: float        # GeneralInstanceHandler.getExpMultiplier, the instance arm
	exp_multiplier_open_world: float      # ... and the open world arm
	player_bound_front: float             # PlayerAccountData: playerCommonData.setBoundingRadius(new BoundRadius(front, side, boundHeight))
	player_bound_side: float
	player_run_speed: float               # PlayerClass.PlayerStatsTemplate.getRunSpeed
	base_attack_range: int                # PlayerGameStats.getAttackRange, the value without a weapon
	base_attack_speed: int                # PlayerGameStats.getBaseAttackSpeed, the value without a weapon

	@staticmethod
	def read(java_src: Path) -> "JavaCombatRules":
		base = Path(java_src) / "com" / "aionemu" / "gameserver"
		stat_functions = base / "utils" / "stats" / "StatFunctions.java"
		text = _read(stat_functions)
		body = re.search(r"calculateBaseExp\(Npc npc\)\s*\{(.*?)\n\t\}", text, re.DOTALL)
		if not body:
			raise OracleError(f"{stat_functions}: calculateBaseExp not found")
		multipliers = {name: float(value) for name, value in re.findall(r"case\s+([A-Z_]+):\s*multiplier\s*=\s*([0-9.]+)f;", body.group(1))}
		if not multipliers:
			raise OracleError(f"{stat_functions}: calculateBaseExp has no `case RATING: multiplier = Nf;` arms")
		step = re.search(r"multiplier\s*\+=\s*npc\.getRank\(\)\.ordinal\(\)\s*\*\s*([0-9.]+)f;", body.group(1))
		if not step:
			raise OracleError(f"{stat_functions}: calculateBaseExp does not add the rank ordinal")

		ranks = {name: ordinal for ordinal, (name, _) in enumerate(enum_constants(base / "model" / "templates" / "npc" / "NpcRank.java", "NpcRank"))}
		rewards: dict[int, int] = {}
		for name, args in enum_constants(base / "utils" / "stats" / "XPRewardEnum.java", "XPRewardEnum"):
			parts = [p.strip() for p in (args or "").split(",")]
			if len(parts) != 2:
				raise OracleError(f"XPRewardEnum.{name}: expected 2 constructor arguments")
			rewards[java_int(parts[0], f"XPRewardEnum.{name} levelDifference")] = java_int(parts[1], f"XPRewardEnum.{name} xpRewardPercent")

		exp_multiplier = _search(base / "instance" / "handlers" / "GeneralInstanceHandler.java",
		                         r"getExpMultiplier\(\)\s*\{\s*return[^?]*\?\s*([0-9.]+)f\s*:\s*([0-9.]+)f;", "getExpMultiplier")
		bound = _search(base / "model" / "account" / "PlayerAccountData.java",
		                r"setBoundingRadius\(new BoundRadius\(([0-9.]+)f,\s*([0-9.]+)f,", "the player bound radius")
		run_speed = _search(base / "model" / "PlayerClass.java", r"public float getRunSpeed\(\)\s*\{\s*return\s+([0-9.]+)f;", "the player run speed")
		player_stats = base / "model" / "stats" / "container" / "PlayerGameStats.java"
		attack_range = _search(player_stats, r"public Stat2 getAttackRange\(\)\s*\{\s*int base = (\d+);", "the base attack range")
		attack_speed = _search(player_stats, r"public int getBaseAttackSpeed\(\)\s*\{\s*int base = (\d+);", "the base attack speed")
		return JavaCombatRules(multipliers, f32(float(step.group(1))), ranks, rewards, f32(float(exp_multiplier.group(1))),
		                       f32(float(exp_multiplier.group(2))), f32(float(bound.group(1))), f32(float(bound.group(2))),
		                       f32(float(run_speed.group(1))), int(attack_range.group(1)), int(attack_speed.group(1)))

	def rating_multiplier(self, rating: str | None, rank: str | None) -> float:
		"""StatFunctions.calculateBaseExp: the rating multiplier plus rank.ordinal() * 0.2f, in float arithmetic."""
		if rating is None:
			raise OracleError("the npc template has no rating (Java: NullPointerException in the switch of calculateBaseExp)")
		if rating not in self.rating_multipliers:
			raise OracleError(f"rating {rating}: calculateBaseExp throws IllegalArgumentException for it")
		if rank is None:
			raise OracleError("the npc template has no rank (Java: NullPointerException on npc.getRank().ordinal())")
		if rank not in self.rank_ordinals:
			raise OracleError(f"unknown NpcRank {rank}")
		return f32(self.rating_multipliers[rating] + f32(self.rank_ordinals[rank] * self.rank_step))

	def xp_reward_from(self, level_difference: int) -> int:
		"""XPRewardEnum.xpRewardFrom: clamped to the first and the last constant."""
		lowest, highest = min(self.xp_reward_percent), max(self.xp_reward_percent)
		if level_difference < lowest:
			return self.xp_reward_percent[lowest]
		if level_difference > highest:
			return self.xp_reward_percent[highest]
		if level_difference not in self.xp_reward_percent:
			raise OracleError(f"XPRewardEnum has no constant for the level difference {level_difference} (Java: NoSuchElementException)")
		return self.xp_reward_percent[level_difference]


def base_exp(max_hp: int, multiplier: float) -> int:
	"""StatFunctions.calculateBaseExp: 0 for a non-positive maxHp, else Math.round(maxHp * multiplier)."""
	if max_hp <= 0:
		return 0
	return round_to_int(f32(max_hp * multiplier))


def experience_reward(base: int, map_multiplier: float, xp_percentage: int) -> int:
	"""StatFunctions.calculateExperienceReward: Math.round(baseXP * mapMulti * (xpPercentage / 100f)) (the open world arm)."""
	return round_to_int(f32(f32(base * map_multiplier) * f32(xp_percentage / f32(100.0))))


def xp_hunting(reward: int, xp_rate: float, exp_need: int) -> int:
	"""Rates.XP_HUNTING.calcResult: (long) Math.min(xp * rate, expNeed * 0.2f)."""
	return to_long(min(f32(reward * xp_rate), f32(exp_need * f32(0.2))))


def exp_need(experience_table: list[int], level: int) -> int:
	"""PlayerCommonData.getExpNeed: startExp(level + 1) - startExp(level), 0 at the max level (PlayerExperienceTable.getStartExpForLevel)."""
	max_level = len(experience_table)
	if level < 0 or level > max_level:
		raise OracleError(f"level {level} is not in the experience table (Java: IllegalArgumentException)")
	if level == max_level:
		return 0
	start = lambda l: 0 if l == 0 else experience_table[l - 1]  # noqa: E731 - getStartExpForLevel
	return start(level + 1) - start(level)


def attack_range(attack_range_stat: int, player_bound: float, npc_bound: float) -> float:
	"""
	The range PositionUtil.isInAttackRange finally compares against, for two creatures that are both standing still: PlayerController's
	`1 + attackRangeStat / 1000f` (PlayerController.java:402) plus both bound radii, which isInRange(o1, o2, range, false) adds
	(PositionUtil.java:246-249). A moving attacker or target adds calculateMaxDistanceOffset on top, which the gate avoids by stopping first.
	"""
	return f32(f32(f32(1 + f32(attack_range_stat / f32(1000.0))) + player_bound) + npc_bound)


def max_covered_distance(movement_speed: int, millis: int) -> float:
	"""PositionUtil.calculateMaxCoveredDistance: metersPerSecondInThousands * movementDurationMillis / 1_000_000f, 0 for a non-positive duration."""
	if millis <= 0:
		return 0.0
	return f32(movement_speed * millis / f32(1000000.0))


@dataclass(frozen=True)
class NpcTemplateInfo:
	level: int
	max_hp: int
	rating: str | None
	rank: str | None
	aggro_range: int
	aggro_angle: int
	npc_attack_range: int
	attack_speed: int
	race: str
	tribe: str | None
	ai: str | None
	bound_front: float
	bound_side: float
	bound_upper: float


def npc_template(data: StaticData, npc_id: int) -> NpcTemplateInfo:
	"""The NpcTemplate attributes the gate needs, with the JAXB field defaults of NpcTemplate.java:54-110."""
	for element in data.stream("npc_templates", "npc_template"):
		if java_int(element.get("npc_id"), "npc_template npc_id") != npc_id:
			continue
		stats = element.find("stats")
		if stats is None or stats.get("maxHp") is None:
			raise OracleError(f"npc {npc_id} has no <stats maxHp=...> (its maxHp would be 0 and calculateBaseExp would answer 0)")
		bound = element.find("bound_radius")  # VisibleObjectTemplate.getBoundRadius falls back to BoundRadius.DEFAULT = (0, 0, 0)
		return NpcTemplateInfo(
			java_int(element.get("level"), f"npc_template {npc_id} level", 0),
			java_int(stats.get("maxHp"), f"npc_template {npc_id} maxHp"),
			element.get("rating"), element.get("rank"),
			java_int(element.get("srange"), f"npc_template {npc_id} srange", 0),
			java_int(element.get("sangle"), f"npc_template {npc_id} sangle", 360),
			java_int(element.get("arange"), f"npc_template {npc_id} arange", 0),
			java_int(element.get("attack_speed"), f"npc_template {npc_id} attack_speed", 2000),
			element.get("race", "NONE"), element.get("tribe"), element.get("ai"),
			parse_float(bound.get("front", "0")) if bound is not None else 0.0,
			parse_float(bound.get("side", "0")) if bound is not None else 0.0,
			parse_float(bound.get("upper", "0")) if bound is not None else 0.0)
	raise OracleError(f"no npc_template with npc_id {npc_id}")


def _map_template(data: StaticData, map_id: int):
	for element in data.children("world_maps", "map"):
		if java_int(element.get("id"), "map id") == map_id:
			return element
	raise OracleError(f"map {map_id} is not in world_maps")


def _instance_handler_class(handlers_dir: Path, map_id: int) -> str | None:
	"""The class of `@InstanceID(map_id)` under data/handlers (InstanceEngine.addInstanceHandlerClass), which may override getExpMultiplier."""
	if not handlers_dir.is_dir():
		raise OracleError(f"{handlers_dir} does not exist: pass --java-handlers (the instance handler of the map decides its exp multiplier)")
	pattern = re.compile(r"@InstanceID\(\s*" + str(map_id) + r"\s*\)")
	for path in sorted(handlers_dir.rglob("*.java")):
		if pattern.search(_read(path)):
			return path.stem
	return None


def player_weapon_stats(data: StaticData, enums: JavaEnums, rules: JavaCombatRules, items: list[dict]) -> tuple[int, int]:
	"""
	(PlayerGameStats.getAttackRange().getCurrent(), getBaseAttackSpeed()) of a fresh character wearing its starting gear, which carries no
	ATTACK_RANGE or ATTACK_SPEED stat modifier: the main hand is the equipped item whose slot mask is ItemSlot.MAIN_HAND and the off hand the one
	in ItemSlot.SUB_HAND (Equipment.java:677-686; one item per slot here, so the "same item in both slots" arm cannot be taken).
	getAttackRange takes the smaller of the two weapon ranges unless the off hand is a shield, i.e. ItemGroup.SHIELD - the only group with
	ItemSubType.SHIELD (ItemGroup.java:30, Equipment.java:484-490); getBaseAttackSpeed adds a quarter of the off hand's speed, in int division,
	and does so for *any* off hand item, so an off hand without <weapon_stats> makes Java throw NullPointerException and this oracle raise.
	"""
	slot_masks = {name: mask for name, mask, _ in enums.slots}
	for slot in ("MAIN_HAND", "SUB_HAND"):
		if slot not in slot_masks:
			raise OracleError(f"ItemSlot has no {slot} constant")
	equipped = {item["slot"]: item["itemId"] for item in items if item["equipped"]}
	main_id = equipped.get(slot_masks["MAIN_HAND"])
	off_id = equipped.get(slot_masks["SUB_HAND"])
	wanted = {item_id for item_id in (main_id, off_id) if item_id is not None}
	groups: dict[int, str] = {}
	stats: dict[int, tuple[int, int] | None] = {}  # None: the template has no <weapon_stats>, so getWeaponStats() returns null
	for element in data.stream("item_templates", "item_template"):
		item_id = java_int(element.get("id"), "item_template id")
		if item_id not in wanted:
			continue
		groups[item_id] = element.get("item_group", "NONE")
		weapon = element.find("weapon_stats")
		stats[item_id] = (java_int(weapon.get("attack_range"), "attack_range", 0), java_int(weapon.get("attack_speed"), "attack_speed", 0)) \
			if weapon is not None else None
	for item_id in wanted:
		if item_id not in stats:
			raise OracleError(f"item {item_id} has no item_template")
		if groups[item_id] not in enums.item_groups:
			raise OracleError(f"item {item_id}: unknown item_group {groups[item_id]}")

	def weapon_of(item_id: int, method: str) -> tuple[int, int]:
		if stats[item_id] is None:
			raise OracleError(f"item {item_id} is equipped in a hand but has no <weapon_stats> (Java: NullPointerException in {method})")
		return stats[item_id]

	shield = off_id is not None and groups[off_id] == "SHIELD"
	min_weapon_range = None  # Java: Integer.MAX_VALUE
	if main_id is not None:
		min_weapon_range = weapon_of(main_id, "PlayerGameStats.getAttackRange")[0]
	if off_id is not None and not shield:
		off_range = weapon_of(off_id, "PlayerGameStats.getAttackRange")[0]
		min_weapon_range = off_range if min_weapon_range is None else min(min_weapon_range, off_range)
	attack_range_stat = rules.base_attack_range if min_weapon_range is None else min_weapon_range

	attack_speed = rules.base_attack_speed
	if main_id is not None:
		attack_speed = weapon_of(main_id, "PlayerGameStats.getBaseAttackSpeed")[1]
		if off_id is not None:
			attack_speed += weapon_of(off_id, "PlayerGameStats.getBaseAttackSpeed")[1] // 4
	return attack_range_stat, attack_speed


def monster_report(data: StaticData, java_src: Path, handlers_dir: Path | None, map_id: int, npc_id: int, player_level: int, clock: GameClock,
                   race: str | None = None, player_class: str = "WARRIOR", xp_solo_rate: float = 1.0) -> dict:
	java_src = Path(java_src)
	rules = JavaCombatRules.read(java_src)
	enums = JavaEnums(java_src)
	world_map = _map_template(data, map_id)
	world_type = world_map.get("world_type", "NONE")
	if world_map.get("instance") is not None and world_map.get("instance") not in ("false", "0"):
		raise OracleError(f"map {map_id} is an instance: calculateExperienceReward then multiplies by the instance's maxPlayers, which is not "
		                  "static data, and the oracle does not model it")
	handler = _instance_handler_class(handlers_dir if handlers_dir is not None else java_src.parent / "data" / "handlers", map_id)
	if handler is not None:
		raise OracleError(f"map {map_id} has the instance handler {handler}, which may override getExpMultiplier; the oracle does not model it")
	if race is None:
		if world_type not in RACE_OF_WORLD_TYPE:
			raise OracleError(f"map {map_id} has world_type {world_type}: pass --race, the oracle cannot tell whose spawn point to measure from")
		race = RACE_OF_WORLD_TYPE[world_type]

	creation = creation_report(data, java_src, race, player_class)
	for function in creation["passiveStatFunctions"]:
		if function["applies"] and function["stat"] in UNMODELLED_PLAYER_STATS:
			raise OracleError(f"skill {function['skillId']}: its passive {function['function']} changes {function['stat']}, which this oracle "
			                  "computes for a character without stat functions")
	spawn = creation["spawn"]
	attack_range_stat, player_attack_speed = player_weapon_stats(data, enums, rules, creation["items"])
	player_bound = max(rules.player_bound_front, rules.player_bound_side)  # BoundRadius.getMaxOfFrontAndSide
	movement_speed = round_to_int(f32(rules.player_run_speed * 1000))  # PlayerGameStats.getMovementSpeed, the run arm

	template = npc_template(data, npc_id)
	npc_bound = max(template.bound_front, template.bound_side)
	range_value = attack_range(attack_range_stat, player_bound, npc_bound)
	covered = max_covered_distance(movement_speed, 100)

	rows = [row for row in evaluate(load_groups(data, map_id), load_npc_templates(data), clock) if row["npcId"] == npc_id]
	if not rows:
		raise OracleError(f"npc {npc_id} has no regular spawn spot on map {map_id}")
	spots = []
	for row in rows:
		flags = row["flags"]
		fixed = not flags["pool"] and not flags["walker"] and not flags["randomWalk"]
		spots.append({
			"x": row["x"], "y": row["y"], "z": row["z"], "h": row["h"],
			"staticId": row["staticId"],
			"ai": row["spotAi"] if row["spotAi"] is not None else template.ai,
			"respawnTime": row["respawnTime"],
			"spawned": row["spawned"],
			"fixed": fixed,
			"flags": flags,
			"distance": round(distance(f32(spawn["x"]), f32(spawn["y"]), f32(spawn["z"]), row["x"], row["y"], row["z"]), 3),
		})
	spots.sort(key=lambda s: (s["distance"], s["x"], s["y"], s["z"]))
	plain = [s for s in spots if s["staticId"] == 0 and s["fixed"] and s["spawned"] is True]
	respawn_times = sorted({s["respawnTime"] for s in spots})

	multiplier = rules.rating_multiplier(template.rating, template.rank)
	base = base_exp(template.max_hp, multiplier)
	xp_percentage = rules.xp_reward_from(template.level - player_level)
	reward = experience_reward(base, rules.exp_multiplier_open_world, xp_percentage)
	experience_table = [_java_long(e.text, "player_experience_table exp") for e in data.children("player_experience_table", "exp")]
	need = exp_need(experience_table, player_level)
	return {
		"format": "aion-m5b-monster",
		"version": 1,
		"map": map_id,
		"worldType": world_type,
		"npcId": npc_id,
		"playerLevel": player_level,
		"gameTime": {"hour": clock.hour, "day": clock.day, "month": clock.month, "weekday": clock.weekday},
		"template": {
			"level": template.level,
			"maxHp": template.max_hp,
			"rating": template.rating,
			"rank": template.rank,
			"rankOrdinal": rules.rank_ordinals.get(template.rank) if template.rank else None,
			"aggroRange": template.aggro_range,
			"aggroAngle": template.aggro_angle,
			"attackRange": template.npc_attack_range,
			"attackSpeed": template.attack_speed,
			"race": template.race,
			"tribe": template.tribe,
			"ai": template.ai,
			"boundRadius": {"front": template.bound_front, "side": template.bound_side, "upper": template.bound_upper,
			                "maxOfFrontAndSide": npc_bound},
		},
		# every regular spawn spot of the id on the map, nearest first; `fixed` is the isPinnedToFixedSpots condition per spot and `pinned` is it
		# for the whole id, which is what the M5a gate's V2 needs before it may assert an exact position
		"spots": spots,
		"pinned": all(s["fixed"] for s in spots),
		"nearestPlainSpot": plain[0] if plain else None,
		"respawnTime": respawn_times[0] if len(respawn_times) == 1 else None,
		"respawnTimes": respawn_times,
		"player": {
			"race": race,
			"playerClass": player_class,
			"spawn": spawn,
			"attackRangeStat": attack_range_stat,
			"attackSpeed": player_attack_speed,
			"movementSpeed": movement_speed,
			"boundRadius": {"front": rules.player_bound_front, "side": rules.player_bound_side, "maxOfFrontAndSide": player_bound},
		},
		"exp": {
			"ratingMultiplier": multiplier,
			"baseExp": base,
			"expMultiplier": rules.exp_multiplier_open_world,
			"xpPercentage": xp_percentage,
			"experienceReward": reward,
			"xpSoloRate": xp_solo_rate,
			"expNeed": need,
			# Rates.XP_HUNTING's cap, expNeed * 0.2f, and what the character actually gains: the STR_GET_EXP parameter of R1(a)
			"cap": f32(need * f32(0.2)),
			"awarded": xp_hunting(reward, xp_solo_rate, need),
		},
		"ranges": {
			"attackRange": range_value,
			"maxCoveredDistance": covered,
			"toleranceRange": f32(range_value + covered),
		},
	}
