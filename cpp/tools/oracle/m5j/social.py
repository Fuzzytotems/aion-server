"""m5j-social: what the M5j stage-1 gate (m5j-plan.md §10.4, gs.scenario.m5j Z1-Z14) asserts about the social, punishment and PvP paths, as
the Java server computes it (m5j-plan.md §18.1 CP2 "the oracle's PvP AP formulas (m5j-social, H-11)").

Java rules, each with the method the value comes from (game-server/src/com/aionemu/gameserver):
- message and question ids by name: the `return new SM_SYSTEM_MESSAGE(<id>` of SM_SYSTEM_MESSAGE.NAME and SM_QUESTION_WINDOW.NAME's constant;
- the configuration: Config.loadProperties' layering with the @Property defaults (m5c/trade_config.load_config) for CustomConfig
  LEVEL_TO_WHISPER, LEVEL_TO_SEARCH, FACTIONS_SEARCH_MODE, SEARCH_GM_LIST, MAX_DAILY_PVP_KILLS and ENABLE_AP_CAP (a cap is refused) and
  RatesConfig AP_PVP_RATES, AP_PVP_LOSS_RATES;
  Rates.get(player, rates) picks rates[min(length - 1, membership)];
- the Daeva seed: PlayerCommonData.updateDaeva's ascension quests and setExp's level cap (m5c/sanctum.JavaC19Rules), the exp of a level
  PlayerExperienceTable.getStartExpForLevel (table[level - 1]) and the level it loads at (sanctum.level_at_load);
- the titles: player_titles.xml's <title id nameId race> in file order (TitleList.addTitle refuses a title of the other race that is not
  PC_ALL; TitleTemplate.getL10n is ChatUtil.l10n(nameId));
- AbyssRankEnum (utils/stats/AbyssRankEnum.java): (id, pointsGained, pointsLost, requiredAP, requiredGP) per rank and getRankForPoints(ap, gp):
  the last rank in declaration order whose requiredAP <= ap and requiredGP <= gp;
- one solo PvP kill (PvpService.doReward with the winner the only attacker, of another race, in range, alive, no team, apWinMulti 1):
  apLost = Rates.AP_PVP_LOST.calcResult(victim, StatFunctions.calculatePvPApLost(victim, winner)) - the level penalty of
  `winner.getLevel() - defeated.getLevel()` (>= 5: Math.round(p * 0.1f), 4: 0.65f, 3: 0.85f) and (long) (p * rate);
  apActuallyLost = apLost * apRelevantDamage / totalDamage = apLost (all damage was the winner's);
  apGained: baseApReward = calculatePvpApGained(victim, maxRank = max(1, winner rank id), maxLevel = winner level) * 1f, the level arm of
  `maxLevel - defeated.getLevel()` (> 4: 0.1f, < -3: 1.3f, 3: 0.85f, 4: 0.65f, -2: 1.1f, -3: 1.2f) and the rank penalty
  (winnerRank <= 7 and winnerRank - defeatedRank > 0: p -= Math.round(p * (difference * 0.05f))); apRewardPerMember = Math.round(base *
  (damage / (float) total) / 1) = Math.round(base); then, while KillCounter.addKillFor(...) < MAX_DAILY_PVP_KILLS (the first kill counts 1),
  Rates.AP_PVP.calcResult(member, ap) = (long) (ap * rate * (AP_BOOST / 100f)) with the base AP_BOOST of 100; else 1;
  AbyssRank.addAp: the cap (ENABLE_AP_CAP), currentAp + amount floored at 0, the rank from getRankForPoints; SM_ABYSS_RANK carries the new AP.
"""

from __future__ import annotations

import re
from pathlib import Path

from staticdata_oracle import OracleError

from m5a.creation import enum_constants
from m5a.data import StaticData, java_int
from m5a.javafloat import f32, round_to_int, to_long
from m5b.monster import _java_long
from m5c.sanctum import JavaC19Rules, level_at_load
from m5c.trade_config import load_config

# key -> (Config class below configs/main, field, Java type), read the way trade_config reads its keys
SOCIAL_KEYS = {
	"gameserver.chat.whisper.level": ("CustomConfig", "LEVEL_TO_WHISPER", "int"),
	"gameserver.search.player.level": ("CustomConfig", "LEVEL_TO_SEARCH", "int"),
	"gameserver.search.factions.mode": ("CustomConfig", "FACTIONS_SEARCH_MODE", "boolean"),
	"gameserver.search.gm.list": ("CustomConfig", "SEARCH_GM_LIST", "boolean"),
	"gameserver.pvp.maxkills": ("CustomConfig", "MAX_DAILY_PVP_KILLS", "int"),
	"gameserver.enable.ap.cap": ("CustomConfig", "ENABLE_AP_CAP", "boolean"),
	"gameserver.rates.ap.pvp.gain": ("RatesConfig", "AP_PVP_RATES", "float[]"),
	"gameserver.rates.ap.pvp.loss": ("RatesConfig", "AP_PVP_LOSS_RATES", "float[]"),
}

# statements the arithmetic below was written against (whitespace-insensitive): a change in the Java source fails the oracle
STATEMENTS = (
	("utils/stats/StatFunctions.java", "int pointsLost = defeated.getAbyssRank().getRank().getPointsLost();"),
	("utils/stats/StatFunctions.java", "int difference = winner.getLevel() - defeated.getLevel();"),
	("utils/stats/StatFunctions.java", "if (difference >= 5) pointsLost = Math.round(pointsLost * 0.1f); else if (difference == 4) pointsLost = "
	                                   "Math.round(pointsLost * 0.65f); else if (difference == 3) pointsLost = Math.round(pointsLost * 0.85f);"),
	("utils/stats/StatFunctions.java", "return Rates.AP_PVP_LOST.calcResult(defeated, pointsLost);"),
	("utils/stats/StatFunctions.java", "int pointsGained = defeated.getAbyssRank().getRank().getPointsGained();"),
	("utils/stats/StatFunctions.java", "if (winnerAbyssRank <= 7 && abyssRankDifference > 0) { float penaltyPercent = abyssRankDifference * 0.05f; "
	                                   "pointsGained -= Math.round(pointsGained * penaltyPercent); }"),
	("services/PvpService.java", "float baseApReward = StatFunctions.calculatePvpApGained(victim, maxRank, maxLevel) * apWinMulti;"),
	("services/PvpService.java", "int apRewardPerMember = Math.round(baseApReward * groupDamagePercentage / players.size());"),
	("services/PvpService.java", "if (KillCounter.addKillFor(member.getObjectId(), victim.getObjectId()) < CustomConfig.MAX_DAILY_PVP_KILLS) {"),
	("services/PvpService.java", "memberApGain = Rates.AP_PVP.calcResult(member, apRewardPerMember);"),
	("services/PvpService.java", "final int apActuallyLost = apLost * apRelevantDamage / totalDamage;"),
	("model/gameobjects/player/Rates.java", "return (long) (ap * get(player, RatesConfig.AP_PVP_RATES) * statRate);"),
	("model/gameobjects/player/Rates.java", "return (long) (ap * get(player, RatesConfig.AP_PVP_LOSS_RATES));"),
	("model/gameobjects/player/Rates.java", "return membershipRates[Math.min(membershipRates.length - 1, membershipLevel)];"),
	("model/gameobjects/player/AbyssRank.java", "currentAp += cappedCount; if (currentAp < 0) currentAp = 0;"),
)

GAINED_LEVEL_FACTORS = {3: 0.85, 4: 0.65, -2: 1.1, -3: 1.2}  # calculatePvpApGained's switch


def _read(path: Path) -> str:
	try:
		return path.read_text(encoding="utf-8")
	except OSError as e:
		raise OracleError(f"{path}: {e}") from e


def _squeeze(text: str) -> str:
	text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
	text = re.sub(r"//[^\n]*", "", text)
	return re.sub(r"\s+", "", text)


def check_statements(java_src: Path) -> None:
	base = Path(java_src) / "com" / "aionemu" / "gameserver"
	cache: dict[str, str] = {}
	for relative, statement in STATEMENTS:
		if relative not in cache:
			cache[relative] = _squeeze(_read(base / relative))
		if _squeeze(statement) not in cache[relative]:
			raise OracleError(f"{relative} no longer contains `{statement}`: the Java source does not have the shape this oracle was written against")


def message_ids(java_src: Path, messages: list[str], questions: list[str]) -> tuple[dict[str, int], dict[str, int]]:
	base = Path(java_src) / "com" / "aionemu" / "gameserver" / "network" / "aion" / "serverpackets"
	system = _read(base / "SM_SYSTEM_MESSAGE.java")
	found_messages = {}
	for name in messages:
		found = re.search(rf"public\s+static\s+SM_SYSTEM_MESSAGE\s+{re.escape(name)}\([^)]*\)\s*\{{\s*return\s+new\s+SM_SYSTEM_MESSAGE\((\d+)", system)
		if found is None:
			raise OracleError(f"SM_SYSTEM_MESSAGE.{name}: not found")
		found_messages[name] = int(found.group(1))
	question = _read(base / "SM_QUESTION_WINDOW.java")
	found_questions = {}
	for name in questions:
		found = re.search(rf"public\s+static\s+final\s+int\s+{re.escape(name)}\s*=\s*(\d+)\s*;", question)
		if found is None:
			raise OracleError(f"SM_QUESTION_WINDOW.{name}: not found")
		found_questions[name] = int(found.group(1))
	return found_messages, found_questions


class AbyssRanks:
	"""AbyssRankEnum's constants in declaration order: name -> (id, pointsGained, pointsLost, requiredAP, requiredGP)."""

	def __init__(self, java_src: Path):
		source = Path(java_src) / "com" / "aionemu" / "gameserver" / "utils" / "stats" / "AbyssRankEnum.java"
		self.ranks: list[tuple[str, int, int, int, int, int]] = []
		for name, args in enum_constants(source, "AbyssRankEnum"):
			values = [java_int(a.strip(), f"AbyssRankEnum.{name}") for a in (args or "").split(",")]
			if len(values) != 5:
				raise OracleError(f"AbyssRankEnum.{name}({args}): expected five ints")
			self.ranks.append((name, *values))
		if not self.ranks:
			raise OracleError("AbyssRankEnum: no constants")

	def for_points(self, ap: int, gp: int = 0) -> tuple[str, int, int, int, int, int]:
		"""getRankForPoints: starts at GRADE9_SOLDIER (the first), takes every rank whose requirements the points meet"""
		r = self.ranks[0]
		for rank in self.ranks:
			if rank[4] <= ap and rank[5] <= gp:
				r = rank
		return r


def _rate(rates: list[float], membership: int) -> float:
	if not rates:
		return 1.0  # Rates.get: "Missing rates" warning, 1
	return rates[min(len(rates) - 1, membership)]


def ap_lost(ranks: AbyssRanks, victim_ap: int, victim_level: int, winner_level: int, loss_rates: list[float], membership: int) -> int:
	"""StatFunctions.calculatePvPApLost, then Rates.AP_PVP_LOST"""
	points = ranks.for_points(victim_ap)[3]
	difference = winner_level - victim_level
	if difference >= 5:
		points = round_to_int(f32(points * f32(0.1)))
	elif difference == 4:
		points = round_to_int(f32(points * f32(0.65)))
	elif difference == 3:
		points = round_to_int(f32(points * f32(0.85)))
	return to_long(f32(points * f32(_rate(loss_rates, membership))))


def ap_gained_base(ranks: AbyssRanks, victim_ap: int, victim_level: int, winner_rank: int, max_level: int) -> int:
	"""StatFunctions.calculatePvpApGained(defeated, winnerAbyssRank, maxLevel)"""
	victim = ranks.for_points(victim_ap)
	points = victim[2]
	difference = max_level - victim_level
	if difference > 4:
		points = round_to_int(f32(points * f32(0.1)))
	elif difference < -3:
		points = round_to_int(f32(points * f32(1.3)))
	elif difference in GAINED_LEVEL_FACTORS:
		points = round_to_int(f32(points * f32(GAINED_LEVEL_FACTORS[difference])))
	rank_difference = winner_rank - victim[1]
	if winner_rank <= 7 and rank_difference > 0:
		penalty = f32(rank_difference * f32(0.05))
		points -= round_to_int(f32(points * penalty))
	return points


def solo_kill(ranks: AbyssRanks, config: dict, victim_ap: int, victim_level: int, winner_ap: int, winner_level: int, membership: int,
              kill_number: int = 1) -> dict:
	"""PvpService.doReward for a winner who did all the damage alone"""
	loss_rates = list(config["gameserver.rates.ap.pvp.loss"].value)
	gain_rates = list(config["gameserver.rates.ap.pvp.gain"].value)
	if config["gameserver.enable.ap.cap"].value:
		raise OracleError("gameserver.enable.ap.cap is true: the cap is not modelled (AbyssRank.addAp's cappedCount)")
	winner_rank = max(1, ranks.for_points(winner_ap)[1])  # rewardPlayerTeam: maxRank starts at 1
	lost = ap_lost(ranks, victim_ap, victim_level, winner_level, loss_rates, membership)
	base = f32(ap_gained_base(ranks, victim_ap, victim_level, winner_rank, winner_level) * f32(1.0))  # * apWinMulti
	per_member = round_to_int(f32(f32(base * f32(1.0)) / 1))  # baseApReward * (damage / (float) total) / players.size()
	if kill_number < config["gameserver.pvp.maxkills"].value:
		gained = to_long(f32(f32(per_member * f32(_rate(gain_rates, membership))) * f32(100 / 100))) if per_member > 0 else 1
	else:
		gained = 1
	victim_after = max(0, victim_ap - lost) if lost > 0 else victim_ap
	winner_after = max(0, winner_ap + gained)
	return {
		"victim": {"apBefore": victim_ap, "apLost": lost, "apAfter": victim_after, "rankBefore": ranks.for_points(victim_ap)[1],
		           "rankAfter": ranks.for_points(victim_after)[1], "level": victim_level},
		"winner": {"apBefore": winner_ap, "apGained": gained, "apAfter": winner_after, "rankBefore": winner_rank,
		           "rankAfter": ranks.for_points(winner_after)[1], "level": winner_level},
		"membership": membership,
		"killNumber": kill_number,
	}


def titles(data: StaticData) -> dict:
	by_race: dict[str, list[dict]] = {}
	for element in data.children("player_titles", "title"):
		race = element.get("race")
		entry = {"id": java_int(element.get("id"), "title id"), "nameId": java_int(element.get("nameId"), "title nameId"), "race": race}
		by_race.setdefault(race, []).append(entry)
	return {race: entries[:3] for race, entries in by_race.items()}


def daeva_seeds(data: StaticData, java_src: Path, levels: list[int]) -> dict:
	rules = JavaC19Rules.read(java_src)
	experience = [_java_long(e.text, "player_experience_table exp") for e in data.children("player_experience_table", "exp")]
	seeds = {}
	for level in levels:
		if not rules.daeva_level <= level <= len(experience):
			raise OracleError(f"--daeva-level {level}: a Daeva level is {rules.daeva_level}..{len(experience)}")
		exp = experience[level - 1]
		loaded = level_at_load(experience, exp, True, rules)
		if loaded != level:
			raise OracleError(f"--daeva-level {level}: getStartExpForLevel({level}) = {exp} loads at level {loaded}")
		seeds[str(level)] = {"exp": exp, "levelWithoutQuest": level_at_load(experience, exp, False, rules)}
	return {"ascensionQuests": rules.ascension_quests, "daevaLevel": rules.daeva_level, "nonDaevaMaxLevel": rules.non_daeva_max_level,
	        "byLevel": seeds}


def social_report(data: StaticData, java_src: Path, config_dir: Path | None, profile: Path | None, overrides: list[str], messages: list[str],
                  questions: list[str], daeva_levels: list[int], kill: tuple[int, int, int, int] | None, membership: int,
                  require_profile: bool = False) -> dict:
	check_statements(java_src)
	config = load_config(java_src, config_dir, profile, overrides, keys=SOCIAL_KEYS, require_profile=require_profile)
	ranks = AbyssRanks(java_src)
	found_messages, found_questions = message_ids(java_src, messages, questions)
	report = {
		"format": "aion-m5j-social",
		"version": 1,
		"config": {key: value.as_json() for key, value in config.items()},
		"messages": found_messages,
		"questions": found_questions,
		"titles": titles(data),
		"abyssRanks": [{"name": r[0], "id": r[1], "pointsGained": r[2], "pointsLost": r[3], "requiredAP": r[4], "requiredGP": r[5]} for r in ranks.ranks],
		"daeva": daeva_seeds(data, java_src, daeva_levels),
	}
	if kill is not None:
		victim_ap, victim_level, winner_ap, winner_level = kill
		report["pvpKill"] = solo_kill(ranks, config, victim_ap, victim_level, winner_ap, winner_level, membership)
	return report
