"""m5a-spawns and m5a-border-target: the objects SpawnEngine.spawnAll puts into an open world map, by spot, with the flags that make a spot
nondeterministic (pools, walkers, temporary spawns outside the given game time, handler spawns), and a region move target for the visibility case.

Java rules (SpawnsData.addRegularSpawns, SpawnEngine.spawnInstance, VisibleObjectSpawner.spawnNpc, TemporarySpawn, GameTime, PositionUtil):
- regular spawns are the <spawn> children of every <spawn_map>, grouped by npc id; a custom spawn (custom="true") removes the groups of its npc
  id collected so far for the map, and later spawns of that npc id in the same <spawn_map> are skipped;
- a group with difficult_id != 0 is not spawned in the open world (difficulty 0);
- a temporary group (<temporary_spawn> on the spawn) is spawned only if it is in spawn time; a spot's own <temporary_spawn> is checked only when
  the group is not a pool;
- a handler group (handler="RIFT"/"STATIC"/...) spawns no Npc: RIFT goes to the RiftManager, STATIC creates StaticObjects, others nothing;
- a pool smaller than the number of spots spawns a random subset (a pool that is not smaller logs a warning and spawns every spot);
- npc ids 400001..499998 are gatherables; any other id needs an npc template, otherwise nothing is spawned;
- spots with a walker_id may be regrouped into walker formations (other positions); npc type FLAG npcs are visible map-wide (FlagKnownList);
- "in range" is PositionUtil.isInRange: float squared 3D distance strictly below range * range.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field

from staticdata_oracle import OracleError

from .data import StaticData, java_boolean, java_int
from .javafloat import distance, f32, in_range, parse_float

MINUTES_IN_HOUR = 60
MINUTES_IN_DAY = 24 * MINUTES_IN_HOUR
MONTH_DAYS = [31] * 12  # GameTime.Month: every month has 31 days
MINUTES_IN_YEAR = sum(MONTH_DAYS) * MINUTES_IN_DAY
WEEKDAYS = ("MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY", "SUNDAY")


@dataclass(frozen=True)
class GameClock:
	"""The game time of GameTimeService (hour 0-23, day 1-31, month 1-12) and the server's day of week; None = unknown to the caller."""

	hour: int | None = None
	day: int | None = None
	month: int | None = None
	weekday: str | None = None

	@staticmethod
	def from_minutes(minutes: int, weekday: str | None = None) -> "GameClock":
		"""GameTime.getHour/getDay/getMonth of `minutes` since 01.01.0000 00:00 (SM_GAME_TIME)."""
		if minutes < 0:
			raise OracleError("game time must be >= 0")
		hour = (minutes % MINUTES_IN_DAY) // MINUTES_IN_HOUR
		month = 0
		rest = minutes % MINUTES_IN_YEAR
		for days in MONTH_DAYS:
			month += 1
			rest -= days * MINUTES_IN_DAY
			if rest < 0:
				break
		day = 1
		in_year = minutes % MINUTES_IN_YEAR
		for days in MONTH_DAYS:
			in_month = days * MINUTES_IN_DAY
			if in_year > in_month:
				in_year -= in_month
			else:
				if in_year < in_month:  # if both are equal, it's day 1 of the following month
					day += in_year // MINUTES_IN_DAY
				break
		return GameClock(hour, day, month, weekday)


@dataclass(frozen=True)
class TemporarySpawn:
	weekdays: tuple[str, ...]
	spawn: tuple[int | None, int | None, int | None]  # hour, day, month (None = "*")
	despawn: tuple[int | None, int | None, int | None]

	@staticmethod
	def parse(element) -> "TemporarySpawn":
		weekdays = tuple(element.get("weekdays", "").split())
		for weekday in weekdays:
			if weekday not in WEEKDAYS:
				raise OracleError(f"temporary_spawn weekdays: {weekday!r} is not a DayOfWeek")
		return TemporarySpawn(weekdays, _parse_times(element.get("spawn_time")), _parse_times(element.get("despawn_time")))

	def is_in_spawn_time(self, clock: GameClock) -> bool | None:
		"""TemporarySpawn.isInSpawnTime; None if the answer depends on a part of the clock the caller did not give."""
		if self.weekdays:
			if clock.weekday is None:
				return None
			if clock.weekday not in self.weekdays:
				return False
		for current, spawn, despawn, check in ((clock.month, self.spawn[2], self.despawn[2], _check_date),
		                                       (clock.day, self.spawn[1], self.despawn[1], _check_date),
		                                       (clock.hour, self.spawn[0], self.despawn[0], _check_hour)):
			if spawn is None:
				continue
			if current is None:
				return None
			if not check(current, spawn, despawn):
				return False
		return True


def _parse_times(text: str | None) -> tuple[int | None, int | None, int | None]:
	"""TemporarySpawn.parseTime for types 0..2 of "hour.day.month" ("*" = any, "/n" = every nth, stored negative)."""
	if text is None:
		return (None, None, None)
	parts = text.split(".")
	while parts and parts[-1] == "":  # Java String.split drops trailing empty strings
		parts.pop()
	if len(parts) < 3:
		raise OracleError(f"temporary_spawn time {text!r}: Java throws ArrayIndexOutOfBoundsException")
	result = []
	for part in parts[:3]:
		if part == "*":
			result.append(None)
			continue
		if part.startswith("/"):
			part = "-" + part[1:]
		result.append(java_int(part, f"temporary_spawn time {text!r}"))
	return tuple(result)


def _check_with_despawn_expression(current: int, spawn_or_expression: int, despawn_expression: int) -> bool:
	if spawn_or_expression < 0:
		spawn_or_expression = -spawn_or_expression
	return current >= spawn_or_expression and spawn_or_expression == despawn_expression


def _check_date(current: int, spawn: int, despawn: int | None) -> bool:
	if despawn is not None and despawn < 0:
		return _check_with_despawn_expression(current, spawn, -despawn)
	if spawn < 0:
		spawn = -spawn
	if despawn is None:
		return current >= spawn
	if spawn <= despawn:
		return spawn <= current <= despawn
	return current >= spawn or current <= despawn


def _check_hour(current: int, spawn: int, despawn: int | None) -> bool:
	if despawn is not None and despawn < 0:
		return _check_with_despawn_expression(current, spawn, -despawn)
	if spawn < 0:
		spawn = -spawn
	if despawn is None:
		return current >= spawn
	if spawn < despawn:
		return spawn <= current < despawn
	if spawn > despawn:
		return current >= spawn or current < despawn
	return True


@dataclass
class Spot:
	npc_id: int
	x: float
	y: float
	z: float
	h: int
	walker_id: str | None
	random_walk: int
	temporary: TemporarySpawn | None
	# SpawnSpotTemplate.staticId (default 0): a spot with a static id makes Npc.hasStatic() true, which changes ask(IS_IMMUNE_TO_ABNORMAL_STATES),
	# puts GeoService.spawn/despawnPlaceableObject on the spawn and death paths and adds a staticId argument to every canSee (m5b-plan.md D11).
	static_id: int = 0
	# SpawnSpotTemplate.ai: overrides the npc template's ai name for this spot; SpawnTemplate.NO_AI ("null") means "no ai at all"
	# (SpawnTemplate.java:39, Creature.java:64-66)
	ai: str | None = None


@dataclass
class Group:
	npc_id: int
	pool: int
	difficult_id: int
	handler: str | None
	temporary: TemporarySpawn | None
	spots: list[Spot] = field(default_factory=list)
	# Spawn.respawnTime in seconds, default 0 = no respawn (SpawnTemplate.isNoRespawn; RespawnService.scheduleRespawn schedules at
	# getRespawnTime() * 1000 ms)
	respawn_time: int = 0


@dataclass(frozen=True)
class NpcInfo:
	level: int
	npc_type: str


def load_npc_templates(data: StaticData) -> dict[int, NpcInfo]:
	npcs: dict[int, NpcInfo] = {}
	for element in data.stream("npc_templates", "npc_template"):
		npc_id = java_int(element.get("npc_id"), "npc_template npc_id")
		npcs[npc_id] = NpcInfo(java_int(element.get("level"), f"npc_template {npc_id} level", 0), element.get("type", "NONE"))
	return npcs


def load_groups(data: StaticData, map_id: int) -> list[Group]:
	"""SpawnsData.getSpawnsByWorldId(map_id): the regular spawn groups (the order inside the result is not used by the oracle)."""
	by_npc: dict[int, list[Group]] = {}
	for spawn_map in data.children("spawns", "spawn_map"):
		if java_int(spawn_map.get("map_id"), "spawn_map map_id") != map_id:
			continue
		customs: list[int] = []
		for spawn in spawn_map.findall("spawn"):
			npc_id = java_int(spawn.get("npc_id"), "spawn npc_id")
			if npc_id in customs:
				continue
			if java_boolean(spawn.get("custom")):  # event spawns are not read from the spawns holder
				by_npc.pop(npc_id, None)
				customs.append(npc_id)
			temporary = spawn.find("temporary_spawn")
			group = Group(npc_id, java_int(spawn.get("pool"), f"spawn {npc_id} pool", 0), java_int(spawn.get("difficult_id"), f"spawn {npc_id}", 0),
			              spawn.get("handler"), TemporarySpawn.parse(temporary) if temporary is not None else None,
			              respawn_time=java_int(spawn.get("respawn_time"), f"spawn {npc_id} respawn_time", 0))
			for spot in spawn.findall("spot"):
				spot_temporary = spot.find("temporary_spawn")
				group.spots.append(Spot(npc_id, parse_float(spot.get("x")), parse_float(spot.get("y")), parse_float(spot.get("z")),
				                        _java_byte(spot.get("h"), npc_id), spot.get("walker_id"), java_int(spot.get("random_walk"), "random_walk", 0),
				                        TemporarySpawn.parse(spot_temporary) if spot_temporary is not None else None,
				                        java_int(spot.get("static_id"), f"spot static_id of npc {npc_id}", 0), spot.get("ai")))
			by_npc.setdefault(npc_id, []).append(group)
	return [group for groups in by_npc.values() for group in groups]


def _java_byte(text: str | None, npc_id: int) -> int:
	value = java_int(text, f"spot h of npc {npc_id}", 0)  # JAXB leaves a missing attribute at the field default 0
	if not -128 <= value <= 127:
		raise OracleError(f"spot h={text!r} of npc {npc_id} is out of the byte range")
	return value


def is_gatherable(npc_id: int) -> bool:
	return 400000 < npc_id < 499999


def evaluate(groups: list[Group], npcs: dict[int, NpcInfo], clock: GameClock) -> list[dict]:
	"""Every spot of the map's regular spawn groups with its flags; `spawned` is True, False or None (depends on randomness or unknown time)."""
	rows = []
	for group in groups:
		group_in_time = True if group.temporary is None else group.temporary.is_in_spawn_time(clock)
		pool = 0 < group.pool < len(group.spots)
		template = npcs.get(group.npc_id)
		gatherable = is_gatherable(group.npc_id)
		for spot in group.spots:
			spot_in_time = True
			if not pool and spot.temporary is not None:
				spot_in_time = spot.temporary.is_in_spawn_time(clock)
			if group.difficult_id != 0 or group.handler is not None or group_in_time is False or spot_in_time is False:
				spawned = False
			elif not gatherable and template is None:
				spawned = False  # VisibleObjectSpawner.spawnNpc: "No template for NPC"
			elif pool or group_in_time is None or spot_in_time is None:
				spawned = None
			else:
				spawned = True
			walker = spot.walker_id is not None
			rows.append({
				"npcId": spot.npc_id,
				"x": spot.x,
				"y": spot.y,
				"z": spot.z,
				"h": spot.h,
				"level": template.level if template else None,
				"spawned": spawned,
				"deterministic": spawned is True and not walker,
				# per spot, for the M5b monster oracle (m5b-plan.md D11 and G-01): the spot's static id, the spot's ai override
				# (Creature.java:64-66) and the group's respawn time in seconds
				"staticId": spot.static_id,
				"spotAi": spot.ai,
				"respawnTime": group.respawn_time,
				"flags": {
					"pool": pool,
					"temporary": group.temporary is not None or spot.temporary is not None,
					"walker": walker,
					"randomWalk": spot.random_walk > 0,
					"handler": group.handler,
					"gatherable": gatherable,
					"flag": template is not None and template.npc_type == "FLAG",
					"difficultId": group.difficult_id,
				},
			})
	rows.sort(key=lambda r: (r["npcId"], r["x"], r["y"], r["z"], r["h"]))
	return rows


def spots_report(data: StaticData, map_id: int, center: tuple[float, float, float], radius: float, clock: GameClock) -> dict:
	rows = evaluate(load_groups(data, map_id), load_npc_templates(data), clock)
	cx, cy, cz = (f32(c) for c in center)
	near = []
	for row in rows:
		if in_range(cx, cy, cz, row["x"], row["y"], row["z"], radius):
			near.append(dict(row, distance=round(distance(cx, cy, cz, row["x"], row["y"], row["z"]), 3)))
	flags = [row for row in rows if row["flags"]["flag"] and row["spawned"] is not False]
	return {
		"format": "aion-m5a-spawns",
		"version": 1,
		"map": map_id,
		"center": [cx, cy, cz],
		"radius": radius,
		"gameTime": {"hour": clock.hour, "day": clock.day, "month": clock.month, "weekday": clock.weekday},
		"spots": near,
		"flagNpcs": flags,
	}


def _deterministic_within(rows: list[dict], point: tuple[float, float, float], radius: float) -> set[tuple]:
	return {(r["npcId"], r["x"], r["y"], r["z"]) for r in rows if r["deterministic"] and not r["flags"]["gatherable"] and
	        in_range(point[0], point[1], point[2], r["x"], r["y"], r["z"], radius)}


def _any_within(rows: list[dict], point: tuple[float, float, float], radius: float) -> set[tuple]:
	return {(r["npcId"], r["x"], r["y"], r["z"]) for r in rows if r["spawned"] is not False and
	        in_range(point[0], point[1], point[2], r["x"], r["y"], r["z"], radius)}


def border_target(data: StaticData, map_id: int, start: tuple[float, float, float], clock: GameClock, world_size: int | None = None) -> dict:
	"""The first point T (distances 150..300 m in 10 m steps, then 8 directions from east counter-clockwise) inside the map where a straight
	walk from the start makes deterministic NPCs appear (within 90 m of T, not within 100 m of the start) and disappear (within 90 m of the
	start, not within 100 m of T). T keeps the start's z."""
	rows = evaluate(load_groups(data, map_id), load_npc_templates(data), clock)
	sx, sy, sz = (f32(c) for c in start)
	size = world_size if world_size is not None else _world_size(data, map_id)
	for dist in range(150, 301, 10):
		for step in range(8):
			angle = step * math.pi / 4
			tx = f32(sx + dist * math.cos(angle))
			ty = f32(sy + dist * math.sin(angle))
			if not (0 <= tx < size and 0 <= ty < size):
				continue
			target = (tx, ty, sz)
			appear = _deterministic_within(rows, target, 90) - _any_within(rows, (sx, sy, sz), 100)
			disappear = _deterministic_within(rows, (sx, sy, sz), 90) - _any_within(rows, target, 100)
			if appear and disappear:
				return {
					"format": "aion-m5a-border-target",
					"version": 1,
					"map": map_id,
					"start": [sx, sy, sz],
					"target": [tx, ty, sz],
					"distance": dist,
					"direction": step * 45,
					"appear": [{"npcId": k[0], "x": k[1], "y": k[2], "z": k[3]} for k in sorted(appear)],
					"disappear": [{"npcId": k[0], "x": k[1], "y": k[2], "z": k[3]} for k in sorted(disappear)],
				}
	raise OracleError(f"no region move target found around {start} in map {map_id}")


def _world_size(data: StaticData, map_id: int) -> int:
	for element in data.children("world_maps", "map"):
		if java_int(element.get("id"), "map id") == map_id:
			return java_int(element.get("world_size"), f"map {map_id} world_size", 0)
	raise OracleError(f"map {map_id} is not in world_maps")
