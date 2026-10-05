"""m5g-team: what the M5g party gate needs to predict a team kill's experience, and the team constants it asserts (m5g-plan.md H-01, D8).

Java rules, each with the method the value comes from:
- the experience share: PlayerTeamDistributionService.doReward (PlayerTeamDistributionService.java:35-84). PlayerTeamRewardStats counts a
  member who is online and within GroupConfig.GROUP_MAX_DISTANCE (100, GroupConfig.java) of the npc; mentors are left out (no mentor here).
  expReward = StatFunctions.calculateExperienceReward(highestLevel, npc), the m5b-monster arithmetic with the HIGHEST counted level;
  rewardXp = Math.round(expReward * level / (float) partyLvlSum) - the long product converted to float, Math.round(float); 0 when the member is 10
  or more levels below the highest; `rewardXp *= damagePercent` (the long converted to float and narrowed back); then
  PlayerCommonData.addExp(rewardXp, Rates.XP_GROUP_HUNTING): (long) Math.min(xp * XP_GROUP_RATES[membership], expNeed * 0.2f) (Rates.java:21-27).
  A membership-0 account, no BOOST_GROUP_HUNTING_XP_RATE stat and no legion bonus: the rate is the profile's first group rate.
- the sequence: the gate kills in order, each kill adds its shares to the members' exp and a member levels up when his exp reaches the next
  level's start exp (PlayerCommonData.setExp), which changes his level and his cap for the next kill. The seeded exp is the start exp of the level.
- the level search of D8: the smallest levels A = B < C (C below A + 10) such that, for the gate's kills - (A, B, C), (A, B, C), then (A, B) -
  no share is at its member's cap, the level split differs from an even split (Math.round(reward / size)) for A and for C in every kill whose
  counted levels differ, and the group rate's
  value differs from the solo rate's. The search fails (OracleError) when no level pair below `max_level` qualifies.
- the constants: LootGroupRules' default rules (LootGroupRules.java:32-40), TeamType's words (TeamType.java:8-13), GroupEvent's ids
  (GroupEvent.java:8-15) and SM_SYSTEM_MESSAGE / SM_QUESTION_WINDOW ids by name, all read from the Java sources.
"""

from __future__ import annotations

import re
from pathlib import Path

from staticdata_oracle import OracleError

from m5a.creation import enum_constants
from m5a.data import StaticData, java_int
from m5a.javafloat import f32, round_to_int, to_long
from m5b.monster import JavaCombatRules, _java_long, base_exp, exp_need, experience_reward, npc_template

GROUP_MAX_DISTANCE_DEFAULT = 100


def _read(source: Path) -> str:
	try:
		return source.read_text(encoding="utf-8")
	except OSError as e:
		raise OracleError(f"{source}: {e}") from e


class TeamRules:
	"""The npc's experience reward per highest level and the experience table."""

	def __init__(self, data: StaticData, java_src: Path, npc_id: int):
		self.rules = JavaCombatRules.read(Path(java_src))
		self.template = npc_template(data, npc_id)
		self.multiplier = self.rules.rating_multiplier(self.template.rating, self.template.rank)
		self.base = base_exp(self.template.max_hp, self.multiplier)
		self.table = [_java_long(e.text, "player_experience_table exp") for e in data.children("player_experience_table", "exp")]

	def reward(self, highest_level: int) -> int:
		"""StatFunctions.calculateExperienceReward(highestLevel, npc), the open world arm"""
		return experience_reward(self.base, self.rules.exp_multiplier_open_world, self.rules.xp_reward_from(self.template.level - highest_level))


def _start_exp(table: list[int], level: int) -> int:
	"""PlayerExperienceTable.getStartExpForLevel: 0 for level 0, else table[level - 1] (level 1 starts at table[0] = 0)"""
	return 0 if level == 0 else table[level - 1]


def _level_of(table: list[int], exp: int) -> int:
	"""PlayerExperienceTable.getLevelForExp"""
	level = 0
	for i, start in enumerate(table):
		if exp >= start:
			level = i + 1
	return max(1, level)


def share(rules: TeamRules, levels: list[int], index: int, in_range: list[bool], rate: float, damage_percent: float = 1.0):
	"""One member's share of one kill (PlayerTeamDistributionService.java:35-84); None when he is not counted."""
	counted = [i for i, ok in enumerate(in_range) if ok]
	if index not in counted:
		return None
	highest = max(levels[i] for i in counted)
	level_sum = sum(levels[i] for i in counted)
	reward = rules.reward(highest)
	reward_xp = round_to_int(f32(f32(float(reward * levels[index])) / f32(float(level_sum))))
	if highest - levels[index] >= 10:
		reward_xp = 0
	reward_xp = to_long(f32(f32(float(reward_xp)) * f32(damage_percent)))
	need = exp_need(rules.table, levels[index])
	cap = f32(need * f32(0.2))
	value = f32(reward_xp * f32(rate))
	awarded = to_long(min(value, cap))
	even = round_to_int(f32(f32(float(reward)) / f32(float(len(counted)))))
	return {
		"level": levels[index],
		"highestLevel": highest,
		"partyLvlSum": level_sum,
		"experienceReward": reward,
		"rewardXp": reward_xp,
		"expNeed": need,
		"cap": cap,
		"capped": value >= cap,
		"awarded": awarded,
		# the two mutations of §10.4 the gate must tell apart: an even split, and the solo rate
		"evenSplitAwarded": to_long(min(f32(even * f32(rate)), cap)),
	}


def simulate(rules: TeamRules, start_levels: list[int], kills: list[list[bool]], group_rate: float, solo_rate: float) -> list[dict]:
	"""The gate's kills in order: each member's level, share and exp before and after (levels move with the exp)."""
	exps = [_start_exp(rules.table, level) for level in start_levels]
	results = []
	for kill in kills:
		levels = [_level_of(rules.table, e) for e in exps]
		members = []
		for i in range(len(levels)):
			value = share(rules, levels, i, kill, group_rate)
			if value is not None:
				solo = share(rules, levels, i, kill, solo_rate)
				value["soloRateAwarded"] = solo["awarded"]
				value["expBefore"] = exps[i]
			members.append(value)
		for i, value in enumerate(members):
			if value is not None:
				exps[i] += value["awarded"]
				value["expAfter"] = exps[i]
				value["levelAfter"] = _level_of(rules.table, exps[i])
		results.append({"inRange": kill, "members": members})
	return results


def acceptable(results: list[dict], checked: list[int]) -> list[str]:
	"""Why a simulated sequence does not prove the formula (empty: it does)"""
	problems = []
	for k, kill in enumerate(results):
		for i, member in enumerate(kill["members"]):
			if member is None:
				continue
			if member["capped"]:
				problems.append(f"kill {k + 1}: member {i} is at his cap")
			if member["awarded"] == member["soloRateAwarded"]:
				problems.append(f"kill {k + 1}: member {i}'s share is the same at the solo rate")
			levels = {m["level"] for m in kill["members"] if m is not None}
			if i in checked and len(levels) > 1 and member["awarded"] == member["evenSplitAwarded"]:
				problems.append(f"kill {k + 1}: member {i}'s share is the same as an even split")
	return problems


def search_levels(rules: TeamRules, group_rate: float, solo_rate: float, max_level: int = 20) -> tuple[int, int, list[dict]]:
	"""D8's search: the smallest A (= B) and then C"""
	kills = [[True, True, True], [True, True, True], [True, True, False]]
	for a in range(1, max_level):
		for c in range(a + 1, min(a + 10, max_level + 1)):
			results = simulate(rules, [a, a, c], kills, group_rate, solo_rate)
			if not acceptable(results, [0, 2]):
				return a, c, results
	raise OracleError(f"no levels A = B < C <= {max_level} make every share of the gate's kills uncapped and distinguishable")


def team_constants(java_src: Path, messages: list[str], questions: list[str]) -> dict:
	base = Path(java_src) / "com" / "aionemu" / "gameserver"
	rules_text = _read(base / "model" / "team" / "common" / "legacy" / "LootGroupRules.java")
	body = re.search(r"public\s+LootGroupRules\(\)\s*\{(.*?)\}", rules_text, re.S)
	if body is None:
		raise OracleError("LootGroupRules(): no default constructor")
	defaults = dict(re.findall(r"(\w+)\s*=\s*([\w.]+)\s*;", body.group(1)))
	rule_name = defaults.pop("lootRule").split(".")[-1]
	loot_rules = [(name, java_int(args, f"LootRuleType.{name}")) for name, args in enum_constants(base / "model" / "team" / "common" / "legacy" /
	                                                                                                   "LootRuleType.java", "LootRuleType")]
	loot_rule_ids = dict(loot_rules)
	words = {"lootRule": loot_rule_ids[rule_name], "misc": 0}
	for field in ("commonItemAbove", "superiorItemAbove", "heroicItemAbove", "fabledItemAbove", "eternalItemAbove", "mythicItemAbove"):
		words[field] = java_int(defaults.get(field, "0"), f"LootGroupRules.{field}")
	team_types = {}
	for name, args in enum_constants(base / "model" / "team" / "TeamType.java", "TeamType"):
		type_word, sub_type = [a.strip() for a in args.split(",")]
		team_types[name] = {"type": int(type_word, 16) if type_word.lower().startswith("0x") else int(type_word), "subType": int(sub_type)}
	group_events = {name: java_int(args, f"GroupEvent.{name}") for name, args in enum_constants(base / "model" / "team" / "common" / "legacy" /
	                                                                                              "GroupEvent.java", "GroupEvent")}
	commands = {name: java_int(args, f"TeamCommand.{name}") for name, args in enum_constants(base / "model" / "team" / "common" / "events" /
	                                                                                          "TeamCommand.java", "TeamCommand")}
	system = _read(base / "network" / "aion" / "serverpackets" / "SM_SYSTEM_MESSAGE.java")
	message_ids = {}
	for name in messages:
		found = re.search(rf"public\s+static\s+SM_SYSTEM_MESSAGE\s+{name}\([^)]*\)\s*\{{\s*return\s+new\s+SM_SYSTEM_MESSAGE\((\d+)", system)
		if found is None:
			raise OracleError(f"SM_SYSTEM_MESSAGE.{name}: not found")
		message_ids[name] = int(found.group(1))
	question = _read(base / "network" / "aion" / "serverpackets" / "SM_QUESTION_WINDOW.java")
	question_ids = {}
	for name in questions:
		found = re.search(rf"public\s+static\s+final\s+int\s+{name}\s*=\s*(\d+)\s*;", question)
		if found is None:
			raise OracleError(f"SM_QUESTION_WINDOW.{name}: not found")
		question_ids[name] = int(found.group(1))
	group_config = _read(base / "configs" / "main" / "GroupConfig.java")
	distance = re.search(r'gameserver\.playergroup\.maxdistance",\s*defaultValue\s*=\s*"(\d+)"', group_config)
	return {
		"lootGroupRulesDefault": words,
		"lootRuleIds": loot_rule_ids,
		"teamTypes": team_types,
		"groupEvents": group_events,
		"teamCommands": commands,
		"messages": message_ids,
		"questions": question_ids,
		"groupMaxDistance": int(distance.group(1)) if distance else GROUP_MAX_DISTANCE_DEFAULT,
	}


def team_report(data: StaticData, java_src: Path, npc_id: int, levels: list[int] | None, kills: list[list[bool]] | None, group_rate: float,
                solo_rate: float, messages: list[str], questions: list[str], max_level: int = 20) -> dict:
	rules = TeamRules(data, java_src, npc_id)
	if levels is None:
		a, c, results = search_levels(rules, group_rate, solo_rate, max_level)
		levels = [a, a, c]
		kills = [[True, True, True], [True, True, True], [True, True, False]]
	else:
		kills = kills or [[True] * len(levels)]
		results = simulate(rules, levels, kills, group_rate, solo_rate)
	return {
		"format": "aion-m5g-team",
		"version": 1,
		"npcId": npc_id,
		"npcLevel": rules.template.level,
		"baseExp": rules.base,
		"groupRate": group_rate,
		"soloRate": solo_rate,
		"levels": levels,
		"startExp": [_start_exp(rules.table, level) for level in levels],
		"kills": results,
		"problems": acceptable(results, [0, len(levels) - 1]),
		"constants": team_constants(java_src, messages, questions),
	}
