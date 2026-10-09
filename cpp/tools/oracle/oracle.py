#!/usr/bin/env python3
"""Independent static data oracles (static-data.md section 4). Python 3.12, stdlib only. See README.md.

	oracle.py generate [--static-data DIR] [--country-code N] [--out DIR]   V2+V3+V4 in one pass, writes the expected/ documents
	oracle.py counts [--static-data DIR] [--country-code N] [--json F] [--txt F]
	oracle.py check [--static-data DIR]                                    regenerate and diff against expected/ (exit 1 on drift)
	oracle.py compare-counts --log FILE [--expected F]                     "Loaded N ..." lines of a (C++) log vs expected counts
	oracle.py compare-totals --actual FILE [--expected F]                  V3 totals document of the C++ loader vs expected totals
	oracle.py xsd-check --ir FILE [--xsd-root DIR] [--allowlist F] [--out F]   V1: IR vs the XSDs (exit 1 on unallowed differences)
	oracle.py xsd-inventory [--xsd-root DIR]                                XSD construct counts (sanity check of the XSD reader)
	oracle.py m5a-spawns --map ID --x X --y Y --z Z [--radius R] [--game-minutes M | --game-hour H ...] [--weekday DAY]
	oracle.py m5a-border-target --map ID --x X --y Y --z Z [--game-minutes M | --game-hour H ...]
	oracle.py m5a-creation --race ELYOS|ASMODIANS --class CLASS [--java-src DIR]   (m5a/ package, m5a-plan.md F-05)
	oracle.py m5b-monster --map ID --npc-id ID [--player-level N] [--race R] [--class C] [--xp-solo-rate R]   (m5b/ package, m5b-plan.md G-01)
	oracle.py m5b2-skills --race R --class C [--level N] [--skill ID[:LEVEL] ...] [--npc ID ...] [--death-count N]   (m5b2/, m5b2-plan.md G-01)
	oracle.py m5b3-drops (--npc ID [--map ID] | --survey --map ID) [--player-level N] [--race R] [--drop-rate R]   (m5b3/drops.py, m5b3-plan.md G-01)
	                     [--inventory ITEM[:COUNT] ...] [--cube-expansions N]   (the cube-slot budget, m5b3/items.py)
	oracle.py m5b3-item (--item ID [--item ID ...] | --survey --map ID [--map ID ...])   (m5b3/items.py, m5b3/survey.py, m5b3-plan.md G-01)
	oracle.py m5b3-material --map ID [--near X,Y,Z [--stand [--bound-upper U]]] [--radius R] [--limit N] [--geo-dir DIR]   (m5b3/materials.py, G-01, G-04)
	oracle.py m5c-trade (--npc ID [--item ID] | --item ID | --map ID) [--count N] [--race R] [--set KEY=VALUE ...]   (m5c/trade.py, m5c-plan.md G-01)
	oracle.py m5c-craft (--recipe ID [--skill-level N] [--craft-type 0|1] | --skill ID --level N [--map ID] | --gatherable ID [--skill-level N])
	                    [--skill-xp X] [--character-level N] [--membership M] [--set KEY=VALUE ...]   (m5c/craft.py, m5c-plan.md §2.6 G-01)
	oracle.py m5c-economy [--map ID] [--npc ID ...] [--near X,Y,Z] [--far D] [--direction DEG] [--recover-exp N] [--npc-expands N]
	                      [--quest-expands N] [--item-expands N] [--mail ITEM:COUNT:KINAH[:express] ...] [--item ID ...] [--manastone ID ...]
	                      [--membership M] [--class C] [--race R] [--level N] [--daeva CLASS [--daeva-old-level N]]
	                      [--craft-recipe ID --craft-tool ID [--craft-map ID] [--craft-distance D ...]]
	                      [--influence RACE=N ...] [--profile FILE | --no-profile] [--set KEY=VALUE ...]   (m5c/economy.py, m5c/sanctum.py, G-01)
	oracle.py m5d-quest --quest ID [--race R] [--class C] [--level N] [--exp X] [--gender G] [--completed ID[:GROUP] ...] [--inventory ITEM[:COUNT] ...]
	                    [--profile FILE | --no-profile]   (m5d/quests.py, m5d-plan.md G-01)
	oracle.py m5d-quests --map ID [--race R] [--class C] [--level N] [--gender G] [--completed ID[:GROUP] ...] [--started ID ...]
	                     [--inventory ITEM[:COUNT] ...] [--profile FILE | --no-profile]   (m5d/quests.py)
	oracle.py m5d-quests (--registration-order [--npc ID] | --census)   the XML registry's lists and m5d-plan.md §2.3-§2.5 (m5d/registry.py)
	oracle.py m5e-progression --race R --class C [--level N] [--daeva] [--known-skills FILE] [--step STEP ...] [--skill ID ...]
	                     [--weapon ID ...] [--dp N] [--robot] [--chain-after CATEGORY] [--target-kind PC|NPC]   (m5e/progression.py,
	                     m5e-plan.md G-01): the experience table, the class change's pages and actions, the class master, the skills a
	                     sequence of steps (create, enter:L, level:L, class:C, action:ID, quit, seed:C, book:ITEM) teaches with their message ids, the
	                     base max HP / MP after each step, and the constants and conditions of the gate's casts and weapons
	oracle.py m5f-travel [--npc ID ...] [--race R] [--hotspot ID ... --from X,Y,Z ...] [--obelisk NPCID ...] [--portal NPCID ... [--now-ms MS] [--tz Z]]
	                     [--instance-exit WORLD ...] [--instance-spawns WORLD ... --near X,Y,Z --radius R [--difficulty N] [--game-hour H]]
	                     [--exp-for-level L] [--census [--cpp-src DIR]] [--geo-check [--geo-map ID ...]] [--profile FILE | --no-profile]
	                     (m5f/, m5f-plan.md G-01): travel destinations, prices, portals, instance cooltimes/exits/spawns, the M5f census
	oracle.py quest-trace generate [--out DIR] [--only REL ...] | check [--expected-dir DIR] [--only REL ...]   golden traces of the Java
	                     quest handlers (questtrace/, phase6-inventory.md §7.6 item 3): expected/quest/<id>.json for the first slice (exit 1
	                     on drift; with --only, only the named handlers are checked)
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from staticdata_oracle import OracleError  # noqa: E402
from staticdata_oracle import run as runner  # noqa: E402
from staticdata_oracle import totals as totals_mod  # noqa: E402
from staticdata_oracle import xsdcheck  # noqa: E402
from staticdata_oracle.imports import DEFAULT_COUNTRY_CODE  # noqa: E402


def _data_dir(args):
	return runner.check_static_data_dir(Path(args.static_data) if args.static_data else runner.DEFAULT_STATIC_DATA)


def cmd_generate(args):
	result = runner.run(_data_dir(args), args.country_code)
	for path in runner.write_outputs(result, Path(args.out)):
		print(f"wrote {path}")
	return 0


def cmd_counts(args):
	result = runner.run(_data_dir(args), args.country_code, totals=False, census=False, region_variants=False)
	text = runner.counts_text(result["counts"])
	if args.json:
		Path(args.json).write_text(runner.dump_json(result["counts"]), encoding="utf-8", newline="\n")
	if args.txt:
		Path(args.txt).write_text(text, encoding="utf-8", newline="\n")
	if not args.json and not args.txt:
		sys.stdout.write(text)
	return 0


def cmd_check(args):
	result = runner.run(_data_dir(args), DEFAULT_COUNTRY_CODE)
	expected_dir = Path(args.expected_dir)
	drift = []
	for name, content in runner.output_files(result).items():
		path = expected_dir / name
		if not path.is_file() or path.read_text(encoding="utf-8") != content:
			drift.append(name)
	for name in drift:
		print(f"drift: {expected_dir / name}")
	return 1 if drift else 0


def cmd_compare_counts(args):
	expected = runner.load_json(Path(args.expected))
	diffs = runner.compare_counts_log(expected, Path(args.log).read_text(encoding="utf-8", errors="replace"))
	for d in diffs:
		print(d)
	print("counts equal" if not diffs else f"{len(diffs)} difference(s)")
	return 1 if diffs else 0


def cmd_compare_totals(args):
	diffs = totals_mod.compare(runner.load_json(Path(args.expected)), runner.load_json(Path(args.actual)))
	for d in diffs:
		print(d)
	print("totals equal" if not diffs else f"{len(diffs)} difference(s)")
	return 1 if diffs else 0


def cmd_xsd_check(args):
	xsd_root = Path(args.xsd_root) if args.xsd_root else runner.DEFAULT_STATIC_DATA
	report = xsdcheck.check(runner.load_json(Path(args.ir)), xsdcheck.load_schemas(xsd_root),
	                        runner.load_json(Path(args.allowlist)) if args.allowlist else None)
	text = runner.dump_json(report)
	if args.out:
		Path(args.out).write_text(text, encoding="utf-8", newline="\n")
	else:
		sys.stdout.write(text)
	return 0 if report["ok"] else 1


def cmd_xsd_inventory(args):
	xsd_root = Path(args.xsd_root) if args.xsd_root else runner.DEFAULT_STATIC_DATA
	sys.stdout.write(runner.dump_json(xsdcheck.load_schemas(xsd_root).inventory()))
	return 0


def _m5a_clock(args):
	from m5a.spawns import GameClock
	if args.game_minutes is not None:
		return GameClock.from_minutes(args.game_minutes, args.weekday)
	return GameClock(args.game_hour, args.game_day, args.game_month, args.weekday)


def cmd_m5a_spawns(args):
	from m5a.data import StaticData
	from m5a.spawns import spots_report
	report = spots_report(StaticData(_data_dir(args)), args.map, (args.x, args.y, args.z), args.radius, _m5a_clock(args))
	sys.stdout.write(runner.dump_json(report))
	return 0


def cmd_m5a_border_target(args):
	from m5a.data import StaticData
	from m5a.spawns import border_target
	sys.stdout.write(runner.dump_json(border_target(StaticData(_data_dir(args)), args.map, (args.x, args.y, args.z), _m5a_clock(args))))
	return 0


def cmd_m5a_creation(args):
	from m5a.creation import creation_report
	from m5a.data import StaticData
	data_dir = _data_dir(args)
	java_src = Path(args.java_src) if args.java_src else data_dir.parent.parent / "src"
	sys.stdout.write(runner.dump_json(creation_report(StaticData(data_dir), java_src, args.race, args.player_class)))
	return 0


def cmd_m5b_monster(args):
	from m5a.data import StaticData
	from m5b.monster import monster_report
	data_dir = _data_dir(args)
	java_src = Path(args.java_src) if args.java_src else data_dir.parent.parent / "src"
	handlers = Path(args.java_handlers) if args.java_handlers else None
	report = monster_report(StaticData(data_dir), java_src, handlers, args.map, args.npc_id, args.player_level, _m5a_clock(args), args.race,
	                        args.player_class, args.xp_solo_rate)
	sys.stdout.write(runner.dump_json(report))
	return 0


def cmd_m5b2_skills(args):
	from m5a.data import StaticData
	from m5b2.skills import skills_report
	data_dir = _data_dir(args)
	java_src = Path(args.java_src) if args.java_src else data_dir.parent.parent / "src"
	report = skills_report(StaticData(data_dir), java_src, args.race, args.player_class, args.level, args.skill or [], args.npc or [],
	                       args.death_count)
	sys.stdout.write(runner.dump_json(report))
	return 0


def _item_counts(values, option: str) -> list[tuple[int, int]]:
	"""ITEM[:COUNT] arguments (COUNT default 1)"""
	result = []
	for value in values or []:
		item, _, count = value.partition(":")
		try:
			result.append((int(item), int(count) if count else 1))
		except ValueError as e:
			raise OracleError(f"{option} {value}: not ITEM[:COUNT]") from e
	return result


def cmd_m5b3_drops(args):
	from m5a.data import StaticData
	from m5b3.drops import DropData, drops_report, map_survey
	from m5b3.items import JavaItemRules, cube_budget
	if args.survey == (args.npc is not None) or (args.survey and args.map is None):
		raise OracleError("pass --npc ID [--map ID], or --survey --map ID")
	if args.survey and (args.inventory or args.cube_expansions):
		raise OracleError("--inventory and --cube-expansions describe the looter's cube for one --npc, not a survey")
	data_dir = _data_dir(args)
	java_src = Path(args.java_src) if args.java_src else data_dir.parent.parent / "src"
	data = StaticData(data_dir)
	drop_data = DropData(data, java_src, Path(args.java_handlers) if args.java_handlers else None)
	if args.survey:
		report = map_survey(drop_data, args.map, args.player_level, args.race, args.drop_rate)
	else:
		report = drops_report(drop_data, args.npc, args.map, args.player_level, args.race, args.drop_rate)
		if report["registerDrop"] or report["registerDropByAi"]:
			report["cube"] = cube_budget(report, data, JavaItemRules.read(java_src), _item_counts(args.inventory, "--inventory"),
			                             args.cube_expansions)
	sys.stdout.write(runner.dump_json(report))
	return 0


def cmd_m5b3_item(args):
	from m5a.data import StaticData
	from m5b3.items import item_report
	if bool(args.item) == args.survey:
		raise OracleError("pass --item ID [--item ID ...], or --survey --map ID [--map ID ...]")
	if args.survey != bool(args.map):
		raise OracleError("--map names the maps of a --survey (and --survey needs at least one)")
	data_dir = _data_dir(args)
	java_src = Path(args.java_src) if args.java_src else data_dir.parent.parent / "src"
	data = StaticData(data_dir)
	if args.survey:
		from m5b3.drops import DropData
		from m5b3.survey import droppable_items, item_survey
		drop_data = DropData(data, java_src, Path(args.java_handlers) if args.java_handlers else None)
		droppable, killers = droppable_items(drop_data, args.map)
		sys.stdout.write(runner.dump_json(item_survey(data, java_src, droppable, killers)))
	else:
		sys.stdout.write(runner.dump_json(item_report(data, java_src, args.item)))
	return 0


def cmd_m5b3_material(args):
	from m5a.data import StaticData
	from m5b3.materials import material_report
	data_dir = _data_dir(args)
	near = None
	if args.near is not None:
		parts = args.near.split(",")
		try:
			near = tuple(float(p) for p in parts)
		except ValueError as e:
			raise OracleError(f"--near {args.near}: not x,y,z") from e
		if len(near) != 3:
			raise OracleError(f"--near {args.near}: not x,y,z")
	geo_dir = Path(args.geo_dir) if args.geo_dir else data_dir.parent / "geo"
	world_maps = data_dir / "world_maps.xml"
	if args.stand and near is None:
		raise OracleError("--stand needs --near")
	extra = {} if args.bound_upper is None else {"bound_upper": args.bound_upper}
	report = material_report(StaticData(data_dir), geo_dir, world_maps, args.map, near, args.radius, args.limit, stand=args.stand, **extra)
	sys.stdout.write(runner.dump_json(report))
	return 0


def cmd_m5c_trade(args):
	from m5a.creation import RACES
	from m5a.data import StaticData
	from m5c.trade import parse_influences, trade_report
	from m5c.trade_config import load_config, refuse_event_keys
	data_dir = _data_dir(args)
	java_src = Path(args.java_src) if args.java_src else data_dir.parent.parent / "src"
	config_dir = Path(args.config) if args.config else java_src.parent / "config"
	profile = None if args.no_profile else Path(args.profile) if args.profile else config_dir / "mygs.properties"
	config = load_config(java_src, config_dir, profile, args.set or [], require_profile=bool(args.profile) and not args.no_profile)
	data = StaticData(data_dir, config["gameserver.country.code"].value)  # the region variants of gameserver.country.code
	refuse_event_keys(data)
	report = trade_report(data, java_src, config, args.npc, args.item, args.map, tuple(args.race) if args.race else RACES, args.count,
	                      parse_influences(args.influence or []), args.legion_level, args.account_max_level, args.membership,
	                      handlers_dir=Path(args.java_handlers) if args.java_handlers else None)
	sys.stdout.write(runner.dump_json(report))
	return 0


def cmd_m5c_craft(args):
	from m5a.data import StaticData
	from m5c.craft import CraftContext, craft_report
	data_dir = _data_dir(args)
	java_src = Path(args.java_src) if args.java_src else data_dir.parent.parent / "src"
	config_dir = Path(args.config) if args.config else java_src.parent / "config"
	profile = None if args.no_profile else Path(args.profile) if args.profile else config_dir / "mygs.properties"
	ctx = CraftContext.create(StaticData(data_dir), java_src, config_dir, profile, args.set or [], args.membership)
	report = craft_report(ctx, args.recipe, args.skill, args.level, args.gatherable, args.skill_level, args.craft_type, args.map,
	                      args.character_level, args.skill_xp)
	sys.stdout.write(runner.dump_json(report))
	return 0


def cmd_m5c_economy(args):
	from m5a.data import StaticData
	from m5c.economy import ECONOMY_KEYS, economy_report
	from m5c.trade import parse_influences
	from m5c.trade_config import load_config, refuse_event_keys
	data_dir = _data_dir(args)
	java_src = Path(args.java_src) if args.java_src else data_dir.parent.parent / "src"
	config_dir = Path(args.config) if args.config else java_src.parent / "config"
	profile = None if args.no_profile else Path(args.profile) if args.profile else config_dir / "mygs.properties"
	config = load_config(java_src, config_dir, profile, args.set or [], keys=ECONOMY_KEYS, require_profile=bool(args.profile) and not args.no_profile)
	data = StaticData(data_dir, config["gameserver.country.code"].value)
	refuse_event_keys(data, ECONOMY_KEYS)
	near = None
	if args.near is not None:
		try:
			near = tuple(float(p) for p in args.near.split(","))
		except ValueError as e:
			raise OracleError(f"--near {args.near}: not x,y,z") from e
		if len(near) != 3:
			raise OracleError(f"--near {args.near}: not x,y,z")
	craft_ctx = None
	if args.daeva is not None or args.craft_recipe is not None:  # the C19 blocks compose m5c-craft over the same data and profile
		from m5c.craft import CraftContext
		craft_ctx = CraftContext.create(data, java_src, config_dir, profile, args.set or [], args.membership)
	report = economy_report(data, java_src, config, args.map, args.npc or [], near, args.far, args.recover_exp, args.npc_expands, args.quest_expands,
	                        args.item_expands, args.mail or [], args.item or [], args.player_class, args.level,
	                        influences=parse_influences(args.influence or []),
	                        handlers_dir=Path(args.java_handlers) if args.java_handlers else None,
	                        commons_src=Path(args.commons_src) if args.commons_src else None, direction=args.direction, player_race=args.race,
	                        manastones=args.manastone or [], membership=args.membership, craft_ctx=craft_ctx, daeva_class=args.daeva,
	                        daeva_old_level=args.daeva_old_level, craft_recipe=args.craft_recipe, craft_map=args.craft_map, craft_tool=args.craft_tool,
	                        craft_distances=tuple(args.craft_distance) if args.craft_distance else None)
	sys.stdout.write(runner.dump_json(report))
	return 0


def _m5d_world(args):
	from m5a.data import StaticData
	from m5d.quests import DEFAULT_PROFILE, QuestWorld
	data_dir = _data_dir(args)
	java_src = Path(args.java_src) if args.java_src else data_dir.parent.parent / "src"
	profile =None if args.no_profile else Path(args.profile) if args.profile else DEFAULT_PROFILE
	return QuestWorld(StaticData(data_dir), java_src, Path(args.java_handlers) if args.java_handlers else None,
	                  Path(args.config) if args.config else None, profile)


def cmd_m5d_quest(args):
	from m5d.quests import quest_report
	report = quest_report(_m5d_world(args), args.quest, args.race, args.player_class, args.level, args.gender, args.completed or [],
	                      args.inventory or [], args.exp)
	sys.stdout.write(runner.dump_json(report))
	return 0


def cmd_m5d_quests(args):
	from m5d.quests import map_report
	from m5d.registry import census_report, registration_order_report
	if args.npc is not None and not args.registration_order:
		raise OracleError("--npc goes with --registration-order")
	if args.registration_order or args.census:
		# the character and game-time options, None unless given (--class and --level default in the --map path only)
		given = [flag for name, flag in (("race", "--race"), ("player_class", "--class"), ("level", "--level"), ("gender", "--gender"),
		                                 ("completed", "--completed"), ("started", "--started"), ("inventory", "--inventory"),
		                                 ("game_minutes", "--game-minutes"), ("game_hour", "--game-hour"), ("game_day", "--game-day"),
		                                 ("game_month", "--game-month"), ("weekday", "--weekday")) if getattr(args, name) is not None]
		if given:
			raise OracleError(f"{given[0]} describes a character or the game time: --registration-order and --census have neither")
		world = _m5d_world(args)
		report = registration_order_report(world, args.npc) if args.registration_order else census_report(world)
		sys.stdout.write(runner.dump_json(report))
		return 0
	report = map_report(_m5d_world(args), args.map, args.race, "WARRIOR" if args.player_class is None else args.player_class,
	                    1 if args.level is None else args.level, args.gender, _m5a_clock(args), args.completed or [], args.started or [],
	                    args.inventory or [])
	sys.stdout.write(runner.dump_json(report))
	return 0


def cmd_m5e_progression(args):
	from m5a.data import StaticData
	from m5e.progression import progression_report
	data_dir = _data_dir(args)
	java_src = Path(args.java_src) if args.java_src else data_dir.parent.parent / "src"
	handlers = Path(args.java_handlers) if args.java_handlers else data_dir.parent / "handlers" / "quest"
	character = {"dp": args.dp, "robot": args.robot}
	if args.chain_after is not None:
		character["chainAfter"] = args.chain_after
	if args.target_kind is not None:
		character["targetKind"] = args.target_kind
	if args.weapon_group is not None:
		character["weaponGroup"] = args.weapon_group
	prices = None
	if args.stigma:
		# StigmaService's kinah goes through PricesService.getPriceForService: the profile's price keys, as m5c-trade reads them
		from m5c.trade import race_prices
		from m5c.trade_config import load_config
		config_dir = Path(args.config) if args.config else java_src.parent / "config"
		profile = None if args.no_profile else Path(args.profile) if args.profile else config_dir / "mygs.properties"
		config = load_config(java_src, config_dir, profile, [], require_profile=bool(args.profile) and not args.no_profile)
		prices = race_prices(config, args.race, None)
	report = progression_report(StaticData(data_dir), java_src, handlers, args.race, args.player_class, args.level, args.step or [],
	                            Path(args.known_skills) if args.known_skills else None, args.daeva, args.skill or [], args.weapon or [],
	                            character, args.stigma or [], prices)
	sys.stdout.write(runner.dump_json(report))
	return 0


def cmd_m5e_stumble(args):
	from m5e.stumble import stumble_report
	data_dir = _data_dir(args)
	geo_dir = Path(args.geo_dir) if args.geo_dir else data_dir.parent / "geo"
	stumbles = []
	for text in args.stumble:
		try:
			values = tuple(float(v) for v in text.split(","))
		except ValueError as e:
			raise OracleError(f"--stumble {text}: not FX,FY,FZ,TX,TY") from e
		if len(values) != 5:
			raise OracleError(f"--stumble {text}: not FX,FY,FZ,TX,TY")
		stumbles.append(values)
	sys.stdout.write(runner.dump_json(stumble_report(geo_dir, data_dir / "world_maps.xml", args.map, stumbles)))
	return 0


def _xyz(text: str, option: str) -> tuple[float, float, float]:
	try:
		values = tuple(float(v) for v in text.split(","))
	except ValueError as e:
		raise OracleError(f"{option} {text}: not X,Y,Z") from e
	if len(values) != 3:
		raise OracleError(f"{option} {text}: not X,Y,Z")
	return values


def _paired(values: list, count: int, option: str, owner: str) -> list:
	"""one value per selector: as many as the selectors, or one shared by all"""
	if count == 0:
		return []
	if len(values) == count:
		return values
	if len(values) == 1:
		return values * count
	raise OracleError(f"{option}: give it once, or once per {owner} ({count}), not {len(values)} times")


def _m5f_timezone(args, config_dir: Path, profile: Path | None) -> str:
	"""--tz, else gameserver.timezone of the profile or config/main (GSConfig.TIME_ZONE_ID; empty = the system zone, ZoneIdTransformer)"""
	if args.tz:
		return args.tz
	from m5c.trade_config import _read_properties_file
	value = None
	main = config_dir / "main"
	if main.is_dir():
		for path in sorted(main.glob("*.properties")):
			for key, v in _read_properties_file(path):
				if key == "gameserver.timezone":
					value = v
	if profile is not None and profile.is_file():
		for key, v in _read_properties_file(profile):
			if key == "gameserver.timezone":
				value = v
	return value.strip() if value and value.strip() else "local"


def cmd_m5f_travel(args):
	import time as _time
	from m5a.data import StaticData
	from m5f import travel
	data_dir = _data_dir(args)
	java_src = Path(args.java_src) if args.java_src else data_dir.parent.parent / "src"
	config_dir = Path(args.config) if args.config else java_src.parent / "config"
	profile = None if args.no_profile else Path(args.profile) if args.profile else config_dir / "mygs.properties"
	if args.profile and not args.no_profile and not Path(args.profile).is_file():
		raise OracleError(f"--profile {args.profile}: no such file (only the default config/mygs.properties may be missing)")
	td = travel.TravelData(StaticData(data_dir), java_src)
	selectors = (args.npc, args.hotspot, args.obelisk, args.portal, args.instance_exit, args.instance_spawns)
	if not any(selectors) and args.exp_for_level is None and not args.census and not args.geo_check:
		raise OracleError("m5f-travel: give at least one selector (--npc, --hotspot, --obelisk, --portal, --instance-exit, --instance-spawns, "
		                  "--exp-for-level, --census, --geo-check)")
	result = {"format": "aion-m5f-travel", "version": 1, "npcs": [], "hotspots": [], "obelisks": [], "portals": [], "instanceExits": [],
	          "instanceSpawns": [], "exp": None, "census": None, "geoCheck": None}

	prices_cache = {}

	def prices_for(race):
		if race not in prices_cache:
			prices_cache[race] = travel.load_prices(java_src, config_dir, profile, bool(args.profile) and not args.no_profile, race)
		return prices_cache[race]

	if args.npc:
		daeva_only = travel.daeva_only_npcs(java_src)
		result["npcs"] = [travel.npc_report(td, npc, args.race, prices_for, daeva_only) for npc in args.npc]
	if args.hotspot:
		if not args.origin:
			raise OracleError("--hotspot needs --from X,Y,Z (the player's position)")
		origins = _paired(args.origin, len(args.hotspot), "--from", "--hotspot")
		result["hotspots"] = [travel.hotspot_report(td, h, _xyz(o, "--from")) for h, o in zip(args.hotspot, origins)]
	if args.obelisk:
		result["obelisks"] = [travel.obelisk_report(td, npc) for npc in args.obelisk]
	if (args.portal or args.instance_exit) and not args.race:
		raise OracleError("--portal and --instance-exit need --race ELYOS|ASMODIANS")
	if args.portal:
		now_ms = args.now_ms if args.now_ms is not None else int(_time.time() * 1000)
		zone = travel.Zone(_m5f_timezone(args, config_dir, profile))
		result["portals"] = [travel.portal_report(td, npc, args.race, now_ms, zone) for npc in args.portal]
	if args.instance_exit:
		result["instanceExits"] = [travel.instance_exit_report(td, w, args.race) for w in args.instance_exit]
	if args.instance_spawns:
		from m5a.spawns import GameClock
		count = len(args.instance_spawns)
		if not args.near or args.radius is None:
			raise OracleError("--instance-spawns needs --near X,Y,Z and --radius R")
		nears = _paired(args.near, count, "--near", "--instance-spawns")
		if args.game_minutes is not None:
			clock = GameClock.from_minutes(args.game_minutes, args.weekday)
		else:
			clock = GameClock(args.game_hour, args.game_day, args.game_month, args.weekday)
		result["instanceSpawns"] = [travel.instance_spawns_report(td, w, args.difficulty, _xyz(n, "--near"), args.radius, clock)
		                            for w, n in zip(args.instance_spawns, nears)]
	if args.exp_for_level is not None:
		result["exp"] = travel.exp_report(td, args.exp_for_level)
	if args.census:
		from m5f.census import census_report
		cpp_dir = Path(args.cpp_src) if args.cpp_src else data_dir.parents[2] / "cpp" / "game-server"
		handlers = Path(args.java_handlers) if args.java_handlers else data_dir.parent / "handlers" / "quest"
		result["census"] = census_report(td.data, java_src, handlers, cpp_dir, td.spawn_groups())
	if args.geo_check:
		from m5f.geocheck import geo_check
		geo_dir = Path(args.geo_dir) if args.geo_dir else data_dir.parent / "geo"
		result["geoCheck"] = geo_check(td, geo_dir, data_dir / "world_maps.xml", args.geo_map)
	sys.stdout.write(runner.dump_json(result))
	return 0


def cmd_m5h_legion(args):
	from m5h.legion import legion_report
	data_dir = _data_dir(args)
	java_src = Path(args.java_src) if args.java_src else data_dir.parent.parent / "src"
	sys.stdout.write(runner.dump_json(legion_report(java_src, args.message or [], args.question or [])))
	return 0


def cmd_m5h_housing(args):
	from m5h.housing import housing_report
	data_dir = _data_dir(args)
	java_src = Path(args.java_src) if args.java_src else data_dir.parent.parent / "src"
	spots = []
	for spec in args.npc or []:
		map_id, npc_id = spec.split(":")
		spots.append((int(map_id), int(npc_id)))
	sys.stdout.write(runner.dump_json(housing_report(data_dir, java_src, args.message or [], spots, args.item or [])))
	return 0


def cmd_m5g_team(args):
	from m5a.data import StaticData
	from m5g.team import team_report
	data_dir = _data_dir(args)
	java_src = Path(args.java_src) if args.java_src else data_dir.parent.parent / "src"
	kills = None
	if args.kill:
		kills = [[flag == "1" for flag in kill] for kill in args.kill]
	report = team_report(StaticData(data_dir), java_src, args.npc_id, args.levels, kills, args.xp_group_rate, args.xp_solo_rate, args.message or [],
	                     args.question or [], args.max_level)
	sys.stdout.write(runner.dump_json(report))
	return 0


def cmd_quest_trace(args):
	from questtrace import extract
	rels = extract.SLICE if not args.only else tuple(args.only)
	if args.action == "generate":
		docs = extract.generate(Path(args.out), rels)
		cases = sum(len(d["cases"]) for d in docs.values())
		refused = [f"{d['java']}: {h['hook']}: {h['unsupported']}" for d in docs.values() for h in d["hooks"] if "unsupported" in h]
		print(f"quest-trace: {len(docs)} quests, {cases} cases written to {args.out}; {len(refused)} hooks refused")
		for r in refused:
			print(f"  {r}")
		return 0
	problems = extract.check(Path(args.expected_dir), rels, extra=not args.only)
	for pr in problems:
		print(pr)
	print(f"quest-trace check: {len(problems)} problems")
	return 1 if problems else 0


def main(argv=None):
	parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	sub = parser.add_subparsers(dest="command", required=True)

	def data_args(p, country=True):
		p.add_argument("--static-data", help=f"static_data directory (default {runner.DEFAULT_STATIC_DATA})")
		if country:
			p.add_argument("--country-code", type=int, default=DEFAULT_COUNTRY_CODE, help="GSConfig.SERVER_COUNTRY_CODE (default 99)")

	p = sub.add_parser("generate")
	data_args(p)
	p.add_argument("--out", default=str(runner.EXPECTED_DIR))
	p.set_defaults(fn=cmd_generate)

	p = sub.add_parser("counts")
	data_args(p)
	p.add_argument("--json")
	p.add_argument("--txt")
	p.set_defaults(fn=cmd_counts)

	p = sub.add_parser("check")
	data_args(p, country=False)
	p.add_argument("--expected-dir", default=str(runner.EXPECTED_DIR))
	p.set_defaults(fn=cmd_check)

	p = sub.add_parser("compare-counts")
	p.add_argument("--log", required=True)
	p.add_argument("--expected", default=str(runner.EXPECTED_DIR / "static_data_counts.json"))
	p.set_defaults(fn=cmd_compare_counts)

	p = sub.add_parser("compare-totals")
	p.add_argument("--actual", required=True)
	p.add_argument("--expected", default=str(runner.EXPECTED_DIR / "totals.json"))
	p.set_defaults(fn=cmd_compare_totals)

	p = sub.add_parser("xsd-check")
	p.add_argument("--ir", required=True)
	p.add_argument("--xsd-root")
	p.add_argument("--allowlist")
	p.add_argument("--out")
	p.set_defaults(fn=cmd_xsd_check)

	p = sub.add_parser("xsd-inventory")
	p.add_argument("--xsd-root")
	p.set_defaults(fn=cmd_xsd_inventory)

	def clock_args(p):
		p.add_argument("--game-minutes", type=int, help="SM_GAME_TIME minutes since 01.01.0000 (sets hour, day and month)")
		p.add_argument("--game-hour", type=int)
		p.add_argument("--game-day", type=int)
		p.add_argument("--game-month", type=int)
		p.add_argument("--weekday", choices=("MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY", "SUNDAY"))

	p = sub.add_parser("m5a-spawns", help="npc and gatherable spots around a point after spawnAll (m5a-plan.md 5.5)")
	data_args(p, country=False)
	p.add_argument("--map", type=int, required=True)
	p.add_argument("--x", type=float, required=True)
	p.add_argument("--y", type=float, required=True)
	p.add_argument("--z", type=float, required=True)
	p.add_argument("--radius", type=float, default=100.0)
	clock_args(p)
	p.set_defaults(fn=cmd_m5a_spawns)

	p = sub.add_parser("m5a-border-target", help="region move target with appearing and disappearing npcs (m5a-plan.md 5.6)")
	data_args(p, country=False)
	p.add_argument("--map", type=int, required=True)
	p.add_argument("--x", type=float, required=True)
	p.add_argument("--y", type=float, required=True)
	p.add_argument("--z", type=float, required=True)
	clock_args(p)
	p.set_defaults(fn=cmd_m5a_border_target)

	p = sub.add_parser("m5a-creation", help="spawn point, items, skills and base HP/MP of a new character (m5a-plan.md 5.3)")
	data_args(p, country=False)
	p.add_argument("--java-src", help="game-server/src (default: two levels above the static data directory, then src)")
	p.add_argument("--race", required=True, choices=("ELYOS", "ASMODIANS"))
	p.add_argument("--class", dest="player_class", required=True)
	p.set_defaults(fn=cmd_m5a_creation)

	p = sub.add_parser("m5b-monster", help="template, spawn spots, respawn time, experience reward and attack ranges of one npc (m5b-plan.md G-01)")
	data_args(p, country=False)
	p.add_argument("--java-src", help="game-server/src (default: two levels above the static data directory, then src)")
	p.add_argument("--java-handlers", help="game-server/data/handlers (default: beside the static data directory), scanned for @InstanceID(map)")
	p.add_argument("--map", type=int, required=True)
	p.add_argument("--npc-id", type=int, required=True, dest="npc_id")
	p.add_argument("--player-level", type=int, default=1, dest="player_level", help="the level of the character that gets the kill (default 1)")
	p.add_argument("--race", choices=("ELYOS", "ASMODIANS"), help="whose spawn point the spot distances are measured from (default: the map's "
	                                                              "world_type)")
	p.add_argument("--class", dest="player_class", default="WARRIOR", help="the attacking character's class (default WARRIOR)")
	p.add_argument("--xp-solo-rate", type=float, default=1.0, dest="xp_solo_rate",
	               help="RatesConfig.XP_SOLO_RATES[0], i.e. gameserver.rates.xp.solo (default 1.0, the M5b gate profile)")
	clock_args(p)
	p.set_defaults(fn=cmd_m5b_monster)

	p = sub.add_parser("m5b2-skills", help="the skills of a fresh character and the template constants the M5b-2 gate asserts (m5b2-plan.md G-01)")
	data_args(p, country=False)
	p.add_argument("--java-src", help="game-server/src (default: two levels above the static data directory, then src)")
	p.add_argument("--race", required=True, choices=("ELYOS", "ASMODIANS"))
	p.add_argument("--class", dest="player_class", required=True, help="a starting class (CM_CREATE_CHARACTER refuses the others)")
	p.add_argument("--level", type=int, default=1, help="the character level whose autolearn skills are reported, 1..9 (default 1)")
	p.add_argument("--skill", action="append", metavar="ID[:LEVEL]",
	               help="a further skill to report, e.g. one the gate seeds into player_skills (default level: the template's lvl); repeatable")
	p.add_argument("--npc", action="append", type=int, metavar="ID", help="an npc whose npc_skills list to report; repeatable")
	p.add_argument("--death-count", type=int, default=1, dest="death_count",
	               help="the deathCount updateSoulSickness casts skill 8291 at, i.e. its skill level (default 1: the first death)")
	p.set_defaults(fn=cmd_m5b2_skills)

	p = sub.add_parser("m5b3-drops", help="what a killed npc can drop: rules, chances, candidates, counts, kinah and the entry count (m5b3-plan.md G-01)")
	data_args(p, country=False)
	p.add_argument("--java-src", help="game-server/src (default: two levels above the static data directory, then src)")
	p.add_argument("--java-handlers", help="game-server/data/handlers: the AI classes, quest handlers and @InstanceID handlers (default: beside "
	                                       "--java-src)")
	p.add_argument("--npc", type=int, metavar="ID", help="the killed npc (a regular spawn)")
	p.add_argument("--map", type=int, metavar="ID", help="the npc's map (default: the one map with a regular spawn of it); with --survey, the map")
	p.add_argument("--survey", action="store_true", help="one summary row per npc id spawned on --map (210010000 Poeta, 220010000 Ishalgen)")
	p.add_argument("--player-level", type=int, default=1, dest="player_level",
	               help="the killer's level when registerDrop runs, i.e. after the kill's XP (default 1; from 10 on, repose 0 is an assumption)")
	p.add_argument("--race", choices=("ELYOS", "ASMODIANS"), help="the killer's race (default: the map's world_type)")
	p.add_argument("--drop-rate", dest="drop_rate", metavar="R",
	               help="the gameserver.rates.drop value of the killer's membership (default: RatesConfig.DROP_RATES[0], 1.0; the M5b-3 gate "
	                    "forces drops with a large one, the M5b/M5b-2 gates switch them off with 0)")
	p.add_argument("--inventory", action="append", metavar="ITEM[:COUNT]",
	               help="one stack in the looter's cube before the loot (COUNT default 1; repeat an id for several stacks): the `cube` budget "
	                    "counts the entries that merge into it")
	p.add_argument("--cube-expansions", type=int, default=0, dest="cube_expansions",
	               help="the cube's npc + quest + item expansions, 9 slots each (default 0: a fresh character's 27 slots)")
	p.set_defaults(fn=cmd_m5b3_drops)

	p = sub.add_parser("m5b3-item", help="an item's actions -> skill -> effect classes and values, its use delay and mask flags, and a godstone's "
	                                     "proc skill; or, with --survey, the effect classes of the starter and droppable skilluse items, every "
	                                     "godstone and every material skill (m5b3-plan.md G-01, §2.5-§2.6)")
	data_args(p, country=False)
	p.add_argument("--java-src", help="game-server/src (default: two levels above the static data directory, then src)")
	p.add_argument("--java-handlers", help="game-server/data/handlers, for the drop survey's AI classes and @InstanceID handlers (default: beside "
	                                       "the static data directory)")
	p.add_argument("--item", type=int, action="append", metavar="ID", help="an item template id; repeatable")
	p.add_argument("--survey", action="store_true", help="the survey of m5b3/survey.py over the droppable items of the --map(s)")
	p.add_argument("--map", type=int, action="append", metavar="ID", help="with --survey: a map whose droppable items are surveyed; repeatable")
	p.set_defaults(fn=cmd_m5b3_item)

	p = sub.add_parser("m5b3-material", help="the skill materials of a map: mesh material zones with their areas and skills, nearest first, and the "
	                                         "terrain materials (m5b3-plan.md G-01, §2.6)")
	data_args(p, country=False)
	p.add_argument("--geo-dir", dest="geo_dir", help="game-server/data/geo (default: beside the static data directory)")
	p.add_argument("--map", type=int, required=True, metavar="ID")
	p.add_argument("--near", metavar="X,Y,Z", help="sort the zones by their distance from this point (e.g. the race's spawn point)")
	p.add_argument("--radius", type=float, help="only the zones whose center is within this distance of --near")
	p.add_argument("--limit", type=int, help="list at most this many zones (the counts cover all)")
	p.add_argument("--stand", action="store_true",
	               help="with --near: where a player stands so that the nearest unconditional zone is the only one whose TOUCH check passes, a "
	                    "step-off point outside every zone and an untouched point inside that zone alone where every TOUCH ray misses (the `stand` "
	                    "section, m5b3-plan.md G-04, §18.2)")
	p.add_argument("--bound-upper", type=float, dest="bound_upper", default=None,
	               help="the player's BoundRadius.upper for the TOUCH ray (default 1.75: PlayerAppearance height 1.0 x 1.75)")
	p.set_defaults(fn=cmd_m5b3_material)

	p = sub.add_parser("m5c-trade", help="a merchant's goods with their buy prices, what it pays for a sold item, and a map's merchants (m5c-plan.md G-01)")
	data_args(p, country=False)
	p.add_argument("--java-src", help="game-server/src (default: two levels above the static data directory, then src)")
	p.add_argument("--config", help="game-server/config, whose administration, main and network folders give the defaults (default: beside --java-src)")
	p.add_argument("--profile", help="the override file Config.loadProperties reads (default: <config>/mygs.properties, which may be missing; a file "
	                                 "named here must exist)")
	p.add_argument("--no-profile", action="store_true", dest="no_profile", help="read no override file: the default folders and --set only")
	p.add_argument("--set", action="append", metavar="KEY=VALUE", help="a property read as one line of the profile (its trailing white space "
	                                                                   "stays), e.g. the gate's keys; repeatable. gameserver.country.code picks the "
	                                                                   "goodslists region variant")
	p.add_argument("--java-handlers", help="game-server/data/handlers, whose AI classes answer a dialog before DialogService (default: beside "
	                                       "--java-src)")
	p.add_argument("--npc", type=int, metavar="ID", help="a merchant: its buy window, goods, prices and sell behaviour")
	p.add_argument("--item", type=int, metavar="ID", help="one item: at --npc, or alone with every npc that sells or purchases it")
	p.add_argument("--map", type=int, metavar="ID", help="every npc of the map with a trade function (210010000 Poeta, 220010000 Ishalgen)")
	p.add_argument("--count", type=int, default=1, help="the count of one CM_BUY_ITEM entry the prices are computed for, 1..20000 (default 1)")
	p.add_argument("--race", action="append", choices=("ELYOS", "ASMODIANS"), help="the buyer's race (default: both); repeatable")
	p.add_argument("--influence", action="append", metavar="RACE=N", help="Influence.getInfluence(race) when sieges are on; repeatable")
	p.add_argument("--legion-level", type=int, default=0, dest="legion_level", help="the buyer's legion level, 0 without a legion (default 0)")
	p.add_argument("--account-max-level", type=int, default=1, dest="account_max_level",
	               help="Account.getMaxPlayerLevel for the sell limit when gameserver.limits.enable is on (default 1)")
	p.add_argument("--membership", type=int, default=0, help="the account membership that picks the sell limit rate (default 0)")
	p.set_defaults(fn=cmd_m5c_trade)

	p = sub.add_parser("m5c-craft", help="a recipe's materials, products, procs, task timing and skill-up; a profession skill at a level; a gatherable "
	                                     "(m5c-plan.md §2.6 G-01)")
	data_args(p, country=False)
	p.add_argument("--java-src", help="game-server/src (default: two levels above the static data directory, then src)")
	p.add_argument("--config", help="game-server/config, whose administration, main and network folders give the defaults (default: beside --java-src)")
	p.add_argument("--profile", help="the override file Config.loadProperties reads (default: <config>/mygs.properties; a missing file is no error)")
	p.add_argument("--no-profile", action="store_true", dest="no_profile", help="read no override file: the default folders and --set only")
	p.add_argument("--set", action="append", metavar="KEY=VALUE",
	               help="a property as if written in the profile, e.g. the gate's gameserver.craft.fail.chance=0; repeatable")
	p.add_argument("--recipe", type=int, metavar="ID", help="a recipe_template id, e.g. 155001381 (Roast Inina)")
	p.add_argument("--skill", type=int, metavar="ID", help="a profession skill: 30001-30003 gathering, 40001-40010 crafting, 40009 morph (with --level)")
	p.add_argument("--level", type=int, metavar="N", help="the skill level of --skill (0: not learned yet, the master's first price)")
	p.add_argument("--gatherable", type=int, metavar="ID", help="a gatherable_template id, e.g. 400601 (Young Aria, Poeta)")
	p.add_argument("--skill-level", type=int, dest="skill_level", metavar="N",
	               help="the crafter's or gatherer's skill level (default: the recipe's skillpoint or the gatherable's skillLevel)")
	p.add_argument("--craft-type", type=int, default=0, dest="craft_type", help="CM_CRAFT's craftType: 1 uses the bonus item for +15%% xp (default 0)")
	p.add_argument("--skill-xp", type=int, default=0, dest="skill_xp", help="the skill's current xp before the craft or gather (default 0)")
	p.add_argument("--character-level", type=int, default=1, dest="character_level", help="the character level, for a gatherable's lvlLimit (default 1)")
	p.add_argument("--map", type=int, metavar="ID", help="with --skill of a gathering skill: the gatherables spawned on this map (210010000 Poeta, "
	                                                     "220010000 Ishalgen)")
	p.add_argument("--membership", type=int, default=0, help="the account membership that picks the rate of every float[] rate key (default 0)")
	p.set_defaults(fn=cmd_m5c_craft)

	p = sub.add_parser("m5c-economy", help="the M5c gate's talk spots and windows, soul healing, cube, manastone removal, mail commission and the "
	                                       "extraction, identification and equip facts of items (m5c-plan.md G-01)")
	data_args(p, country=False)
	p.add_argument("--java-src", help="game-server/src (default: two levels above the static data directory, then src)")
	p.add_argument("--java-handlers", help="game-server/data/handlers, whose GeneralNpcAI and PostboxAI answer DIALOG_START (default: beside --java-src)")
	p.add_argument("--commons-src", dest="commons_src", help="commons/src, for Rnd.get (default: beside the game-server tree)")
	p.add_argument("--config", help="game-server/config, whose administration, main and network folders give the defaults (default: beside --java-src)")
	p.add_argument("--profile", help="the override file Config.loadProperties reads (default: <config>/mygs.properties, which may be missing; a file "
	                                 "named here must exist)")
	p.add_argument("--no-profile", action="store_true", dest="no_profile", help="read no override file: the default folders and --set only")
	p.add_argument("--set", action="append", metavar="KEY=VALUE", help="a property read as one line of the profile, e.g. the gate's keys; repeatable")
	p.add_argument("--influence", action="append", metavar="RACE=N", help="Influence.getInfluence(race) when sieges are on; repeatable")
	p.add_argument("--map", type=int, default=210010000, metavar="ID", help="the map whose spots the --npc talk blocks use (default 210010000 Poeta)")
	p.add_argument("--npc", type=int, action="append", metavar="ID",
	               help="an npc to talk to: its spots, the X2 band spot, a near and a far spot, the window it opens and its function arms; the first "
	                    "one's spot is the reference the others' spots are chosen by (nearest); repeatable")
	p.add_argument("--near", metavar="X,Y,Z", help="the reference point for choosing each npc's spot (default: the first --npc's first fixed spot)")
	p.add_argument("--far", type=float, default=10.0, help="the distance of the far spot from each npc, outside its talk range (default 10)")
	p.add_argument("--direction", type=float, default=0.0, help="the angle in degrees, counter-clockwise from +x, along which each npc's band, near and "
	                                                            "far spots lie (default 0)")
	p.add_argument("--recover-exp", type=int, dest="recover_exp", metavar="N", help="the recoverable exp at the soul healer: RECOVERY's price and question")
	p.add_argument("--npc-expands", type=int, default=0, dest="npc_expands", help="the cube's npc expansions before a cube expander's question (default 0)")
	p.add_argument("--quest-expands", type=int, default=0, dest="quest_expands", help="the cube's quest expansions (default 0)")
	p.add_argument("--item-expands", type=int, default=0, dest="item_expands", help="the cube's item expansions (default 0)")
	p.add_argument("--mail", action="append", metavar="ITEM:COUNT:KINAH[:express]",
	               help="a letter's attachment (ITEM 0 for none) and kinah: its commission and what the sender pays; repeatable")
	p.add_argument("--item", type=int, action="append", metavar="ID",
	               help="an item: extraction (breakItem), identification (tuning) and whether --class at --level may equip it; repeatable")
	p.add_argument("--class", dest="player_class", default="MAGE", help="the class the --item equip checks are made for (default MAGE)")
	p.add_argument("--race", default="ELYOS", choices=("ELYOS", "ASMODIANS"),
	               help="the character race of the --item equip checks: the item's race and the skill_tree rows it learns (default ELYOS; the "
	                    "price blocks are given for both races)")
	p.add_argument("--level", type=int, default=1, help="the character level of the equip checks (default 1)")
	p.add_argument("--manastone", type=int, action="append", metavar="ID",
	               help="a manastone each --item is tried with: CM_MANASTONE arm 2 on the item without stones (`socketing`); repeatable")
	p.add_argument("--membership", type=int, default=0, help="the account membership that picks a float[] rate (default 0)")
	p.add_argument("--daeva", metavar="CLASS", help="the C19 Daeva seed of an advanced class of --race: its level and what its enter world learns")
	p.add_argument("--daeva-old-level", type=int, default=1, dest="daeva_old_level",
	               help="players.old_level of the Daeva seed, the level its last quit stored (default 1)")
	p.add_argument("--craft-recipe", type=int, dest="craft_recipe", metavar="ID",
	               help="the C19 craft: the recipe's master, component vendors and exact kinah on --craft-map, and the spots at --craft-tool")
	p.add_argument("--craft-map", type=int, default=110010000, dest="craft_map", metavar="ID", help="the map of the C19 craft (default 110010000 Sanctum)")
	p.add_argument("--craft-tool", type=int, dest="craft_tool", metavar="ID", help="the item template id of the static object crafted at (e.g. 150000009)")
	p.add_argument("--craft-distance", type=float, action="append", dest="craft_distance", metavar="D",
	               help="a distance from the tool the character sends CM_CRAFT from (default 3, 7 and 12); repeatable")
	p.set_defaults(fn=cmd_m5c_economy)

	def m5d_args(p):
		data_args(p, country=False)
		p.add_argument("--java-src", help="game-server/src (default: two levels above the static data directory, then src)")
		p.add_argument("--java-handlers", help="game-server/data/handlers/quest (default: beside src)")
		p.add_argument("--config", help="game-server/config, for the rate defaults (default: beside src)")
		p.add_argument("--profile", help="the override file Config.loadProperties reads over config/{administration,main,network} (default: "
		                                 "<config>/mygs.properties when it exists; a file named here must exist)")
		p.add_argument("--no-profile", action="store_true", dest="no_profile", help="read no override file: the default folders only")
		p.add_argument("--race", choices=("ELYOS", "ASMODIANS"))
		p.add_argument("--class", dest="player_class", default="WARRIOR", help="the character's class (default WARRIOR)")
		p.add_argument("--gender", choices=("MALE", "FEMALE"), help="needed only when a quest in question has gender_permitted")
		p.add_argument("--completed", nargs="+", action="extend", metavar="ID[:GROUP]",
		               help="quests the character has completed once (reward group GROUP, default 0 when it has rewards)")
		p.add_argument("--inventory", nargs="+", action="extend", metavar="ITEM[:COUNT]",
		               help="items in the cube (COUNT default 1; 0 states that the cube does not hold it), needed for an inventory_items check "
		                    "on an item a quest of the list may have given or taken")

	p = sub.add_parser("m5d-quest", help="one quest: handler, prerequisites, steps as QuestState changes, targets, rewards after rates, follow-up "
	                                     "(m5d-plan.md G-01)")
	m5d_args(p)
	p.add_argument("--quest", type=int, required=True)
	p.add_argument("--level", type=int, help="the character level before the reward, for the follow-up (default: the level of --exp, else the "
	                                         "quest's minlevel_permitted, at least 1); the follow-up is checked at the level after the reward")
	p.add_argument("--exp", type=int, help="the character's exp before the reward (default: the start exp of --level)")
	p.set_defaults(fn=cmd_m5d_quest)

	p = sub.add_parser("m5d-quests", help="the quests a character meets on a map and the SM_NEARBY_QUESTS set, XML-only registry marked "
	                                      "(m5d-plan.md G-01, D9)")
	m5d_args(p)
	mode = p.add_mutually_exclusive_group(required=True)
	mode.add_argument("--map", type=int)
	mode.add_argument("--registration-order", action="store_true", dest="registration_order",
	                  help="QuestEngine.init's XML registration order and the lists it builds: questOnEnterWorld, questOnLevelUp and each npc's "
	                       "onQuestStart/onTalkEvent/onKillEvent (m5d-plan.md T-04)")
	mode.add_argument("--census", action="store_true", help="m5d-plan.md §2.3-§2.5: the handlers, where each XML quest starts, what "
	                                                         "completing the reachable ones needs, E-09's reward bodies, the start zones")
	p.add_argument("--npc", type=int, metavar="ID", help="with --registration-order: only this npc's lists, with their order flags")
	p.add_argument("--level", type=int, help="the character level (default 1)")
	p.add_argument("--started", nargs="+", action="extend", type=int, metavar="ID", help="quests the character has in START state")
	clock_args(p)
	p.set_defaults(fn=cmd_m5d_quests, player_class=None)  # --class: WARRIOR in the --map path; the other modes refuse it

	p = sub.add_parser("m5e-progression", help="levels, the class change, the skills each step teaches with their message ids, and the gate's "
	                                           "casts and weapons (m5e-plan.md G-01)")
	data_args(p, country=False)
	p.add_argument("--java-src", help="game-server/src (default: two levels above the static data directory, then src)")
	p.add_argument("--java-handlers", help="game-server/data/handlers/quest (default: beside static_data)")
	p.add_argument("--race", required=True, choices=("ELYOS", "ASMODIANS"))
	p.add_argument("--class", dest="player_class", required=True, help="the character's class at the first step")
	p.add_argument("--level", type=int, default=1, help="the character's level at the first step (default 1)")
	p.add_argument("--daeva", action="store_true", help="the character is a Daeva at the first step")
	p.add_argument("--known-skills", dest="known_skills", metavar="FILE",
	               help="the skill list at the first step: JSON [{skillId, level}] or `id[:level]` tokens (e.g. the enter world's SM_SKILL_LIST)")
	p.add_argument("--step", nargs="+", action="extend", metavar="STEP", help="create, enter:L, level:L, class:CLASS, action:ID, quit, seed:CLASS, book:ITEM")
	p.add_argument("--skill", nargs="+", action="extend", type=int, metavar="ID", help="skills whose constants and conditions to report")
	p.add_argument("--weapon", nargs="+", action="extend", type=int, metavar="ID",
	               help="weapons (item ids): item group, required skills, robot id; the first is the main hand of the condition check")
	p.add_argument("--weapon-group", dest="weapon_group", help="the main hand's item group for the conditions (default: the first --weapon)")
	p.add_argument("--dp", type=int, default=0, help="the character's DP for DpCondition (default 0)")
	p.add_argument("--robot", action="store_true", help="the character rides a robot (RideRobotCondition)")
	p.add_argument("--chain-after", dest="chain_after", metavar="CATEGORY", help="the chain category the last chain skill left (ChainCondition)")
	p.add_argument("--target-kind", dest="target_kind", choices=("PC", "NPC"), help="what the first target is (TargetCondition)")
	p.add_argument("--stigma", nargs="+", action="extend", type=int, metavar="ITEM",
	               help="stigma stones equipped after the steps: their kinah price and the skills addStigmaSkills teaches")
	p.add_argument("--config", help="game-server/config, for the prices of --stigma (default: beside src)")
	p.add_argument("--profile", help="the override file over config/{administration,main,network} for --stigma's prices (default: "
	                                 "<config>/mygs.properties when it exists; a file named here must exist)")
	p.add_argument("--no-profile", action="store_true", dest="no_profile", help="read no override file for --stigma's prices")
	p.set_defaults(fn=cmd_m5e_progression)

	p = sub.add_parser("m5e-stumble", help="whether a stumble's segment is open ground in the geo data and, if so, the z GeoMap.getClosestCollision "
	                                       "gives its end (m5e-plan.md X9g)")
	data_args(p, country=False)
	p.add_argument("--geo-dir", dest="geo_dir", help="game-server/data/geo (default: beside the static data directory)")
	p.add_argument("--map", type=int, required=True, metavar="ID")
	p.add_argument("--stumble", action="append", required=True, metavar="FX,FY,FZ,TX,TY",
	               help="the npc's position before the hit and the stumble's end x, y; repeatable")
	p.set_defaults(fn=cmd_m5e_stumble)

	p = sub.add_parser("m5f-travel", help="teleporters, flight masters, hotspots, obelisks, portals, instance cooltimes, exits and spawns, the "
	                                      "experience table, the M5f census and the destinations' geo check (m5f-plan.md G-01)")
	data_args(p, country=False)
	p.add_argument("--java-src", help="game-server/src (default: two levels above the static data directory, then src)")
	p.add_argument("--java-handlers", help="game-server/data/handlers/quest, for --census's class change rules (default: beside static_data)")
	p.add_argument("--config", help="game-server/config, whose administration, main and network folders give the defaults (default: beside --java-src)")
	p.add_argument("--profile", help="the override file Config.loadProperties reads (default: <config>/mygs.properties; a missing file is no error)")
	p.add_argument("--no-profile", action="store_true", dest="no_profile", help="read no override file: the default folders only")
	p.add_argument("--npc", type=int, action="append", metavar="ID", help="a teleporter or flight master: spots, Daeva gate, locations and prices; repeatable")
	p.add_argument("--race", choices=("ELYOS", "ASMODIANS"), help="the player's race: --npc's prices (default: the npc's race), --portal, --instance-exit")
	p.add_argument("--hotspot", type=int, action="append", metavar="ID", help="a hotspot (CM_BIND_POINT_TELEPORT): price from --from; repeatable")
	p.add_argument("--from", dest="origin", action="append", metavar="X,Y,Z", help="the player's position for --hotspot (once, or once per --hotspot)")
	p.add_argument("--obelisk", type=int, action="append", metavar="NPCID", help="a bind point npc: spots and bind_points price; repeatable")
	p.add_argument("--portal", type=int, action="append", metavar="NPCID", help="a portal npc: use bar, paths, cooltime, reuse time, exit; repeatable")
	p.add_argument("--now-ms", type=int, dest="now_ms", metavar="EPOCH_MS", help="the server clock for --portal's reuse time (default: now)")
	p.add_argument("--tz", help="the server zone for --portal: local, UTC, +HH:MM or a zone name (default: gameserver.timezone, empty = local)")
	p.add_argument("--instance-exit", type=int, action="append", dest="instance_exit", metavar="WORLD", help="instance_exit of WORLD for --race; repeatable")
	p.add_argument("--instance-spawns", type=int, action="append", dest="instance_spawns", metavar="WORLD",
	               help="the spots SpawnEngine.spawnInstance places in WORLD within --radius of --near; repeatable")
	p.add_argument("--difficulty", type=int, default=0, help="spawnInstance's difficultId (default 0)")
	p.add_argument("--near", action="append", metavar="X,Y,Z", help="the center for --instance-spawns (once, or once per --instance-spawns)")
	p.add_argument("--radius", type=float, help="the radius for --instance-spawns (PositionUtil.isInRange: strictly inside)")
	p.add_argument("--game-minutes", type=int, dest="game_minutes", help="SM_GAME_TIME minutes for the temporary spawns of --instance-spawns")
	p.add_argument("--game-hour", type=int, dest="game_hour")
	p.add_argument("--game-day", type=int, dest="game_day")
	p.add_argument("--game-month", type=int, dest="game_month")
	p.add_argument("--weekday", choices=("MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY", "SUNDAY"))
	p.add_argument("--exp-for-level", type=int, dest="exp_for_level", metavar="L", help="PlayerExperienceTable.getStartExpForLevel(L)")
	p.add_argument("--census", action="store_true", help="W-14 (level-16 passives per class) and W-21 (Haramel's npc skills) vs the C++ tree")
	p.add_argument("--cpp-src", dest="cpp_src", metavar="DIR", help="cpp/game-server (or its src) for --census (default: the repository's)")
	p.add_argument("--geo-check", action="store_true", dest="geo_check", help="§10.5 G3: the destinations' data z vs the geo data's getZ")
	p.add_argument("--geo-dir", dest="geo_dir", help="game-server/data/geo for --geo-check (default: beside the static data directory)")
	p.add_argument("--geo-map", type=int, action="append", dest="geo_map", metavar="ID", help="limit --geo-check to these maps; repeatable")
	p.set_defaults(fn=cmd_m5f_travel)
	p = sub.add_parser("m5g-team", help="a team kill's experience shares in the gate's kill order, the level search of D8 and the team "
	                                    "constants (m5g-plan.md H-01)")
	data_args(p, country=False)
	p.add_argument("--java-src", help="game-server/src (default: two levels above the static data directory, then src)")
	p.add_argument("--npc-id", type=int, required=True, dest="npc_id")
	p.add_argument("--levels", type=int, nargs="+", help="the members' levels (default: search the smallest A = B < C of D8)")
	p.add_argument("--kill", nargs="+", action="extend", metavar="FLAGS",
	               help="one kill per value: a 0/1 string, member i counted (online and within range) when its character i is 1")
	p.add_argument("--xp-group-rate", type=float, default=1.0, dest="xp_group_rate", help="RatesConfig.XP_GROUP_RATES[0]")
	p.add_argument("--xp-solo-rate", type=float, default=1.0, dest="xp_solo_rate", help="RatesConfig.XP_SOLO_RATES[0]")
	p.add_argument("--max-level", type=int, default=20, dest="max_level", help="the level search's bound (default 20)")
	p.add_argument("--message", nargs="+", action="extend", metavar="NAME", help="SM_SYSTEM_MESSAGE ids to look up by name")
	p.add_argument("--question", nargs="+", action="extend", metavar="NAME", help="SM_QUESTION_WINDOW ids to look up by name")
	p.set_defaults(fn=cmd_m5g_team)
	p = sub.add_parser("m5h-legion", help="the legion gate's constants: message and question ids, the legion enums, the emblem chunk size and "
	                                      "the announcement limit (m5h-plan.md G-01)")
	data_args(p, country=False)
	p.add_argument("--java-src", help="game-server/src (default: two levels above the static data directory, then src)")
	p.add_argument("--message", nargs="+", action="extend", metavar="NAME", help="SM_SYSTEM_MESSAGE ids to look up by name")
	p.add_argument("--question", nargs="+", action="extend", metavar="NAME", help="SM_QUESTION_WINDOW ids to look up by name")
	p.set_defaults(fn=cmd_m5h_legion)
	p = sub.add_parser("m5h-housing", help="the studio cases' constants: npc spots and talk info, the Elyos studio, furniture templates, PartType, "
	                                       "HouseDoorState and message ids (m5h-plan.md G-01)")
	data_args(p, country=False)
	p.add_argument("--java-src", help="game-server/src (default: two levels above the static data directory, then src)")
	p.add_argument("--message", nargs="+", action="extend", metavar="NAME", help="SM_SYSTEM_MESSAGE ids to look up by name")
	p.add_argument("--npc", nargs="+", action="extend", metavar="MAP:NPC", help="an npc's first spawn spot on a map and its talk info")
	p.add_argument("--item", nargs="+", action="extend", type=int, metavar="ITEM", help="a furniture item and its house object template")
	p.set_defaults(fn=cmd_m5h_housing)

	p = sub.add_parser("quest-trace", help="golden traces of the Java quest handlers: every return leaf of every hook as a case with its "
	                                       "effects (questtrace/, phase6-inventory.md §7.6 item 3)")
	p.add_argument("action", choices=("generate", "check"))
	p.add_argument("--out", default=str(runner.EXPECTED_DIR / "quest"), help="generate: the output directory (default expected/quest)")
	p.add_argument("--expected-dir", default=str(runner.EXPECTED_DIR / "quest"), help="check: the committed traces (default expected/quest)")
	p.add_argument("--only", nargs="+", metavar="REL", help="handlers below data/handlers/quest instead of the slice (questtrace.extract.SLICE)")
	p.set_defaults(fn=cmd_quest_trace)

	args = parser.parse_args(argv)
	try:
		return args.fn(args)
	except OracleError as e:
		print(f"oracle error: {e}", file=sys.stderr)
		return 2


if __name__ == "__main__":
	# the answers are JSON with ensure_ascii=False (runner.dump_json), and a static-data name can hold a character outside ASCII (the M5b-3
	# godstone "Freyr's Esprit" of item_templates.xml): redirected on Windows, stdout would encode it in the ANSI code page, which the gate's JSON
	# parser (nlohmann, Oracle.cpp) refuses as ill-formed UTF-8 - so the command line answers in UTF-8 whatever the platform
	if hasattr(sys.stdout, "reconfigure"):
		sys.stdout.reconfigure(encoding="utf-8")
	sys.exit(main())
