"""The golden quest trace extractor (phase6-inventory.md §7.6 item 3; phase6-questgen-prototype.md §8.3).

For one Java quest handler it symbolically executes every hook (the overrides of AbstractQuestHandler's on*Event methods and
rideAction) over questgen's jast statement trees and records every return leaf as a case:

- given: the inputs the path read, with a value that satisfies every guard on the path: the target (an Npc with its npc id, or none),
  the handler's QuestState (none, or its status, the var slots read, canRepeat, the reward group), the dialog action (name and id), the
  inventory counts the guards name, other quests' states, the player's race/level/class/gender, the hook's own arguments (item id,
  movie id, zone);
- assume: what a helper returned when a guard branched on it (`if (QuestService.startQuest(env))`);
- guards: the Java text of every condition taken, with its outcome, in order;
- effects: the calls with side effects in order, with their arguments evaluated under `given` (dialog pages, var, status and reward-group
  writes, item gives and removes, movies, packets, the AbstractQuestHandler and QuestService helpers, env writes);
- returns (the value, or {"resultOf": k} for a helper's result, or {"fromBoolean": ...}) or throws (a NullPointerException when the path
  dereferences an absent QuestState or target; the call's arguments are evaluated first, JLS 15.12.4);
- ranges: the inputs a guard bounds on both ends, as [lo, hi]: both ends satisfy every guard of the path (a value a guard excludes is
  skipped), given holds lo and a harness may check hi as well; rangeExcludes lists the values inside [lo, hi] a guard excludes;
- dialogExcludes: when the path reads the dialog action only through `!=` guards, the action ids they exclude (given holds one action
  outside them; any other outside them takes the same path with the same effects, Extractor.dialog_excludes).

Each document also holds the registration trace (register(), in statement order, loops over constant arrays unrolled; the Python-only form
of phase6-questgen-prototype.md §8.2) and the hooks with their case counts or the reason a hook is refused.

The cases are call traces: a helper call is an effect with its arguments, not expanded into the packets it sends. A recording double of
AbstractQuestHandler/QuestState (phase6-inventory.md §7.6 item 3, the link seam) returns the assumed results and compares the calls; a harness
on the real engine (after M5d) compares the observable subset (dialog pages, var and status writes, items, movies) and runs the helpers
for real. Guards are equalities, ranges, set membership and their negations over the inputs, so a satisfying value is picked directly;
an infeasible branch is dropped.

A hook is refused (listed with its reason, no cases) when it does something this extractor does not model: a call outside the helper
table, a read of state that an unmodelled helper may have written (`changeQuestStep` then `qs.getQuestVarById`), a loop that is not over
a constant array, a guard over two inputs, floating-point arithmetic, a closure other than the task of ThreadPoolManager.schedule (which runs
after the hook, run_tasks; lane C, phase6-transliterator.md §7), a construct the shared parser refuses. Semantics are taken from the Java
sources named at each rule.

Limits of the input model: the visible object is an Npc or nothing. QuestEnv.getTargetId (QuestEnv.java:94-96) also returns the template
id of a visible object that is not an Npc (a gatherable, a static object), which makes `instanceof Npc` false with a non-zero target id;
no case has such a target. A target the guards exclude gets the first npc id of the handler's registrations, else of OTHER_NPCS (real
templates of npc_templates.xml), that no guard names.

Common mode: the extractor parses the Java through questgen's jast (and tools/gen/javasrc), the parser the generator uses. A mis-parse there
(precedence, associativity, labels) would give the C++ and the expected trace the same wrong meaning; tests/test_quest_trace.py pins the
precedence and associativity the oracle relies on through evaluated effects. It also means the tools.oracle tests import tools/gen.
"""
from __future__ import annotations

import hashlib
import json
import math
import sys
from dataclasses import dataclass
from pathlib import Path

from m5d.javasrc import DialogTables, java_enum_constants
from staticdata_oracle import OracleError

TOOL_DIR = Path(__file__).resolve().parents[1]          # cpp/tools/oracle
_GEN_DIR = str(TOOL_DIR.parent / 'gen')
_SYS_PATH = [p for p in sys.path if p != _GEN_DIR]
sys.path.append(_GEN_DIR)                               # after the oracle's own packages (tools/gen has a `tests` package too)

import javasrc  # noqa: E402
from questgen import jast  # noqa: E402  (the Java statement parser; the generator itself, questgen.emit, is never imported)

sys.path[:] = _SYS_PATH + [_GEN_DIR]                    # questgen.paths put tools/gen and tools/porting first; keep them last

REPO = TOOL_DIR.parents[2]
JAVA_SRC = REPO / 'game-server' / 'src'
QUEST_DIR = REPO / 'game-server' / 'data' / 'handlers' / 'quest'
EXPECTED_DIR = TOOL_DIR / 'expected' / 'quest'
FORMAT, VERSION = 'aion-quest-trace', 1
MAX_LEAVES = 4000
GS = Path('com') / 'aionemu' / 'gameserver'
# the target of a case whose guards exclude every npc the handler registers: the first three templates of
# data/static_data/npcs/npc_templates.xml (tests/test_quest_trace.py checks they exist), so a harness can spawn it
OTHER_NPCS = (200000, 200001, 201000)

# The first slice (phase6-inventory.md §9.3 item 1): the Poeta and Ishalgen handlers that questgen transliterates in tier A with the P6-T
# rules (tools/gen/questgen, --dry-run). Chosen once by directory and tier; the list is data here, the oracle never runs the generator.
SLICE_TIER_A = (
	'ishalgen/_2000Prologue.java', 'ishalgen/_2001ThinkingAhead.java', 'ishalgen/_2003TreasureOfTheDeceased.java',
	'ishalgen/_2005TeachingaLesson.java', 'ishalgen/_2006HitThemWhereitHurts.java', 'ishalgen/_2106VanarsFlattery.java',
	'ishalgen/_2114TheInsectProblem.java', 'ishalgen/_2122AshesToAshes.java', 'ishalgen/_2123TheImprisonedGourmet.java',
	'ishalgen/_2125TheRobberyPlot.java', 'ishalgen/_2135ForLoveofNegi.java',
	'poeta/_1000Prologue.java', 'poeta/_1001TheKerubThreat.java', 'poeta/_1003IllegalLogging.java', 'poeta/_1004NeutralizingOdium.java',
	'poeta/_1005BarringtheGate.java', 'poeta/_1107TheLostAxe.java', 'poeta/_1111InsomniaMedicine.java',
	'poeta/_1122DeliveringPernossRobe.java', 'poeta/_1123WheresTutty.java',
)
# The ascension route slice (P6-Q, 2026-09-29, lane route-gen): the other generated handlers of the route's three directories. 1100 and 2100
# (tier B) are traced whole since P6-Q prologue (2026-09-29: `WorldMapType.X.getId()` is the map id of WorldMapType.java, and
# `player.getWorldId()` an input); 1205 and 2132 have every hook refused (`new QuestEnv`, getStartingClass on a value) and only their
# registration traced; the 12 dispatches of ascension/ are traced whole. The C++ harness (game-server/tests/quest_handlers_golden) drives
# every document of the directory.
SLICE_ROUTE = (
	'poeta/_1100KaliosCall.java', 'poeta/_1205ANewSkill.java', 'ishalgen/_2100OrderoftheCaptain.java', 'ishalgen/_2132ANewSkill.java',
	'ascension/_1913DispatchtoVerteron.java', 'ascension/_1914DispatchtoVerteron.java', 'ascension/_1915DispatchtoVerteron.java',
	'ascension/_1916DispatchtoVerteron.java', 'ascension/_19070ADispatchtoVerteron.java', 'ascension/_19071ADispatchtoVerteron.java',
	'ascension/_2901DispatchtoAltgard.java', 'ascension/_2902DispatchtoAltgard.java', 'ascension/_2903DispatchtoAltgard.java',
	'ascension/_2904DispatchtoAltgard.java', 'ascension/_29070ADispatchtoAltgard.java', 'ascension/_29071ADispatchtoAltgard.java',
)
# P6-Q slice 2, chunk Q03 (2026-09-29): the 76 handlers of verteron/ and heiron/ that questgen transliterates (the zones the dispatches of
# the route end in). The two it refuses, heiron/_1643TheStarOfHeiron and heiron/_3200PriceOfGoodwill (anonymous Runnables), are hand ports
# with their own unit cases (game-server/tests/quest_handlers_q03), not in the slice. The C++ harness drives every document of the directory.
SLICE_Q03 = (
	'verteron/_1131UndeliveredArmor.java', 'verteron/_1141BelbuasTreasure.java', 'verteron/_1146DelicateMandrake.java',
	'verteron/_1149MissingPoppy.java', 'verteron/_1152OdellaRecipe.java', 'verteron/_1156StolenVillageSeal.java',
	'verteron/_1157GaphyrksLove.java', 'verteron/_1158VillageSealFound.java', 'verteron/_1162AltenosWeddingRing.java',
	'verteron/_1163ArachnaAntidote.java', 'verteron/_1169LightningfootTuka.java', 'verteron/_1170HeadlessStoneStatue.java',
	'verteron/_1183SpiritOfNature.java', 'verteron/_1192VerteronReinforcements.java', 'verteron/_1194ReducingTursinStrength.java',
	'verteron/_1197KrallBook.java', 'verteron/_1198TheWritingOnTheWall.java', 'verteron/_1218NumonerksDemandNote.java',
	'verteron/_1220ASecretDelivery.java', 'verteron/_14010TerrainOfTheVerteronFortress.java', 'verteron/_14011FragmentsInTheSky.java',
	'verteron/_14012DukakiMischief.java', 'verteron/_14013AFrillOfAFuss.java', 'verteron/_14014TurningTheIde.java',
	'verteron/_14015NotBlindedByVengeance.java', 'verteron/_14016AGateAgape.java', 'heiron/_14050OrdersFromHeironFortress.java',
	'heiron/_14051RootOfTheProblem.java', 'heiron/_14052RestlessSouls.java', 'heiron/_14053DangerCubed.java',
	'heiron/_14054KrallIngToKralltumagna.java', 'heiron/_1527RottenRotrons.java', 'heiron/_1528StrangeLeather.java',
	'heiron/_1535TheColdColdGround.java', 'heiron/_1537FishOnTheLine.java', 'heiron/_1540BaittheHooks.java', 'heiron/_1548KlawControl.java',
	'heiron/_1553MirrorMirror.java', 'heiron/_1559WhatsintheBox.java', 'heiron/_1560AJobForPobinerk.java', 'heiron/_1561TheMisersMap.java',
	'heiron/_1562CrossedDestiny.java', 'heiron/_1563TheLegendofVindachinerk.java', 'heiron/_1573SomeTastyMushrooms.java',
	'heiron/_1574AFeatForAVillage.java', 'heiron/_1578WhereDoRotronsComeFrom.java', 'heiron/_1582ThePriestsNightmare.java',
	'heiron/_1604ToCatchASpy.java', 'heiron/_1605TheLepharistSituation.java', 'heiron/_1607MappingTheRevolutionaries.java',
	'heiron/_1609MessageToArbolusHaven.java', 'heiron/_1612LepharistSecrets.java', 'heiron/_1614WheresBelbua.java',
	'heiron/_1620StartSpreadingTheNews.java', 'heiron/_1626LightThePath.java', 'heiron/_1628MeteriasRegret.java',
	'heiron/_1634TheWreckOfTheArgos.java', 'heiron/_1636AFluteForTheFixing.java', 'heiron/_1640TeleporterRepairs.java',
	'heiron/_1644AVeryOldLetter.java', 'heiron/_1647DressingUpForBollvig.java', 'heiron/_1648UndeadWarAlert.java',
	'heiron/_1661FindingTheForges.java', 'heiron/_1670InvisibleBridges.java', 'heiron/_1687TheTigrakiAgreement.java',
	'heiron/_1691TheLittleLeatherSlipper.java', 'heiron/_1692ADayOlderAndDeeperInDebt.java', 'heiron/_1693AreYouMyFather.java',
	'heiron/_18600ScoringSomeBadStigma.java', 'heiron/_18601NightmareonMyStreets.java', 'heiron/_18602NightmareinShiningArmor.java',
	'heiron/_3502NereusNeedsYou.java', 'heiron/_80217ToDarkPoeta.java', 'heiron/_80218DarkPoetaEncore.java',
	'heiron/_80219DarkPoetaFinale.java', 'heiron/_80220DarkPoetaFinale.java',
)
# P6-Q slice 2, chunk Q10 (2026-09-29): the handlers of altgard/ and pandaemonium/ that questgen transliterates (69 of 75; the other six are
# hand ports or held back, docs/deviations/Q10.md). The C++ harness (game-server/tests/quest_handlers_golden) drives every document.
SLICE_Q10 = (
	'altgard/_2207ConversingWithaSkurv.java', 'altgard/_2209TheScribbler.java', 'altgard/_2213PoisonRootPotentFruit.java',
	'altgard/_2216MuMuGrassKnot.java', 'altgard/_2221ManirsUncle.java', 'altgard/_2222ManirsMessage.java', 'altgard/_2223AMythicalMonster.java',
	'altgard/_2228AThornInItsSide.java', 'altgard/_2231SiblingRivalry.java', 'altgard/_2232TheBrokenHoneyJar.java',
	'altgard/_2239MalodorAntidote.java', 'altgard/_2247TheGergersDisguise.java', 'altgard/_2263ShugoPotion.java',
	'altgard/_2266ATrustworthyMessenger.java', 'altgard/_2271AurtrisLetter.java', 'altgard/_2278ASecretProposal.java',
	'altgard/_2279SolidProof.java', 'altgard/_2284EscapingAsmodae.java', 'altgard/_2288MoneyWhereYourMouthIs.java',
	'altgard/_2289RampagingMosbears.java', 'altgard/_2290GrokensEscape.java', 'altgard/_24010SuthransOrders.java',
	'altgard/_24011FunnyFloatingFungus.java', 'altgard/_24012AnOminousCrop.java', 'altgard/_24014StompOutThePlot.java',
	'altgard/_24015TotemPlowed.java', 'altgard/_24016AStrangeNewThread.java', 'altgard/_24112NoLaissezFaireForLepharists.java',
	'pandaemonium/_29004VeldinaCall.java', 'pandaemonium/_29048SeriphimTeachings.java', 'pandaemonium/_2911SongOfBlessing.java',
	'pandaemonium/_2912FollowtheRibbon.java', 'pandaemonium/_2913AChainofDebt.java', 'pandaemonium/_2914ATokenofLostLove.java',
	'pandaemonium/_2916ManInTheLongBlackRobe.java', 'pandaemonium/_2917ArekedilsHeritage.java', 'pandaemonium/_2918DeepMaternalLove.java',
	'pandaemonium/_2919BookOfOblivion.java', 'pandaemonium/_2920ElementaryMyDearDaeva.java', 'pandaemonium/_2921LoveAtFirstSight.java',
	'pandaemonium/_2922FascinatingGift.java', 'pandaemonium/_2925AHeartfeltConfession.java', 'pandaemonium/_2928PowerofLove.java',
	'pandaemonium/_2937UnexpectedReward.java', 'pandaemonium/_2938SecretLibraryAccess.java', 'pandaemonium/_2948HuronsLetter.java',
	'pandaemonium/_2952WinningVindachinerksFavor.java', 'pandaemonium/_2953DeliveringSupplyRequest.java',
	'pandaemonium/_2954DeliveringOdellaJuice.java', 'pandaemonium/_2957FlowersForTheBanquet.java', 'pandaemonium/_2958LastMinuteWorries.java',
	'pandaemonium/_2962JafnharWhereabouts.java', 'pandaemonium/_2963OnBehalfOfAFriend.java', 'pandaemonium/_2965AncientWeapons.java',
	'pandaemonium/_2985AnExpertsReward.java', 'pandaemonium/_4210MissingHaorunerk.java', 'pandaemonium/_4905InterviewingTheVeterans.java',
	'pandaemonium/_4906TalesOfHeroes.java', 'pandaemonium/_4920MakingTheActivatedSurkana.java', 'pandaemonium/_4966GrowthNinissFirstCharm.java',
	'pandaemonium/_4967GrowthNinissSecondCharm.java', 'pandaemonium/_4968GrowthNinissThirdCharm.java',
	'pandaemonium/_4969GrowthNinissFourthCharm.java', 'pandaemonium/_4970TheFashionistas.java', 'pandaemonium/_4971ProjectRunway.java',
	'pandaemonium/_4972JudgeNot.java', 'pandaemonium/_4973MarraWorry.java', 'pandaemonium/_4974TheSecretOfHisSuccess.java',
	'pandaemonium/_4976ASettlerAmbition.java',
)
SLICE = SLICE_TIER_A + SLICE_ROUTE + SLICE_Q03 + SLICE_Q10

ENUM_FILES = {'QuestStatus': 'questEngine/model/QuestStatus.java', 'Race': 'model/Race.java', 'PlayerClass': 'model/PlayerClass.java',
              'Gender': 'model/Gender.java', 'HandlerResult': 'questEngine/handlers/HandlerResult.java', 'DialogPage': 'model/DialogPage.java',
              'BonusType': 'model/templates/rewards/BonusType.java', 'AbyssRankEnum': 'utils/stats/AbyssRankEnum.java'}


class Unsupported(Exception):
	"""a construct the extractor does not model: the hook is refused with this reason"""


# --- values ------------------------------------------------------------------------------------------------------------------------

@dataclass(frozen=True)
class Enum:
	type: str
	name: str


@dataclass(frozen=True)
class K:
	"""a constant: int, float, bool, str (a Java string literal as written), None (null), Enum, tuple (a constant array)"""
	v: object


@dataclass(frozen=True)
class In:
	"""an input of the case: ('target',) ('targetId',) ('dialog',) ('status', q) ('var', q, n) ('canRepeat', q) ('rewardGroup', q)
	('completeCount', q) ('inv', itemId) ('player', attr) ('env', attr) ('arg', name)"""
	key: tuple


@dataclass(frozen=True)
class Ar:
	op: str
	a: object
	b: object


@dataclass(frozen=True)
class Res:
	"""the result of effect k"""
	k: int


@dataclass(frozen=True)
class O:
	"""an object: 'env' 'player' 'qsl' 'inventory' 'target' 'npc' 'item' 'qe' ('qs', q) ('questVars', q) ('questNpc', id)"""
	name: object


@dataclass(frozen=True)
class Cmp:
	op: str
	a: object
	b: object


@dataclass(frozen=True)
class BAnd:
	"""a && b without side effects (QuestState.isStartable); la and lb name the two parts in the guard text"""
	a: object
	b: object
	la: str = ''
	lb: str = ''


@dataclass(frozen=True)
class Lazy:
	"""a state key read only when a short-circuit reaches it"""
	key: tuple


@dataclass(frozen=True)
class Not:
	x: object


@dataclass(frozen=True)
class FromBool:
	x: object


@dataclass(frozen=True)
class New:
	cls: str
	args: tuple


@dataclass(frozen=True)
class Opaque:
	what: str


@dataclass(frozen=True)
class RewardPage:
	"""DialogPage.getRewardPageByIndex(x).id()"""
	x: object


NEGATE = {'==': '!=', '!=': '==', '<': '>=', '>=': '<', '>': '<=', '<=': '>'}
MIRROR = {'==': '==', '!=': '!=', '<': '>', '>': '<', '<=': '>=', '>=': '<='}
INF = math.inf


def java_int(v):
	"""Java int arithmetic wraps at 32 bits"""
	v &= 0xFFFFFFFF
	return v - (1 << 32) if v & 0x80000000 else v


INT_BITS = {'long': 64, 'int': 32, 'short': 16, 'byte': 8}


def java_narrow(x, ty):
	"""a Java cast of the number x to the integral type ty (JLS 5.1.3): a floating value is truncated toward zero and saturates at the
	int (long) range, NaN is 0; an integral value then keeps its low bits, sign-extended"""
	bits = INT_BITS[ty]
	if isinstance(x, float):
		wide = 64 if bits == 64 else 32
		lo, hi = -(1 << (wide - 1)), (1 << (wide - 1)) - 1
		x = 0 if math.isnan(x) else (lo if x == -INF else hi if x == INF else max(lo, min(hi, int(x))))
	x &= (1 << bits) - 1
	return x - (1 << bits) if x >> (bits - 1) else x


# --- input domains -----------------------------------------------------------------------------------------------------------------

@dataclass(frozen=True)
class Dom:
	allowed: tuple | None = None        # the values still possible, in preference order; None: any value of the kind
	excluded: frozenset = frozenset()
	lo: float = -INF
	hi: float = INF

	def restrict(self, op, v):
		if op == '==':
			allowed = (v,) if self.allowed is None else tuple(x for x in self.allowed if x == v and type(x) is type(v))
			return Dom(allowed, self.excluded, self.lo, self.hi)
		if op == '!=':
			return Dom(self.allowed, self.excluded | {v}, self.lo, self.hi)
		if not isinstance(v, int) or isinstance(v, bool):
			raise Unsupported(f'ordering {op} on {v!r}')
		lo, hi = self.lo, self.hi
		if op == '<':
			hi = min(hi, v - 1)
		elif op == '<=':
			hi = min(hi, v)
		elif op == '>':
			lo = max(lo, v + 1)
		elif op == '>=':
			lo = max(lo, v)
		return Dom(self.allowed, self.excluded, lo, hi)

	def ok(self, x):
		if x in self.excluded:
			return False
		if isinstance(x, int) and not isinstance(x, bool):
			return self.lo <= x <= self.hi
		return self.lo == -INF and self.hi == INF         # a range guard implies a non-null int

	def pick(self, prefer=()):
		"""a value of the domain, or None when it is empty (callers use sat() first where None is a value)"""
		if self.allowed is not None:
			return next((x for x in self.allowed if self.ok(x)), _EMPTY)
		for x in prefer:
			if self.ok(x):
				return x
		start = self.lo if self.lo != -INF else (0 if self.hi >= 0 else self.hi)
		step = 1 if self.hi == INF or start <= self.hi else -1
		x = int(start)
		for _ in range(len(self.excluded) + 2):
			if self.ok(x):
				return x
			x += step
		return _EMPTY

	def sat(self):
		return self.pick() is not _EMPTY


_EMPTY = object()


# --- a symbolic path ------------------------------------------------------------------------------------------------------------------------

class Task:
	"""a closure (jast.Closure) the path built: its node and the locals it captured (Java captures effectively final locals, so their
	values when the closure is built are the values it reads)"""
	__slots__ = ('node', 'locals')

	def __init__(self, node, locals_):
		self.node = node
		self.locals = locals_


class SPath:
	__slots__ = ('dom', 'state', 'clobbered', 'locals', 'effects', 'assume', 'guards', 'reads', 'deferred', 'task', 'task_of')

	def __init__(self):
		self.dom = {}           # input key -> Dom
		self.state = {}         # state key -> value written on the path
		self.clobbered = {}     # state category -> the helper that may have written it
		self.locals = {}
		self.effects = []       # [(call, [args], kind, line)]
		self.assume = {}        # effect index -> bool
		self.guards = []        # [(line, text, outcome)]
		self.reads = []         # input keys, in first-read order
		self.deferred = []      # [(Task, delay, effect index of its schedule)]: the tasks the hook scheduled, run after it returns
		self.task = None        # while a scheduled task runs: the effect index of its schedule
		self.task_of = {}       # effect index -> the effect index of the schedule whose task made it

	def fork(self):
		p = SPath.__new__(SPath)
		p.dom, p.state, p.clobbered, p.locals = dict(self.dom), dict(self.state), dict(self.clobbered), dict(self.locals)
		p.effects, p.assume, p.guards, p.reads = list(self.effects), dict(self.assume), list(self.guards), list(self.reads)
		p.deferred, p.task, p.task_of = list(self.deferred), self.task, dict(self.task_of)
		return p


# --- Java-derived tables -----------------------------------------------------------------------------------------------------------

class Tables:
	"""DialogAction ids, DialogPage ids and reward pages, the enum constants and the hook names, read from the Java sources"""

	def __init__(self, java_src=JAVA_SRC):
		base = Path(java_src) / GS
		self.dialog = DialogTables.read(java_src)
		self.action_name = {}
		for name, v in self.dialog.actions.items():
			self.action_name.setdefault(v, name)
		self.enums = {n: [c for c, _ in java_enum_constants(base / f, n)] for n, f in ENUM_FILES.items()}
		# WorldMapType.X.getId() (WorldMapType.java:212-219, getId :221-223): the map id is the first constructor argument, in declaration order
		self.world_maps = {c: int(a.split(',')[0]) for c, a in java_enum_constants(base / 'world' / 'WorldMapType.java', 'WorldMapType')}
		cu = javasrc.parse_file(str(base / 'questEngine' / 'handlers' / 'AbstractQuestHandler.java'))
		aqh = cu.types[0]
		self.hooks = {m.name for m in aqh.methods if m.kind == 'method' and 'static' not in m.modifiers
		              and (m.name.startswith('on') or m.name == 'rideAction')}


# AbstractQuestHandler and QuestService helpers: (effect kind, state categories the Java body may write). 'quest' is the handler's own
# QuestState (status, vars, reward group), 'inventory' the player's items, 'all' anything, other quests included. Pure senders write none.
# AbstractQuestHandler.java: updateQuestStatus :290, changeQuestStep :298-328, sendQuestDialog :330-345, sendQuestSelectionDialog :354,
# closeDialogWindow :359, sendQuestStartDialog :364-399 (QuestService.startQuest, giveQuestItem), sendQuestEndDialog :401-474
# (finishQuest, validateAndFixRewardGroup, onDialog of another quest), defaultCloseDialog :486-529, checkQuestItems :531-575,
# checkItemExistence :576-609, sendEmotion :611, giveQuestItem :618-642, removeQuestItem :644-659, playQuestMovie :661-670, the
# default*Event and useQuest* helpers :672-986, defaultOnLevelChangedEvent :988, defaultOnQuestCompletedEvent :1047, sendQuestRewardDialog
# :1151, sendQuestNoneDialog :1166-1225, sendItemCollectingStartDialog :1227; QuestService.java startQuest :400-444 (modelled: false
# changes nothing, true sets START), finishQuest, collectItemCheck, abandonQuest.
HELPERS = {
	'sendQuestDialog': ('dialog', ()), 'sendQuestSelectionDialog': ('dialog', ()), 'closeDialogWindow': ('dialog', ()),
	'updateQuestStatus': ('quest', ()), 'playQuestMovie': ('movie', ()), 'sendEmotion': ('packet', ()),
	'sendQuestStartDialog': ('dialog', ('quest', 'inventory')), 'sendQuestEndDialog': ('dialog', ('all',)),
	'defaultCloseDialog': ('dialog', ('all',)), 'changeQuestStep': ('var', ('quest',)),
	'checkQuestItems': ('item', ('all',)), 'checkQuestItemsSimple': ('item', ('all',)), 'checkItemExistence': ('item', ('all',)),
	'giveQuestItem': ('item', ('inventory',)), 'removeQuestItem': ('item', ('inventory',)),
	'defaultOnKillEvent': ('var', ('all',)), 'defaultOnKillRankedEvent': ('var', ('all',)), 'defaultOnKillInZoneEvent': ('var', ('all',)),
	'defaultOnUseSkillEvent': ('var', ('all',)), 'defaultOnGetItemEvent': ('var', ('all',)), 'defaultOnEnterZoneEvent': ('var', ('all',)),
	'useQuestObject': ('var', ('all',)), 'useQuestItem': ('var', ('all',)),
	'defaultOnLevelChangedEvent': ('quest', ('all',)), 'defaultOnQuestCompletedEvent': ('quest', ('all',)),
	'sendQuestRewardDialog': ('dialog', ('all',)), 'sendQuestNoneDialog': ('dialog', ('all',)),
	'sendItemCollectingStartDialog': ('dialog', ('all',)),
	'QuestService.startQuest': ('quest', ()), 'QuestService.finishQuest': ('quest', ('all',)),
	'QuestService.collectItemCheck': ('item', ('inventory',)), 'QuestService.abandonQuest': ('quest', ('all',)),
	'super.onDialogEvent': ('dialog', ('all',)),
}
# helper arguments that are the hook's own env or player are left out of the recorded arguments
IMPLICIT = (O('env'), O('player'))
# Java varargs helpers (AbstractQuestHandler.java:988, :1047): an int[] passed as the varargs parameter is the same call as its elements
VARARGS = frozenset(('defaultOnLevelChangedEvent', 'defaultOnQuestCompletedEvent'))
# hook arguments whose values are not ints: a guard can only name some of them, so an unconstrained one is "any value but these"
NON_INT_ARGS = frozenset(('String', 'ZoneName', 'QuestActionType', 'BonusType'))


def category(key):
	"""the state category a helper may write: ('vars', q), ('quest', q) (status, reward group, repeat data), ('inventory',), ('env',)"""
	head = key[0]
	if head == 'var':
		return ('vars', key[1])
	if head in ('status', 'canRepeat', 'rewardGroup', 'completeCount'):
		return ('quest', key[1])
	if head == 'inv':
		return ('inventory',)
	if head in ('dialog', 'env'):
		return ('env',)
	return (head,)


# --- the extractor -----------------------------------------------------------------------------------------------------------------

class Extractor:
	def __init__(self, tables, path, rel):
		self.t = tables
		self.rel = rel
		self.cu = javasrc.parse_file(str(path))
		if len(self.cu.types) != 1 or self.cu.types[0].kind != 'class':
			raise OracleError(f'{rel}: not one top-level class')
		self.td = self.cu.types[0]
		# closures=True (lane C, phase 6 step 1): a lambda or an anonymous Runnable is a jast.Closure node, so a closure refuses the hook that
		# builds it (or is modelled: the task of ThreadPoolManager.schedule, run_tasks) instead of the whole file. switch_expressions=True (the
		# review of #79, item 9): a switch expression (SwitchExpr, eval) and a switch statement with rule arms (Switch.rules, exec_switch) are
		# modelled too, as the generator's P6-T rule parses them
		self.p = jast.Parser(self.cu, switch_expressions=True, closures=True)
		self.mode = 'hook'
		self.leaves = []
		self.reg_npcs = []
		self.reg_items = []
		self.qid = self.quest_id()
		self.consts = {}
		self.fields()
		self.own = {m.name: m for m in self.td.methods if m.kind == 'method' and m.name not in self.t.hooks and m.name != 'register'}
		self.depth = 0

	# -- file level -----------------------------------------------------------------------------------------------------------------
	def quest_id(self):
		ctors = [m for m in self.td.methods if m.kind == 'constructor']
		texts = ctors[0].body.texts() if len(ctors) == 1 else []
		if len(texts) != 7 or texts[1:3] != ['super', '(']:
			raise OracleError(f'{self.rel}: the constructor is not `super(<quest id>)`')
		if texts[3].isdigit():
			return int(texts[3])
		for f in self.td.fields:                        # super(QUEST_ID) with `static final int QUEST_ID = <int>;`
			if f.name == texts[3] and f.initializer is not None and f.initializer.text.strip().isdigit():
				return int(f.initializer.text.strip())
		raise OracleError(f'{self.rel}: the constructor is not `super(<int>)` or `super(<int constant>)`')

	def fields(self):
		"""constant fields: a final one, or one no method writes (the handler is a singleton; a written field is per-player state)"""
		written = {a.name for m in self.td.methods if m.body is not None for a in m.body.assignments() if not a.local}
		out = self.consts
		for f in self.td.fields:
			if f.initializer is None or ('final' not in f.modifiers and f.name in written):
				out[f.name] = Unsupported(f'field {f.name} is written or has no initializer')
				continue
			s = f.initializer.start
			init = self.p.array_init(s)[0] if self.cu.tokens.text[s] == '{' else self.p.expr_span(s, f.initializer.end)
			try:
				vals = self.eval(init, SPath())
			except Unsupported as e:
				out[f.name] = e
				continue
			if len(vals) != 1 or not isinstance(vals[0][1], K):
				out[f.name] = Unsupported(f'field {f.name} is not a constant')
				continue
			out[f.name] = vals[0][1]

	def line(self, node):
		return self.cu.tokens.loc(node.tok)[0]

	def trace(self):
		try:
			reg = self.register_trace()
		except (Unsupported, jast.Unsupported) as e:
			reg = {'unsupported': str(e)}
		self.reg_npcs = [r['npc'] for r in reg if 'npc' in r] if isinstance(reg, list) else []
		# the quest items register() names (QuestEngine.registerQuestItem): the item of an item-use hook whose guards leave it free is one of
		# them, the only items the engine hands the hook (P6-Q slice 2, Q03: _1561TheMisersMap removes the used item by its id)
		self.reg_items = [r['args'][0] for r in reg if r.get('call') == 'registerQuestItem'] if isinstance(reg, list) else []
		hooks, cases = [], []
		for m in self.td.methods:
			if m.kind != 'method' or m.name not in self.t.hooks or m.body is None:
				continue
			try:
				leaves = self.run_hook(m)
				mine = [self.case(m, kind, p, v) for kind, p, v in leaves if not self.dead_assumption(p)]
			except (Unsupported, jast.Unsupported) as e:
				# jast.Unsupported: a construct the parser refuses (a switch expression, a method reference) refuses the hook, not the file
				hooks.append({'hook': m.name, 'line': self.cu.tokens.loc(m.index)[0], 'unsupported': str(e)})
				continue
			for n, c in enumerate(mine, 1):
				c['id'] = f'{m.name}#{n}'
			hooks.append({'hook': m.name, 'line': self.cu.tokens.loc(m.index)[0], 'cases': len(mine)})
			cases += mine
		return reg, hooks, cases

	# -- register() -----------------------------------------------------------------------------------------------------------------
	def register_trace(self):
		reg = [m for m in self.td.methods if m.kind == 'method' and m.name == 'register']
		if len(reg) != 1:
			raise OracleError(f'{self.rel}: no register()')
		self.mode = 'register'
		self.registrations = []
		try:
			outs = self.exec_stmts(self.p.block_stmts(reg[0].body.start), self.new_path())
		finally:
			self.mode = 'hook'
		if len(outs) != 1:
			raise OracleError(f'{self.rel}: register() branches')
		return self.registrations

	# -- hooks ----------------------------------------------------------------------------------------------------------------------
	def new_path(self):
		return SPath()

	def run_hook(self, m):
		self.leaves = []
		self.arg_types = {prm.name: prm.type.name for prm in m.params}
		p = self.new_path()
		for prm in m.params:
			p.locals[prm.name] = self.param_value(prm)
		stmts = self.p.block_stmts(m.body.start)
		for kind, q, v in self.exec_stmts(stmts, p):
			if kind in ('break', 'continue'):
				raise Unsupported(f'{kind} outside a loop or switch')
			for r in self.run_tasks(q):
				self.leaves.append(('return', r, v if kind == 'return' else None))
		return self.leaves

	def run_tasks(self, p):
		"""[path]: the path with the tasks the hook scheduled run after it returned (ThreadPoolManager.schedule: the task runs on a pool
		thread after its delay, ThreadPoolManager.java schedule; the hook has returned by then). The tasks run in the order of their delays,
		equal delays in the order they were scheduled; each runs on the locals it captured, its `return` ends it. A harness runs them by
		advancing its clock past the longest delay (the effects of a task carry the index of their schedule effect, `task`)"""
		if not p.deferred:
			return [p]
		tasks = sorted(p.deferred, key=lambda t: t[1])            # sorted() is stable: equal delays keep their order
		p.deferred = []
		paths = [p]
		for task, _delay, k in tasks:
			nxt = []
			for q in paths:
				saved = q.locals
				q.locals = dict(task.locals)
				q.task = k
				node = task.node
				outs = self.exec_stmts(node.body, q) if node.body is not None else [('normal', r, None) for r, _v in self.eval(node.expr, q)]
				for kind, r, _v in outs:
					if kind in ('break', 'continue'):
						raise Unsupported(f'{kind} outside a loop in a scheduled task')
					if r.deferred:
						raise Unsupported('a task scheduled by a scheduled task')
					r.locals, r.task = saved, None
					nxt.append(r)
			paths = nxt
			self.budget(len(paths))
		return paths

	def param_value(self, prm):
		t = prm.type.name
		if t == 'QuestEnv':
			return O('env')
		if t == 'Player':
			return O('player')
		if t == 'Item':
			return O('item')
		if t in ('int', 'long', 'String', 'ZoneName', 'QuestActionType', 'BonusType'):
			return In(('arg', prm.name))
		raise Unsupported(f'hook parameter {prm.type} {prm.name}')

	def budget(self, n):
		if n + len(self.leaves) > MAX_LEAVES:
			raise Unsupported(f'more than {MAX_LEAVES} paths')

	# -- statements -----------------------------------------------------------------------------------------------------------------
	def exec_stmts(self, stmts, p):
		outs = [('normal', p, None)]
		for s in stmts:
			nxt = []
			for kind, q, v in outs:
				nxt += self.exec_stmt(s, q) if kind == 'normal' else [(kind, q, v)]
			outs = nxt
			self.budget(len(outs))
		return outs

	def exec_stmt(self, s, p):
		if isinstance(s, jast.Block):
			return self.exec_stmts(s.stmts, p)
		if isinstance(s, jast.Empty):
			return [('normal', p, None)]
		if isinstance(s, jast.Local):
			outs = [p]
			for name, extra, init, _tok in s.decls:
				nxt = []
				for q in outs:
					if init is None:
						q.locals[name] = None
						nxt.append(q)
						continue
					for r, v in self.eval(init, q):
						r.locals[name] = v
						nxt.append(r)
				outs = nxt
			return [('normal', q, None) for q in outs]
		if isinstance(s, jast.ExprStmt):
			return [('normal', q, None) for q, _v in self.eval(s.expr, p)]
		if isinstance(s, jast.If):
			out = []
			for q, b in self.branch(s.cond, p):
				if b:
					out += self.exec_stmt(s.then, q)
				elif s.else_ is not None:
					out += self.exec_stmt(s.else_, q)
				else:
					out.append(('normal', q, None))
			return out
		if isinstance(s, jast.Return):
			if s.expr is None:
				return [('return', p, None)]
			return [('return', q, v) for q, v in self.eval(s.expr, p)]
		if isinstance(s, jast.Break):
			return [('break', p, None)]
		if isinstance(s, jast.Continue):
			return [('continue', p, None)]
		if isinstance(s, jast.Switch):
			return self.exec_switch(s, p)
		if isinstance(s, jast.ForEach):
			return self.exec_foreach(s, p)
		raise Unsupported(f'{type(s).__name__} statement at line {self.line(s)}')

	def exec_switch(self, s, p):
		"""Java switch: the group whose label equals the subject, else default; statements run on through later groups until a break. With
		rule arms (`case A -> stmt`, Switch.rules; JLS 14.11.2) an arm never falls through to the next"""
		out = []
		for q, subj in self.eval(s.expr, p):
			labels = []                     # per group: [(value, label text)], (None, None) for default
			for lab_exprs, _body, _toks in s.groups:
				labels.append([(None, None) if lab is None else (self.label_value(lab, subj), self.jtext(lab)) for lab in lab_exprs])
			every = [(v, t) for vals in labels for v, t in vals if t is not None]
			has_default = any(t is None for vals in labels for _v, t in vals)
			text = self.jtext(s.expr)
			for g, vals in enumerate(labels):
				for r in self.select(q, subj, vals, every, text, self.line(s)):
					stmts = [st for _l, body, _t in (s.groups[g:g + 1] if getattr(s, 'rules', False) else s.groups[g:]) for st in body]
					for kind, r2, v in self.exec_stmts(stmts, r):
						out.append(('normal', r2, None) if kind == 'break' else (kind, r2, v))
			if not has_default:
				for r in self.select(q, subj, [(None, None)], every, text, self.line(s)):
					out.append(('normal', r, None))
		return out

	def switch_expr(self, e, p):
		"""a switch expression (`switch (x) { case A, B -> v; default -> w; }`, JLS 15.28): the value of the arm whose label equals the subject,
		else of default; one path per arm the subject can take"""
		out = []
		for q, subj in self.eval(e.expr, p):
			labels = [[(None, None) if lab is None else (self.label_value(lab, subj), self.jtext(lab)) for lab in lab_exprs]
			          for lab_exprs, _v, _t, _s in e.arms]
			every = [(v, t) for vals in labels for v, t in vals if t is not None]
			has_default = any(t is None for vals in labels for _v, t in vals)
			if not has_default:
				raise Unsupported('a switch expression without default')
			text = self.jtext(e.expr)
			for g, vals in enumerate(labels):
				for r in self.select(q, subj, vals, every, text, self.line(e)):
					out += self.eval(e.arms[g][1], r)
		return out

	def label_value(self, lab, subj):
		if isinstance(lab, jast.Name) and lab.name in self.t.enums.get('QuestStatus', ()) and isinstance(subj, (In, K)) and \
				self.is_status(subj):
			return Enum('QuestStatus', lab.name)
		vals = self.eval(lab, self.new_path())
		if len(vals) != 1 or not isinstance(vals[0][1], K):
			raise Unsupported(f'case label {self.jtext(lab)}')
		return vals[0][1].v

	@staticmethod
	def is_status(v):
		return (isinstance(v, In) and v.key[0] == 'status') or (isinstance(v, K) and isinstance(v.v, Enum) and v.v.type == 'QuestStatus')

	def select(self, p, subj, vals, every, text, line):
		"""one path per label of a group on which the subject equals it; the default label: the subject equals none of `every`"""
		out = []
		for v, lt in vals:
			if lt is None:
				if isinstance(subj, K):
					if subj.v not in [x for x, _t in every]:
						out.append(p.fork())
					continue
				q = p.fork()
				ok = True
				for x, _t in every:
					ok = ok and self.constrain(q, subj, '!=', x)
				if ok:
					q.guards.append((line, f'{text} not in [{", ".join(t for _x, t in every)}]', True))
					out.append(q)
				continue
			if isinstance(subj, K):
				if subj.v == v:
					out.append(p.fork())
				continue
			q = p.fork()
			if self.constrain(q, subj, '==', v):
				q.guards.append((line, f'{text} == {lt}', True))
				out.append(q)
		return out

	def exec_foreach(self, s, p):
		outs = []
		for q, it in self.eval(s.iterable, p):
			if not isinstance(it, K) or not isinstance(it.v, tuple):
				raise Unsupported(f'for-each over a non-constant array at line {self.line(s)}')
			cur = [('normal', q, None)]
			done = []
			for x in it.v:
				nxt = []
				for kind, r, v in cur:
					if kind != 'normal':
						done.append((kind, r, v))
						continue
					r.locals[s.name] = K(x)
					for kind2, r2, v2 in self.exec_stmt(s.body, r):
						if kind2 == 'break':
							done.append(('normal', r2, None))
						elif kind2 == 'continue':
							nxt.append(('normal', r2, None))
						else:
							nxt.append((kind2, r2, v2))
				cur = nxt
			outs += cur + done
		return outs

	# -- conditions -----------------------------------------------------------------------------------------------------------------
	def branch(self, e, p):
		"""[(path, outcome)] of a boolean expression with Java's short-circuit order; infeasible outcomes are dropped"""
		if isinstance(e, jast.Paren):
			return self.branch(e.expr, p)
		if isinstance(e, jast.Unary) and e.op == '!':
			return [(q, not b) for q, b in self.branch(e.expr, p)]
		if isinstance(e, jast.Binary) and e.op in ('&&', '||'):
			out = []
			for q, b in self.branch(e.left, p):
				if b == (e.op == '||'):
					out.append((q, b))
				else:
					out += self.branch(e.right, q)
			return out
		out = []
		text, line = self.jtext(e), self.line(e)
		for q, v in self.eval(e, p):
			out += [(r, b) for r, b in self.truth(v, q, text, line)]
		return out

	def truth(self, v, p, text, line):
		if isinstance(v, K):
			if not isinstance(v.v, bool):
				raise Unsupported(f'condition {text} is not boolean')
			return [(p, v.v)]
		if isinstance(v, Not):
			return [(q, not b) for q, b in self.truth(v.x, p, text, line)]
		if isinstance(v, BAnd):
			out = []
			for q, b in self.truth(v.a, p, f'{text} [{v.la}]' if v.la else text, line):
				out += [(q, False)] if not b else self.truth(v.b, q, f'{text} [{v.lb}]' if v.lb else text, line)
			return out
		if isinstance(v, Lazy):
			return self.truth(self.read(p, v.key), p, text, line)
		if isinstance(v, Res):
			out = []
			for b in (True, False):
				q = p.fork()
				q.assume[v.k] = b
				q.guards.append((line, text, b))
				out.append((q, b))
			return out
		if isinstance(v, In):
			v = Cmp('==', v, K(True))
		if isinstance(v, Cmp):
			out = []
			for b in (True, False):
				q = p.fork()
				if self.constrain(q, v.a, v.op if b else NEGATE[v.op], v.b.v if isinstance(v.b, K) else v.b):
					q.guards.append((line, text, b))
					out.append((q, b))
			return out
		raise Unsupported(f'condition {text}')

	def constrain(self, p, a, op, b):
		"""restrict the path so that `a op b` holds (a: an input, a linear offset of one, or a constant); False when infeasible"""
		if isinstance(b, (In, Ar)) and not isinstance(a, (In, Ar)):
			a, b, op = b, (a.v if isinstance(a, K) else a), MIRROR[op]
		if isinstance(a, K):
			return self.compare_values(a.v, op, b)
		if isinstance(b, (In, Ar, O, Res)):
			raise Unsupported('a guard over two inputs')
		while isinstance(a, Ar):
			if a.op in ('+', '-') and isinstance(a.b, K) and isinstance(b, int):
				b = b - a.b.v if a.op == '+' else b + a.b.v
				a = a.a
			elif a.op == '+' and isinstance(a.a, K) and isinstance(b, int):
				b, a = b - a.a.v, a.b
			else:
				raise Unsupported('a guard on arithmetic the extractor does not solve')
		if not isinstance(a, In):
			raise Unsupported(f'a guard on {a}')
		if a.key not in p.reads:
			p.reads.append(a.key)
		d = self.dom_of(p, a.key).restrict(op, b)
		if not d.sat():
			return False
		p.dom[a.key] = d
		return True

	@staticmethod
	def compare_values(x, op, y):
		if op == '==':
			return x == y
		if op == '!=':
			return x != y
		return {'<': x < y, '<=': x <= y, '>': x > y, '>=': x >= y}[op]

	def dom_of(self, p, key):
		if key in p.dom:
			return p.dom[key]
		head = key[0]
		if head == 'status':
			return Dom((None,) + tuple(Enum('QuestStatus', n) for n in self.t.enums['QuestStatus']))
		if head == 'target':
			return Dom(('npc', None))
		if head == 'targetId':
			return Dom(lo=1)
		if head == 'var':
			return Dom(lo=0, hi=63)                 # QuestVars: six 6-bit slots (QuestVars.java:22-58)
		if head in ('inv', 'completeCount'):
			return Dom(lo=0)
		if head in ('canRepeat',) or key in (('env', 'continuation'), ('player', 'mentor')):
			return Dom((True, False))
		if key == ('player', 'race'):
			return Dom(tuple(Enum('Race', n) for n in ('ELYOS', 'ASMODIANS')))
		if key == ('player', 'level'):
			return Dom(lo=1)
		if key == ('player', 'class'):
			return Dom(tuple(Enum('PlayerClass', n) for n in self.t.enums['PlayerClass']))
		if key == ('player', 'gender'):
			return Dom(tuple(Enum('Gender', n) for n in self.t.enums['Gender']))
		return Dom()

	def read(self, p, key, default=None):
		"""the value of a state key on the path: a write, else the input (or `default`); refused when an unmodelled helper may have
		written it"""
		if key in p.state:
			return p.state[key]
		by = p.clobbered.get(category(key)) or p.clobbered.get(('all',))
		if by is not None:
			raise Unsupported(f'reads {"/".join(map(str, key))} after {by}, which may have written it')
		if default is not None:
			return default
		if key not in p.reads:
			p.reads.append(key)
		return In(key)

	def write(self, p, key, v):
		p.state[key] = v

	def env_quest(self, p):
		"""env.getQuestId() on the path: the handler's quest unless env.setQuestId wrote another; None when it is not known (a written
		value that is not a constant, or a helper that may have set it: sendQuestEndDialog does, AbstractQuestHandler.java:428,447)"""
		if ('env', 'questId') in p.state:
			v = p.state[('env', 'questId')]
			return v.v if isinstance(v, K) and isinstance(v.v, int) and not isinstance(v.v, bool) else None
		return None if ('all',) in p.clobbered else self.qid

	def clobber(self, p, helper, cats):
		"""after a helper whose Java body may write these categories, their values are unknown: a later read refuses the hook. 'quest'
		is the handler's quest (the AbstractQuestHandler helpers use the questId field) and env.getQuestId()'s (QuestService.startQuest,
		which sendQuestStartDialog calls, uses the env's)"""
		flags = []
		for c in cats:
			if c == 'all':
				flags.append(('all',))
			elif c == 'quest':
				env_q = self.env_quest(p)
				if env_q is None:
					flags.append(('all',))
					continue
				for q in sorted({self.qid, env_q}):
					flags += [('quest', q), ('vars', q)]
			elif c == 'inventory':
				flags.append(('inventory',))
		# a helper that only changes a quest's state (changeQuestStep, sendQuestStartDialog) never removes an existing QuestState
		keeps = [] if ('all',) in flags else [f[1] for f in flags if f[0] == 'quest' and self.exists(p, f[1])]
		for f in flags:
			p.clobbered[f] = helper
		for k in list(p.state):
			if ('all',) in flags or category(k) in flags:
				del p.state[k]
		for q in keeps:
			p.state[('exists', q)] = True

	def exists(self, p, q):
		"""the path already knows the QuestState of quest q is not null"""
		if p.state.get(('exists', q)):
			return True
		v = p.state.get(('status', q))
		if isinstance(v, K):
			return v.v is not None
		return ('status', q) in p.dom and not p.dom[('status', q)].restrict('==', None).sat()

	# -- expressions ----------------------------------------------------------------------------------------------------------------
	def eval(self, e, p):
		"""[(path, value)]: most expressions have one; a conditional, a null dereference or a target lookup fork"""
		if isinstance(e, jast.Lit):
			return [(p, K(self.literal(e)))]
		if isinstance(e, jast.Name):
			return [(p, self.name(e, p))]
		if isinstance(e, jast.Paren):
			return self.eval(e.expr, p)
		if isinstance(e, jast.FieldAccess):
			return self.field_access(e, p)
		if isinstance(e, jast.Call):
			return self.call(e, p)
		if isinstance(e, jast.New):
			return self.new(e, p)
		if isinstance(e, (jast.ArrayInit, jast.NewArray)):
			items = e.items if isinstance(e, jast.ArrayInit) else e.init.items
			vals = []
			for it in items:
				r = self.eval(it, p)
				if len(r) != 1 or not isinstance(r[0][1], K):
					raise Unsupported('an array initializer that is not constant')
				vals.append(r[0][1].v)
			return [(p, K(tuple(vals)))]
		if isinstance(e, jast.Index):
			out = []
			for q, a in self.eval(e.target, p):
				for r, i in self.eval(e.index, q):
					if not (isinstance(a, K) and isinstance(a.v, tuple) and isinstance(i, K)):
						raise Unsupported(f'index {self.jtext(e)}')
					if not 0 <= i.v < len(a.v):
						raise Unsupported(f'index {self.jtext(e)} out of range (ArrayIndexOutOfBoundsException)')
					out.append((r, K(a.v[i.v])))
			return out
		if isinstance(e, jast.Cast):
			return self.cast(e, p)
		if isinstance(e, jast.Unary):
			return self.unary(e, p)
		if isinstance(e, jast.Binary):
			if e.op in ('&&', '||'):
				return [(q, K(b)) for q, b in self.branch(e, p)]
			return self.binary(e, p)
		if isinstance(e, jast.Cond):
			out = []
			for q, b in self.branch(e.cond, p):
				out += self.eval(e.a if b else e.b, q)
			return out
		if isinstance(e, jast.Assign):
			return self.assign(e, p)
		if isinstance(e, jast.SwitchExpr):
			return self.switch_expr(e, p)
		if isinstance(e, jast.Closure):
			if e.params:
				raise Unsupported(f'a {e.kind} with parameters')
			if e.kind == 'anonymous-class' and (e.iface, e.method) != ('Runnable', 'run'):
				raise Unsupported(f'an anonymous {e.iface}')
			return [(p, Task(e, dict(p.locals)))]
		if isinstance(e, jast.InstanceOf):
			if e.binding:
				raise Unsupported('instanceof pattern')
			out = []
			for q, v in self.eval(e.expr, p):
				if v in (O('target'), O('npc')) and e.type.name == 'Npc':
					out.append((q, Cmp('==', self.read_input(q, ('target',)), K('npc'))))
				else:
					raise Unsupported(f'instanceof {e.type.name}')
			return out
		raise Unsupported(f'{type(e).__name__} expression')

	def read_input(self, p, key):
		if key not in p.reads:
			p.reads.append(key)
		return In(key)

	def literal(self, e):
		k, t = e.kind, e.text
		if k in ('int', 'long'):
			t = t.replace('_', '').rstrip('lL')
			if t.lower().startswith('0x'):
				return int(t, 16)
			if t.lower().startswith('0b'):
				return int(t[2:], 2)
			return int(t, 8) if len(t) > 1 and t.startswith('0') else int(t)
		if k in ('float', 'double'):
			return float(t.replace('_', '').rstrip('fFdD'))
		if k == 'bool':
			return t == 'true'
		if k == 'null':
			return None
		return t                                    # string and char literals as written

	def name(self, e, p):
		n = e.name
		if n in p.locals:
			v = p.locals[n]
			if v is None:
				raise Unsupported(f'local {n} read before it is assigned')
			if isinstance(v, In) and v.key not in p.reads:
				p.reads.append(v.key)                   # a hook argument
			return v
		if n in self.consts:
			v = self.consts[n]
			if isinstance(v, Unsupported):
				raise v
			return v
		if n == 'questId':
			return K(self.qid)
		if n == 'qe':
			return O('qe')
		if n in self.t.dialog.actions:
			return K(self.t.dialog.actions[n])       # DialogAction.* (static import)
		if n in ('workItems', 'actionItems'):
			raise Unsupported(f'AbstractQuestHandler.{n}')
		return O(('class', n))

	def field_access(self, e, p):
		if isinstance(e.target, jast.Name) and e.target.name not in p.locals and e.target.name not in self.consts:
			cls = e.target.name
			if cls == 'DialogAction' and e.name in self.t.dialog.actions:
				return [(p, K(self.t.dialog.actions[e.name]))]
			if cls in self.t.enums and e.name in self.t.enums[cls]:
				return [(p, K(Enum(cls, e.name)))]
			raise Unsupported(f'{cls}.{e.name}')
		out = []
		for q, t in self.eval(e.target, p):
			if e.name == 'length' and isinstance(t, K) and isinstance(t.v, tuple):
				out.append((q, K(len(t.v))))
			else:
				raise Unsupported(f'field {self.jtext(e)}')
		return out

	def eval_args(self, args, p):
		"""[(path, [values])], evaluated left to right"""
		outs = [(p, [])]
		for a in args:
			nxt = []
			for q, vals in outs:
				for r, v in self.eval(a, q):
					nxt.append((r, vals + [v]))
			outs = nxt
		return outs

	def cast(self, e, p):
		out = []
		for q, v in self.eval(e.expr, p):
			ty = e.type.name
			if ty in INT_BITS:
				if isinstance(v, K) and isinstance(v.v, (int, float)) and not isinstance(v.v, bool):
					out.append((q, K(java_narrow(v.v, ty))))
				elif ty in ('int', 'long') and isinstance(v, (In, Ar, Res)):
					out.append((q, v))                  # an int input stays itself (a long input has int values in the handlers)
				else:
					raise Unsupported(f'cast to {ty} of {self.jtext(e.expr)}')
			elif ty in ('float', 'double'):
				out.append((q, K(float(v.v)) if isinstance(v, K) else v))
			elif ty == 'Npc' and v == O('target'):
				out.append((q, O('npc')))
			else:
				raise Unsupported(f'cast to {ty}')
		return out

	def unary(self, e, p):
		op = e.op
		if op in ('++', '--'):
			if not isinstance(e.expr, jast.Name) or e.expr.name not in p.locals:
				raise Unsupported(f'{op} on {self.jtext(e.expr)}')
			old = p.locals[e.expr.name]
			new = self.arith('+' if op == '++' else '-', old, K(1))
			p.locals[e.expr.name] = new
			return [(p, old if e.postfix else new)]
		out = []
		for q, v in self.eval(e.expr, p):
			if op == '!':
				out.append((q, K(not v.v) if isinstance(v, K) else Not(v)))
			elif op == '-' and isinstance(v, K) and isinstance(v.v, float):
				out.append((q, K(-v.v)))                    # a negative floating literal (negation is exact)
			elif op == '-':
				out.append((q, self.arith('-', K(0), v)))
			elif op == '+':
				out.append((q, v))
			else:
				raise Unsupported(f'unary {op}')
		return out

	def arith(self, op, a, b):
		if any(isinstance(x, K) and isinstance(x.v, float) for x in (a, b)):
			raise Unsupported(f'floating-point arithmetic {op}')     # Java float is 32-bit; Python has only double
		if isinstance(a, K) and isinstance(b, K) and isinstance(a.v, int) and isinstance(b.v, int):
			x, y = a.v, b.v
			if op == '+':
				return K(java_int(x + y))
			if op == '-':
				return K(java_int(x - y))
			if op == '*':
				return K(java_int(x * y))
			if op in ('/', '%'):
				if y == 0:
					raise Unsupported('division by zero')
				q = abs(x) // abs(y) * (1 if (x >= 0) == (y >= 0) else -1)
				return K(q if op == '/' else x - q * y)
		if op in ('+', '-', '*', '/', '%') and all(isinstance(v, (K, In, Ar)) for v in (a, b)):
			return Ar(op, a, b)
		raise Unsupported(f'arithmetic {op} on {a} and {b}')

	def binary(self, e, p):
		out = []
		for q, a in self.eval(e.left, p):
			for r, b in self.eval(e.right, q):
				out.append((r, self.combine(e.op, a, b, r)))
		return out

	def combine(self, op, a, b, p):
		if op in ('==', '!='):
			for x, y in ((a, b), (b, a)):
				if isinstance(x, O) and isinstance(y, K) and y.v is None:
					return self.null_check(x, op, p)
			if isinstance(a, K) and isinstance(b, K):
				return K((a.v == b.v) == (op == '=='))
			return Cmp(op, a, b)
		if op in ('<', '>', '<=', '>='):
			if isinstance(a, K) and isinstance(b, K):
				return K(self.compare_values(a.v, op, b.v))
			return Cmp(op, a, b)
		return self.arith(op, a, b)

	def null_check(self, obj, op, p):
		"""`x == null` for the objects that may be null: a QuestState (the status input) and the target"""
		if isinstance(obj.name, tuple) and obj.name[0] == 'qs':
			key = ('status', obj.name[1])
			return Cmp(op, self.read(p, key), K(None))
		if obj.name in ('target', 'npc'):
			return Cmp('!=' if op == '==' else '==', self.read_input(p, ('target',)), K('npc'))
		if obj.name in ('env', 'player', 'qsl', 'inventory', 'item'):
			return K(op == '!=')                      # never null in a hook
		raise Unsupported(f'null check on {obj.name}')

	def assign(self, e, p):
		if not isinstance(e.target, jast.Name) or e.target.name not in p.locals:
			raise Unsupported(f'assignment to {self.jtext(e.target)}')
		out = []
		for q, v in self.eval(e.value, p):
			if e.op != '=':
				v = self.arith(e.op[:-1], q.locals[e.target.name], v)
			q.locals[e.target.name] = v
			out.append((q, v))
		return out

	# -- objects that may be null ---------------------------------------------------------------------------------------------------
	def deref(self, p, obj):
		"""[path] on which obj is not null; the null alternative becomes a NullPointerException leaf of the hook"""
		if isinstance(obj.name, tuple) and obj.name[0] == 'qs':
			if p.state.get(('exists', obj.name[1])):
				return [p]
			key = ('status', obj.name[1])
			v = self.read(p, key)
			if isinstance(v, K):
				if v.v is None:
					self.throw(p, 'NullPointerException')
					return []
				return [p]
			d = self.dom_of(p, key)
			out = []
			if d.restrict('==', None).sat() and self.mode == 'hook':
				q = p.fork()
				q.dom[key] = d.restrict('==', None)
				self.throw(q, 'NullPointerException')
			d2 = d.restrict('!=', None)
			if d2.sat():
				p.dom[key] = d2
				out.append(p)
			return out
		if obj.name in ('target', 'npc'):
			key = ('target',)
			self.read_input(p, key)
			d = self.dom_of(p, key)
			if d.restrict('==', None).sat():
				q = p.fork()
				q.dom[key] = d.restrict('==', None)
				self.throw(q, 'NullPointerException')
			d2 = d.restrict('==', 'npc')
			if not d2.sat():
				return []
			p.dom[key] = d2
			return [p]
		return [p]

	def throw(self, p, exc):
		if p.task is not None or p.deferred:
			# Java: the pool logs a task's exception (the hook has returned), and a hook that throws after a schedule still runs the task
			raise Unsupported(f'a {exc} with a scheduled task')
		self.leaves.append(('throw', p, exc))
		self.budget(0)

	# -- calls ----------------------------------------------------------------------------------------------------------------------
	def call(self, e, p):
		name, tgt = e.name, e.target
		if tgt is None or (isinstance(tgt, jast.Name) and tgt.name == 'this'):
			if name in self.own:
				return self.inline(self.own[name], e, p)
			if name == 'getQuestId':
				return self.after_args(e, p, lambda q, a: [(q, K(self.qid))])
			if name in HELPERS:
				return self.helper(name, e, p)
			raise Unsupported(f'call {name}')
		if isinstance(tgt, jast.Name) and tgt.name == 'super':
			if name == 'onDialogEvent':
				return self.helper('super.onDialogEvent', e, p)
			raise Unsupported(f'super.{name}')
		if isinstance(tgt, jast.Name) and tgt.name not in p.locals and tgt.name not in self.consts and tgt.name not in ('qe', 'questId'):
			return self.static_call(tgt.name, e, p)
		if isinstance(tgt, jast.FieldAccess) and isinstance(tgt.target, jast.Name) and tgt.target.name == 'DialogPage' and name == 'id':
			page = tgt.name
			if page not in self.t.dialog.pages:
				raise Unsupported(f'DialogPage.{page}')
			return [(p, K(self.t.dialog.pages[page]))]
		if isinstance(tgt, jast.FieldAccess) and isinstance(tgt.target, jast.Name) and tgt.target.name == 'WorldMapType' and name == 'getId' \
				and tgt.target.name not in p.locals and not e.args:
			if tgt.name not in self.t.world_maps:
				raise Unsupported(f'WorldMapType.{tgt.name}')
			return [(p, K(self.t.world_maps[tgt.name]))]
		out = []
		for q, recv in self.eval(tgt, p):
			if isinstance(recv, RewardPage) and name == 'id':
				out.append((q, recv))
				continue
			if not isinstance(recv, O):
				raise Unsupported(f'call {self.jtext(e)} on a value')
			# JLS 15.12.4: the target, then the arguments, then the NullPointerException of a null target
			for r, args in self.eval_args(e.args, q):
				for s in self.deref(r, recv):
					out += self.method(recv, name, args, s, e)
		return out

	def after_args(self, e, p, fn):
		out = []
		for q, args in self.eval_args(e.args, p):
			out += fn(q, args)
		return out

	def method(self, recv, name, args, p, e):
		o = recv.name
		if self.mode == 'register':
			return self.register_call(recv, name, args, p)
		if o == 'env':
			if name == 'getPlayer':
				return [(p, O('player'))]
			if name == 'getVisibleObject':
				return [(p, O('target'))]
			if name == 'getTargetId':
				# QuestEnv.getTargetId (QuestEnv.java:94-96): the target's template id, 0 without a target
				out = []
				key = ('target',)
				self.read_input(p, key)
				d = self.dom_of(p, key)
				if d.restrict('==', None).sat():
					q = p.fork()
					q.dom[key] = d.restrict('==', None)
					out.append((q, K(0)))
				if d.restrict('==', 'npc').sat():
					q = p.fork()
					q.dom[key] = d.restrict('==', 'npc')
					out.append((q, self.read_input(q, ('targetId',))))
				return out
			if name == 'getDialogActionId':
				return [(p, self.read(p, ('dialog',)))]
			if name == 'getQuestId':
				return [(p, self.read(p, ('env', 'questId'), default=K(self.qid)))]   # the engine sets it to the handler's quest
			if name == 'getExtendedRewardIndex':
				return [(p, self.read_input(p, ('env', 'extendedRewardIndex')))]
			if name == 'isDialogContinuationFromPreQuest':
				return [(p, self.read_input(p, ('env', 'continuation')))]
			if name in ('setQuestId', 'setDialogActionId'):
				self.effect(p, f'env.{name}', args, 'env', e)
				self.write(p, ('env', 'questId') if name == 'setQuestId' else ('dialog',), args[0])
				return [(p, K(None))]
		if o == 'player':
			if name in ('getQuestStateList', 'getInventory', 'getCommonData'):
				return [(p, O({'getQuestStateList': 'qsl', 'getInventory': 'inventory', 'getCommonData': 'player'}[name]))]
			attr = {'getRace': 'race', 'getLevel': 'level', 'getPlayerClass': 'class', 'getGender': 'gender', 'isMentor': 'mentor',
			        'getWorldId': 'worldId'}.get(name)
			if attr is not None:
				return [(p, self.read_input(p, ('player', attr)))]
			if name == 'getObjectId':
				return [(p, Opaque('$playerObjectId'))]
		if o == 'qsl':
			q_id = self.const_int(args[0], 'a quest id')
			if name == 'getQuestState':
				return [(p, O(('qs', q_id)))]
			if name == 'hasQuest':
				return [(p, Cmp('!=', self.read(p, ('status', q_id)), K(None)))]
		if isinstance(o, tuple) and o[0] == 'qs':
			return self.quest_state(o[1], name, args, p, e)
		if isinstance(o, tuple) and o[0] == 'questVars':
			if name == 'getVarById':
				return [(p, self.read(p, ('var', o[1], self.const_int(args[0], 'a var slot'))))]
			if name == 'setVarById':
				return self.quest_state(o[1], 'setQuestVarById', args, p, e)
		if o == 'inventory' and name == 'getItemCountByItemId':
			return [(p, self.read(p, ('inv', self.const_int(args[0], 'an item id'))))]
		if o == 'inventory' and name == 'decreaseByObjectId' and len(args) == 2:
			# Storage.decreaseByObjectId(itemObjId, count) (Storage.java): the item of that object id, when the inventory holds it
			k = self.effect(p, 'inventory.decreaseByObjectId', args, 'item', e)
			self.clobber(p, 'inventory.decreaseByObjectId', ('inventory',))
			return [(p, Res(k))]
		if o == 'threadPool' and name == 'schedule':
			# ThreadPoolManager.schedule(Runnable, long delay) (ThreadPoolManager.java): the task runs after the hook (run_tasks); the effect
			# records the delay, the task's effects carry its index
			if len(args) != 2 or not isinstance(args[0], Task) or not isinstance(args[1], K) or not isinstance(args[1].v, int):
				raise Unsupported(f'call {self.jtext(e)}')
			if p.task is not None:
				raise Unsupported('a task scheduled by a scheduled task')
			k = self.effect(p, 'ThreadPoolManager.schedule', [args[1]], 'task', e)
			p.deferred.append((args[0], args[1].v, k))
			return [(p, Opaque('$future'))]
		if o in ('target', 'npc'):
			if name == 'getObjectId':
				return [(p, Opaque('$targetObjectId'))]
			if name == 'getNpcId' and o == 'npc':
				return [(p, self.read_input(p, ('targetId',)))]
		if o == 'item':
			if name == 'getItemId':
				return [(p, self.read_input(p, ('item', 'itemId')))]
			if name == 'getObjectId':
				return [(p, Opaque('$itemObjectId'))]
			if name == 'getItemTemplate':
				return [(p, O('itemTemplate'))]
		if o == 'itemTemplate' and name == 'getTemplateId':
			return [(p, self.read_input(p, ('item', 'itemId')))]         # Item.getItemId is getItemTemplate().getTemplateId()
		raise Unsupported(f'call {self.jtext(e)}')

	@staticmethod
	def const_int(v, what):
		if not isinstance(v, K) or not isinstance(v.v, int):
			raise Unsupported(f'{what} that is not a constant')
		return v.v

	def quest_state(self, q, name, args, p, e):
		"""QuestState (QuestState.java) and its QuestVars (QuestVars.java)"""
		if name == 'getStatus':
			return [(p, self.read(p, ('status', q)))]
		if name == 'getQuestVarById':
			return [(p, self.read(p, ('var', q, self.const_int(args[0], 'a var slot'))))]
		if name == 'getQuestVars':
			return [(p, O(('questVars', q)))]
		if name == 'isStartable':
			# QuestState.isStartable (QuestState.java:117-119): COMPLETE and canRepeat
			return [(p, BAnd(Cmp('==', self.read(p, ('status', q)), K(Enum('QuestStatus', 'COMPLETE'))), Lazy(('canRepeat', q)),
			                 'status == COMPLETE', 'canRepeat'))]
		if name == 'canRepeat':
			return [(p, self.read(p, ('canRepeat', q)))]
		if name == 'getRewardGroup':
			return [(p, self.read(p, ('rewardGroup', q)))]
		if name == 'getCompleteCount':
			return [(p, self.read(p, ('completeCount', q)))]
		if name == 'getQuestId':
			return [(p, K(q))]
		if q != self.qid:
			raise Unsupported(f'{name} on the state of quest {q}')
		if name == 'setQuestVarById':
			slot = self.const_int(args[0], 'a var slot')
			self.effect(p, 'qs.setQuestVarById', args, 'var', e)
			self.write(p, ('var', q, slot), args[1])
			return [(p, K(None))]
		if name == 'setQuestVar':
			# QuestVars.setVar (QuestVars.java:52-58): the six 6-bit slots of the packed value
			self.effect(p, 'qs.setQuestVar', args, 'var', e)
			v = args[0]
			if not isinstance(v, K):
				# lane C (phase 6 step 1): a symbolic value the path bounds to one slot's range (`int var = qs.getQuestVarById(0); if (var == 2)
				# qs.setQuestVar(var + 1)`) is slot 0, the other five slots 0; later guards only narrow the bounds
				lo, hi = self.bounds(v, p)
				if not (0 <= lo and hi <= 63):
					self.unknown(f'qs.setQuestVar({v})')
				for slot in range(6):
					self.write(p, ('var', q, slot), v if slot == 0 else K(0))
				return [(p, K(None))]
			for slot in range(6):
				self.write(p, ('var', q, slot), K((v.v >> (6 * slot)) & 0x3F))
			return [(p, K(None))]
		if name == 'setStatus':
			self.effect(p, 'qs.setStatus', args, 'status', e)
			self.write(p, ('status', q), args[0])
			return [(p, K(None))]
		if name == 'setRewardGroup':
			self.effect(p, 'qs.setRewardGroup', args, 'rewardGroup', e)
			self.write(p, ('rewardGroup', q), args[0])
			return [(p, K(None))]
		raise Unsupported(f'QuestState.{name}')

	def bounds(self, v, p):
		"""(lo, hi) of an int value on the path: a constant, an input's domain, or + and - of those (inf when unbounded)"""
		if isinstance(v, K) and isinstance(v.v, int) and not isinstance(v.v, bool):
			return v.v, v.v
		if isinstance(v, In):
			d = self.dom_of(p, v.key)
			if d.allowed is not None:
				ints = [x for x in d.allowed if isinstance(x, int) and not isinstance(x, bool) and d.ok(x)]
				return (min(ints), max(ints)) if ints else (-INF, INF)
			return d.lo, d.hi
		if isinstance(v, Ar) and v.op in ('+', '-'):
			(alo, ahi), (blo, bhi) = self.bounds(v.a, p), self.bounds(v.b, p)
			return (alo + blo, ahi + bhi) if v.op == '+' else (alo - bhi, ahi - blo)
		return -INF, INF

	@staticmethod
	def unknown(what):
		raise Unsupported(f'a symbolic {what}')

	def static_call(self, cls, e, p):
		name = e.name
		if cls == 'QuestService' and f'QuestService.{name}' in HELPERS:
			return self.helper(f'QuestService.{name}', e, p)
		if cls == 'PacketSendUtility' and name == 'sendPacket':
			def send(q, args):
				kind = 'dialog' if isinstance(args[1], New) and args[1].cls == 'SM_DIALOG_WINDOW' else 'packet'
				self.effect(q, 'PacketSendUtility.sendPacket', args, kind, e)
				return [(q, K(None))]
			return self.after_args(e, p, send)
		if cls == 'HandlerResult' and name == 'fromBoolean':
			return self.after_args(e, p, lambda q, a: [(q, FromBool(a[0]))])
		if cls == 'ZoneName' and name == 'get':
			return self.after_args(e, p, lambda q, a: [(q, K(Enum('ZoneName', str(a[0].v).strip('"'))))])
		if cls == 'DialogPage' and name == 'getRewardPageByIndex':
			return self.after_args(e, p, lambda q, a: [(q, RewardPage(a[0]))])
		if cls == 'ThreadPoolManager' and name == 'getInstance' and not e.args:
			return [(p, O('threadPool'))]
		if cls == 'PacketSendUtility' and name == 'broadcastPacket':
			# PacketSendUtility.broadcastPacket(player, packet, toSelf) (PacketSendUtility.java:88-93): the player's known players and, with
			# toSelf, the player; the quester has no other player in sight in a harness, so only toSelf shows
			def broadcast(q, args):
				if len(args) != 3 or args[0] != O('player') or not isinstance(args[1], New) or not isinstance(args[2], K):
					raise Unsupported(f'call {self.jtext(e)}')
				self.effect(q, 'PacketSendUtility.broadcastPacket', args[1:], 'packet', e)
				return [(q, K(None))]
			return self.after_args(e, p, broadcast)
		raise Unsupported(f'call {cls}.{name}')

	def new(self, e, p):
		cls = e.type.name
		if cls in ('SM_DIALOG_WINDOW', 'SM_PLAY_MOVIE', 'SM_EMOTION', 'SM_QUEST_ACTION', 'SM_ITEM_USAGE_ANIMATION'):
			return self.after_args(e, p, lambda q, a: [(q, New(cls, tuple(a)))])
		raise Unsupported(f'new {cls}')

	def helper(self, name, e, p):
		kind, cats = HELPERS[name]

		def run(q, args):
			if name in VARARGS and len(args) == 2 and isinstance(args[1], K) and isinstance(args[1].v, tuple):
				args = [args[0]] + [K(x) for x in args[1].v]
			if name == 'QuestService.startQuest':
				# QuestService.startQuest (QuestService.java:400-444) acts on env.getQuestId(): false changes nothing; true sets the status to
				# START (a new QuestState or the old one's status), sends SM_QUEST_ACTION; the vars of an old state are kept, those of a new
				# one are 0, so they are unknown here
				target = self.env_quest(q)
				if target is None:
					raise Unsupported('QuestService.startQuest when env.getQuestId() is not known')
				k = self.effect(q, name, args, kind, e)
				outs = []
				for b in (True, False):
					r = q.fork()
					r.assume[k] = b
					r.guards.append((self.line(e), self.jtext(e), b))
					if b:
						self.write(r, ('status', target), K(Enum('QuestStatus', 'START')))
						r.clobbered[('vars', target)] = name
						for key in [key for key in r.state if category(key) == ('vars', target)]:
							del r.state[key]
					outs.append((r, K(b)))
				return outs
			k = self.effect(q, name, args, kind, e)
			self.clobber(q, name, cats)
			return [(q, Res(k))]
		return self.after_args(e, p, run)

	def effect(self, p, call, args, kind, e):
		p.effects.append((call, list(args), kind, self.line(e)))
		if p.task is not None:
			p.task_of[len(p.effects) - 1] = p.task
		return len(p.effects) - 1

	def inline(self, m, e, p):
		"""a call of one of the handler's own methods: its body runs on the path (its effects are the hook's)"""
		if self.depth > 8:
			raise Unsupported(f'recursion through {m.name}')
		if len(m.params) != len(e.args):
			raise Unsupported(f'overloaded own method {m.name}')
		out = []
		for q, args in self.eval_args(e.args, p):
			saved = dict(q.locals)
			q.locals = {prm.name: v for prm, v in zip(m.params, args)}
			self.depth += 1
			try:
				res = self.exec_stmts(self.p.block_stmts(m.body.start), q)
			finally:
				self.depth -= 1
			for kind, r, v in res:
				if kind in ('break', 'continue'):
					raise Unsupported(f'{kind} outside a loop in {m.name}')
				r.locals = dict(saved)
				out.append((r, v if kind == 'return' else K(None)))
		return out

	def register_call(self, recv, name, args, p):
		o = recv.name
		if o == 'qe':
			if name == 'registerQuestNpc':
				return [(p, O(('questNpc', self.const_int(args[0], 'an npc id'))))]
			self.registrations.append({'call': name, 'args': [self.render(a, {}, p) for a in args]})
			return [(p, K(None))]
		if isinstance(o, tuple) and o[0] == 'questNpc':
			self.registrations.append({'npc': o[1], 'event': name, 'args': [self.render(a, {}, p) for a in args]})
			return [(p, K(None))]
		raise Unsupported(f'register(): call {name}')

	# -- the case -------------------------------------------------------------------------------------------------------------------
	def pick_all(self, p):
		out = {}
		for key in p.reads:
			d = self.dom_of(p, key)
			if key[0] == 'arg' and self.arg_types.get(key[1]) in NON_INT_ARGS and d.allowed is None:
				out[key] = {'anyExcept': sorted((x.name if isinstance(x, Enum) else x) for x in d.excluded)}
				continue
			prefer = ()
			if key == ('targetId',):
				prefer = tuple(self.reg_npcs) + OTHER_NPCS
			elif key == ('item', 'itemId'):
				prefer = tuple(self.reg_items)
			elif key == ('dialog',) and d.allowed is None:
				prefer = tuple(self.t.dialog.actions.values())
			elif key == ('player', 'worldId'):
				prefer = tuple(self.t.world_maps.values())     # a map that exists (the first of WorldMapType the guards allow)
			v = d.pick(prefer)
			if v is _EMPTY:
				raise OracleError(f'{self.rel}: an empty domain for {key} on a feasible path')
			out[key] = v
		return out

	def ranges(self, p):
		"""({input: [lo, hi]}, {input: [excluded]}) for the inputs an ordering guard bounded on both ends (other than a QuestVars slot's
		own 0..63). lo and hi satisfy every guard: an end a `!=` guard excludes moves inward (`!(var >= 1 && var < 10) && var != 10` is
		[11, 63], not [10, 63]), so `given` holds lo and a harness can check hi too; the values inside that a guard excludes are listed"""
		out, gaps = {}, {}
		for key in p.reads:
			d = p.dom.get(key)
			if d is None or d.allowed is not None:
				continue
			base = self.dom_of(SPath(), key)
			if d.lo == -INF or d.hi == INF or (d.lo, d.hi) == (base.lo, base.hi):
				continue
			lo, hi = int(d.lo), int(d.hi)
			while lo <= hi and not d.ok(lo):
				lo += 1
			while hi >= lo and not d.ok(hi):
				hi -= 1
			if lo < hi:
				out[self.key_name(key)] = [lo, hi]
				inside = sorted(x for x in d.excluded if isinstance(x, int) and not isinstance(x, bool) and lo < x < hi)
				if inside:
					gaps[self.key_name(key)] = inside
		return out, gaps

	def key_name(self, key):
		head = key[0]
		if head == 'var':
			return f'{"questState" if key[1] == self.qid else f"otherQuests.{key[1]}"}.vars.{key[2]}'
		if head == 'inv':
			return f'inventory.{key[1]}'
		return '.'.join(map(str, key))

	def dead_assumption(self, p):
		"""True when the path assumes a helper result the helper's Java body cannot return, so the path is dead Java code (P6-Q, 2026-09-29:
		the golden harness found the 9 such cases of the first slice). `assume` is otherwise free. giveQuestItem(env, itemId, itemCount) with
		constants itemId != 0 and itemCount != 0 returns true on both of its paths (AbstractQuestHandler.java:626-641); QuestService.
		collectItemCheck(env, true) returns false when the player has no QuestState of env's quest (QuestService.java:557-561), which a
		path shows as that quest's status read as absent. removeQuestItem(env, itemId, itemCount) with constants itemId != 0 and itemCount > 0
		returns true when the player holds at least itemCount of the item (AbstractQuestHandler.java:644-651, Storage.java:255-267:
		decreaseByItemId takes the count from the stacks and answers count == 0), which a path shows as a guard that bounds the item's count
		from below by itemCount (P6-Q slice 2, Q03: _1535TheColdColdGround's `count > 4 && removeQuestItem(env, id, 5)`)"""
		values = None
		for k, b in p.assume.items():
			call, args, _kind, _line = p.effects[k]
			args = [a for a in args if a not in IMPLICIT]
			if call == 'giveQuestItem' and not b and len(args) == 2 and all(
					isinstance(a, K) and isinstance(a.v, int) and not isinstance(a.v, bool) and a.v != 0 for a in args):
				return True
			if call == 'removeQuestItem' and not b and len(args) == 2 and all(
					isinstance(a, K) and isinstance(a.v, int) and not isinstance(a.v, bool) for a in args) and args[0].v != 0 and args[1].v > 0:
				d = p.dom.get(('inv', args[0].v))
				if d is not None and d.lo >= args[1].v:
					return True
			if call == 'QuestService.collectItemCheck' and b and args == [K(True)]:
				target = self.env_quest(p)
				if values is None:
					values = self.pick_all(p)
				if target is not None and ('status', target) in values and values[('status', target)] is None:
					return True
		return False

	def case(self, m, kind, p, v):
		values = self.pick_all(p)
		c = {'id': None, 'hook': m.name, 'given': self.given(values), 'guards': [f'{t} -> {str(b).lower()} @{ln}' for ln, t, b in p.guards]}
		rg, gaps = self.ranges(p)
		if rg:
			c['ranges'] = rg
		if gaps:
			c['rangeExcludes'] = gaps
		free = self.free_vars(p, v)
		if free:
			c['free'] = free
		excludes = self.dialog_excludes(p, v)
		if excludes is not None:
			c['dialogExcludes'] = excludes
		if p.assume:
			c['assume'] = [{'effect': k, 'returns': b} for k, b in sorted(p.assume.items())]
		c.update(self.outcome(kind, p, v, values))
		# P6-Q (2026-09-29, the route-gen review): the same path with one ranged input at its high end, the others as given. `given` holds the
		# low end; a boundary moved by one (`var < 6` read as `var < 5`, `itemCount >= 3` as `>= 1`) or an expression replaced by the constant
		# it has at the low end (`var + 1` as 2) keeps every low-end case and fails here. hi satisfies every guard of the path (ranges()).
		high = []
		for key in p.reads:
			name = self.key_name(key)
			if name not in rg:
				continue
			at = dict(values)
			at[key] = rg[name][1]
			high.append({'input': name, 'value': rg[name][1], 'given': self.given(at), **self.outcome(kind, p, v, at)})
		if high:
			c['atHigh'] = high
		return c

	def free_vars(self, p, v):
		"""the QuestVars slots the path reads with no guard on them and uses in no effect argument and not in the return value: any value
		takes the same path with the same effects, so a harness may set one to what a helper the path calls reads (P6-Q, the route-gen review:
		`checkQuestItems(env, 1, ...)` after an unguarded `int var = qs.getQuestVarById(0)` acts only at var 1, and given holds 0)"""
		used = self.used_keys(p, v)
		return [self.key_name(k) for k in p.reads if k[0] == 'var' and k not in p.dom and k not in used]

	def dialog_excludes(self, p, v):
		"""the dialog actions a path excludes when it reads the dialog action only through `!=` guards (the else branch of
		`if (action == QUEST_SELECT) ... else return sendQuestStartDialog(env)`, a switch's default), sorted; None for any other path. given
		holds one representative (the first action of the table the guards allow: USE_OBJECT), with which the start and end helpers do
		nothing; any action outside the list takes the same path with the same effects when the path neither writes the dialog action nor
		uses it in an effect argument or the return value, so a harness may run the case with the actions those helpers act on (P6-Q slice 2,
		the Q10 review: `sendQuestStartDialog` was vacuous in 105 cases, and a start branch returning false passed)"""
		key = ('dialog',)
		if key not in p.reads or key in p.state or key in self.used_keys(p, v):
			return None
		d = self.dom_of(p, key)
		if d.allowed is not None or d.lo != -INF or d.hi != INF:
			return None
		return sorted(x for x in d.excluded if isinstance(x, int) and not isinstance(x, bool))

	def used_keys(self, p, v):
		"""the inputs an effect argument or the return value of the path reads"""
		used = set()

		def walk(x):
			if isinstance(x, (In, Lazy)):
				used.add(x.key)
			elif isinstance(x, (Ar, Cmp, BAnd)):
				walk(x.a)
				walk(x.b)
			elif isinstance(x, (Not, FromBool, RewardPage)):
				walk(x.x)
			elif isinstance(x, New):
				walk(x.args)
			elif isinstance(x, (list, tuple)):
				for y in x:
					walk(y)

		for _call, args, _kind, _line in p.effects:
			walk(args)
		walk(v)
		return used

	def outcome(self, kind, p, v, values):
		"""the effects and the return value (or the exception) of the path, evaluated under the input values"""
		o = {'effects': [{'call': call, 'kind': kd, 'args': [self.render(a, values, p) for a in args if a not in IMPLICIT], 'line': ln,
		                  **({'task': p.task_of[k]} if k in p.task_of else {})}
		                 for k, (call, args, kd, ln) in enumerate(p.effects)]}
		if kind == 'throw':
			o['throws'] = v
		else:
			o['returns'] = None if v is None else self.render(v, values, p)
		return o

	def given(self, values):
		g = {}
		qs = {}
		for key, v in values.items():
			head = key[0]
			if head == 'target':
				g.setdefault('target', {})['kind'] = 'npc' if v == 'npc' else 'none'
			elif head == 'targetId':
				g.setdefault('target', {})['npcId'] = v
			elif head == 'dialog':
				g['dialogAction'] = {'name': self.t.action_name.get(v), 'id': v}
			elif head in ('status', 'var', 'canRepeat', 'rewardGroup', 'completeCount'):
				st = qs.setdefault(key[1], {})
				if head == 'status':
					st['status'] = None if v is None else v.name
				elif head == 'var':
					st.setdefault('vars', {})[str(key[2])] = v
				else:
					st[head] = v
			elif head == 'inv':
				g.setdefault('inventory', {})[str(key[1])] = v
			elif head in ('player', 'env', 'item'):
				g.setdefault(head, {})[key[1]] = v.name if isinstance(v, Enum) else v
			elif head == 'arg':
				g.setdefault('args', {})[key[1]] = v.name if isinstance(v, Enum) else v
		for q, st in sorted(qs.items()):
			if 'status' in st and st['status'] is None:
				st = None
			if q == self.qid:
				g['questState'] = st
			else:
				g.setdefault('otherQuests', {})[str(q)] = st
		return g

	def render(self, v, values, p):
		if isinstance(v, K):
			x = v.v
			if isinstance(x, Enum):
				return x.name
			if isinstance(x, tuple):
				return list(x)
			return x
		if isinstance(v, In):
			x = values.get(v.key)
			return x.name if isinstance(x, Enum) else x
		if isinstance(v, Ar):
			a, b = self.render(v.a, values, p), self.render(v.b, values, p)
			return self.render(self.arith(v.op, K(a), K(b)), values, p)
		if isinstance(v, Res):
			if v.k in p.assume:
				return p.assume[v.k]
			return {'resultOf': v.k}
		if isinstance(v, FromBool):
			x = self.render(v.x, values, p)
			if isinstance(x, bool):
				return 'SUCCESS' if x else 'FAILED'      # HandlerResult.fromBoolean (HandlerResult.java:11-17)
			if x is None:
				return 'UNKNOWN'
			return {'fromBoolean': x}
		if isinstance(v, New):
			return {'new': v.cls, 'args': [self.render(a, values, p) for a in v.args]}
		if isinstance(v, Opaque):
			return v.what
		if isinstance(v, RewardPage):
			g = self.render(v.x, values, p)
			if isinstance(g, dict):
				return {'rewardPageOf': g}
			return self.t.dialog.reward_page(g)          # DialogPage.getRewardPageByIndex(g).id()
		if isinstance(v, O):
			return f'${v.name if isinstance(v.name, str) else "/".join(map(str, v.name))}'
		if isinstance(v, (Cmp, BAnd, Not)):
			return self.render_bool(v, values, p)
		if isinstance(v, Lazy):
			return values.get(v.key)
		if v is None:
			return None
		raise OracleError(f'{self.rel}: cannot render {v!r}')

	def render_bool(self, v, values, p):
		if isinstance(v, Not):
			x = self.render(v.x, values, p)
			return (not x) if isinstance(x, bool) else {'not': x}
		if isinstance(v, BAnd):
			a = self.render(v.a, values, p)
			return a and self.render(v.b, values, p) if isinstance(a, bool) else {'and': [a, self.render(v.b, values, p)]}
		a, b = self.render(v.a, values, p), self.render(v.b, values, p)
		if isinstance(a, dict) or isinstance(b, dict):
			return {'compare': [a, v.op, b]}
		return self.compare_values(a, v.op, b)

	# -- text -----------------------------------------------------------------------------------------------------------------------
	def jtext(self, e):
		"""Java text of an expression, rebuilt from the tree"""
		t = self.jtext
		if isinstance(e, jast.Lit):
			return e.text
		if isinstance(e, jast.Name):
			return e.name
		if isinstance(e, jast.FieldAccess):
			return f'{t(e.target)}.{e.name}'
		if isinstance(e, jast.Call):
			head = '' if e.target is None else t(e.target) + '.'
			return f'{head}{e.name}({", ".join(t(a) for a in e.args)})'
		if isinstance(e, jast.New):
			return f'new {e.type.name}({", ".join(t(a) for a in e.args)})'
		if isinstance(e, jast.NewArray):
			return f'new {e.type.name}[] {t(e.init)}'
		if isinstance(e, jast.ArrayInit):
			return '{ ' + ', '.join(t(i) for i in e.items) + ' }'
		if isinstance(e, jast.Index):
			return f'{t(e.target)}[{t(e.index)}]'
		if isinstance(e, jast.Cast):
			return f'({e.type}) {t(e.expr)}'
		if isinstance(e, jast.Unary):
			return f'{t(e.expr)}{e.op}' if e.postfix else f'{e.op}{t(e.expr)}'
		if isinstance(e, jast.Binary):
			return f'{t(e.left)} {e.op} {t(e.right)}'
		if isinstance(e, jast.Cond):
			return f'{t(e.cond)} ? {t(e.a)} : {t(e.b)}'
		if isinstance(e, jast.Assign):
			return f'{t(e.target)} {e.op} {t(e.value)}'
		if isinstance(e, jast.InstanceOf):
			return f'{t(e.expr)} instanceof {e.type}' + (f' {e.binding}' if e.binding else '')
		if isinstance(e, jast.Paren):
			return f'({t(e.expr)})'
		return type(e).__name__


# --- files -------------------------------------------------------------------------------------------------------------------------

def trace_file(tables, path, rel=None):
	"""the trace document of one Java quest handler"""
	path = Path(path)
	rel = rel or path.name
	try:
		ex = Extractor(tables, path, rel)
		reg, hooks, cases = ex.trace()
	except (jast.Unsupported, javasrc.JavaSyntaxError) as e:
		raise OracleError(f'{rel}: {e}') from e
	return {'format': FORMAT, 'version': VERSION, 'questId': ex.qid, 'java': rel, 'class': ex.td.name,
	        'javaSha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'register': reg, 'hooks': hooks, 'cases': cases}


WIDTH = 140


def _encode(v, indent):
	"""JSON with every object or array that fits on one line (at its indentation) written on one line, the others one member per line"""
	flat = json.dumps(v, ensure_ascii=True, separators=(', ', ': '))
	if not isinstance(v, (dict, list)) or not v or len(flat) + indent <= WIDTH:
		return flat
	pad = ' ' * (indent + 1)
	if isinstance(v, dict):
		items = [f'{pad}{json.dumps(k, ensure_ascii=True)}: {_encode(x, indent + 1)}' for k, x in v.items()]
		return '{\n' + ',\n'.join(items) + '\n' + ' ' * indent + '}'
	items = [f'{pad}{_encode(x, indent + 1)}' for x in v]
	return '[\n' + ',\n'.join(items) + '\n' + ' ' * indent + ']'


def dumps(doc):
	return _encode(doc, 0) + '\n'


def generate(out_dir=EXPECTED_DIR, rels=SLICE, quest_dir=QUEST_DIR, java_src=JAVA_SRC):
	"""write <out_dir>/<questId>.json for every file; returns {questId: doc}"""
	tables = Tables(java_src)
	out_dir = Path(out_dir)
	out_dir.mkdir(parents=True, exist_ok=True)
	docs = {}
	for rel in rels:
		doc = trace_file(tables, Path(quest_dir) / rel, rel)
		docs[doc['questId']] = doc
		(out_dir / f"{doc['questId']}.json").write_bytes(dumps(doc).encode('utf-8'))
	return docs


def check(expected_dir=EXPECTED_DIR, rels=SLICE, quest_dir=QUEST_DIR, java_src=JAVA_SRC, extra=True):
	"""[problem] between the committed traces and a regeneration from the current Java tree; with `extra`, a document in expected_dir
	that none of `rels` produces is a problem too (off for a check of some handlers only: oracle.py quest-trace check --only)"""
	tables = Tables(java_src)
	problems = []
	seen = set()
	for rel in rels:
		doc = trace_file(tables, Path(quest_dir) / rel, rel)
		name = f"{doc['questId']}.json"
		seen.add(name)
		f = Path(expected_dir) / name
		if not f.is_file():
			problems.append(f'{name}: missing (python oracle.py quest-trace generate)')
		elif f.read_bytes().decode('utf-8') != dumps(doc):
			problems.append(f'{name}: stale against {rel}')
	for f in sorted(Path(expected_dir).glob('*.json')) if extra else ():
		if f.name not in seen:
			problems.append(f'{f.name}: not in the slice')
	return problems
