"""The 16 XMLQuests template kinds: what each handler registers at QuestEngine.init and the steps it performs, as QuestState changes.

Every kind is transcribed from its handler in questEngine/handlers/template (the data class under questEngine/handlers/models decides the
constructor arguments) and the AbstractQuestHandler helpers it calls:
- sendQuestDialog (AbstractQuestHandler.java:330-344): SM_DIALOG_WINDOW(npc, page, env questId); a reward window page (5-8, 45-50) is sent
  only in REWARD state ("reward packet exploitation fix"), else the call answers false and nothing is sent;
- sendQuestSelectionDialog (:354-357): page 10 with quest 0; closeDialogWindow (:359-362): page 0 with quest 0;
- sendQuestStartDialog (:373-398): QUEST_ACCEPT/QUEST_ACCEPT_1/QUEST_ACCEPT_SIMPLE -> QuestService.startQuest, the work item, page 1003 (close
  for QUEST_ACCEPT_SIMPLE); ASK_QUEST_ACCEPT page 4; QUEST_REFUSE_1/2 page 1004; QUEST_REFUSE_SIMPLE close; FINISH_DIALOG page 10;
- onDialogEvent of the base (:93-117): ASK_QUEST_ACCEPT page 4, QUEST_ACCEPT_1 page 1003 (no start), the refusals 1004-1007, FINISH_DIALOG;
- sendQuestEndDialog (:413-474): only in REWARD; SELECTED_QUEST_REWARD1..15 / SELECTED_QUEST_NOREWARD (8-23) -> QuestService.finishQuest and
  the follow-up; SET_SUCCEED closes; USE_OBJECT, QUEST_SELECT, SELECT_QUEST_REWARD and both CHECK_USER_HAS_QUEST_ITEM actions show the reward
  page DialogPage.getRewardPageByIndex(rewardGroup) after QuestService.validateAndFixRewardGroup;
- updateQuestStatus (:290-296): SM_QUEST_ACTION(UPDATE), plus updateNearbyQuests in REWARD or COMPLETE; changeQuestStep (:307-328);
  defaultCloseDialog (:510-529); checkQuestItems / checkQuestItemsSimple (:536-574) over QuestService.collectItemCheck (QuestService.java:557-600).
A handler that answers false makes DialogService.handleQuestDialogueOrSendNextPage (DialogService.java:282-291) send the "next page",
SM_DIALOG_WINDOW(npc, dialogActionId, questId); a step records that as `handled: false`.

What is not modelled: xml_quest's operation language (1127 is the only one) and mentor_monster_hunt's group conditions (no quest uses it) -
their steps are null with a reason; report_to_many with mission="true" (defaultOnLevelChangedEvent); the dynamic start triggers (an invasion
world's open rift or vortex, a zone, a distance) are reported, not evaluated. Refused (OracleError) like Java fails: a kill_spawned or
xml_quest on_kill_event <monster> without npc_ids (register() throws at startup), a kill_spawned quest whose kills never reach REWARD, a
crafting_rewards level_reward other than 400 or 500 (canLearn throws in every dialog).
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field

from staticdata_oracle import OracleError

from .javasrc import JObj, java_integer, read_source, strip_comments

KINAH = 182400001  # ItemId.KINAH: collectItemCheck compares it against Inventory.getKinah()
CRAFTING_LEVEL_REWARDS = (400, 500)  # CraftingRewards.canLearn: expert 400, master 500, anything else throws IllegalStateException
QUEST_OBJECT_NPC_PREFIX = 7  # AbstractQuestHandler.loadActionItems: drop.getNpcId() / 100000 == 7 (AbstractQuestHandler.java:71-79)
REWARD_WINDOW_PAGES = ("SELECT_QUEST_REWARD_WINDOW1", "SELECT_QUEST_REWARD_WINDOW2", "SELECT_QUEST_REWARD_WINDOW3", "SELECT_QUEST_REWARD_WINDOW4",
                       "SELECT_QUEST_REWARD_WINDOW5", "SELECT_QUEST_REWARD_WINDOW6", "SELECT_QUEST_REWARD_WINDOW7", "SELECT_QUEST_REWARD_WINDOW8",
                       "SELECT_QUEST_REWARD_WINDOW9", "SELECT_QUEST_REWARD_WINDOW10")


def template_classes(jaxb) -> dict[str, str]:
	"""tag -> the template handler each data class's register() constructs (`questEngine.addQuestHandler(new X(...))`)."""
	result = {}
	for tag, data_class in jaxb.quest_kinds.items():
		text = strip_comments(read_source(jaxb.find(data_class)))
		match = re.search(r"addQuestHandler\s*\(\s*new\s+(\w+)\s*\(", text)
		if not match:
			raise OracleError(f"{data_class}.java: register() does not construct a template handler")
		result[tag] = match.group(1)
	return result


def _dedup(values) -> list[int]:
	out = []
	for v in values or []:
		if v not in out:
			out.append(v)
	return out


@dataclass
class Registration:
	"""What a handler's register() adds to QuestEngine (QuestEngine.java:707-790): npc lists are the npcs whose QuestNpc gets the quest."""

	quest_start: list[int] = field(default_factory=list)
	talk: list[int] = field(default_factory=list)
	kill: list[int] = field(default_factory=list)
	aggro: list[int] = field(default_factory=list)
	at_distance: list[list[int]] = field(default_factory=list)
	enter_world: bool = False
	level_changed: bool = False
	enter_zone: str | None = None
	quest_items: list[int] = field(default_factory=list)
	skills: list[int] = field(default_factory=list)
	kill_in_world: list[int] | str | None = None
	kill_in_zone: list[str] | str | None = None
	can_act: list[int] = field(default_factory=list)

	def add(self, name: str, values) -> None:
		target = getattr(self, name)
		for v in values:
			if v not in target:
				target.append(v)

	def as_json(self) -> dict:
		return {"onQuestStart": self.quest_start, "onTalkEvent": self.talk, "onKillEvent": self.kill, "onAddAggroListEvent": self.aggro,
		        "onAtDistanceEvent": self.at_distance, "onEnterWorld": self.enter_world, "onLevelChanged": self.level_changed,
		        "onEnterZone": self.enter_zone, "questItems": self.quest_items, "questSkills": self.skills, "onKillInWorld": self.kill_in_world,
		        "onKillInZone": self.kill_in_zone, "canAct": self.can_act}


@dataclass
class Monster:
	"""questEngine/handlers/models/Monster as MonsterHuntData.register builds it from a <quest_kill> (MonsterHuntData.java:60-75), or a
	<monster> of kill_spawned as JAXB reads it."""

	npc_ids: list[int]
	var: int
	end_var: int
	step: int = 0
	spawner: int = 0

	def as_json(self) -> dict:
		result = {"npcIds": self.npc_ids, "var": self.var, "count": self.end_var, "step": self.step}
		if self.spawner:
			result["spawnerObjectId"] = self.spawner
		return result


def monsters_from_quest_kill(template: JObj) -> list[Monster]:
	"""MonsterHuntData.register: endVar = count if > 0, var = var if > 0, then seq if > 0; step if > 0; npc ids de-duplicated."""
	monsters = []
	for qk in template["questKill"] or []:
		var = qk["var"] if qk["var"] > 0 else 0
		if qk["seq"] > 0:
			var = qk["seq"]
		monsters.append(Monster(_dedup(qk["npcIds"]), var, qk["kill"] if qk["kill"] > 0 else 0, qk["step"] if qk["step"] > 0 else 0))
	return monsters


def _int32(value: int) -> int:
	return (value + 2**31) % 2**32 - 2**31


class SimState:
	"""A QuestState as the handlers change it: status, the six 6-bit vars (QuestVars.java), and the packets updateQuestStatus sends."""

	def __init__(self, status: str | None = "START", variables: list[int] | None = None):
		self.status = status
		self.vars = list(variables) if variables else [0] * 6
		self.updates = 0
		self.nearby = 0

	def get(self, index: int) -> int:
		if not 0 <= index < 6:
			raise OracleError(f"quest var index {index}: Java throws ArrayIndexOutOfBoundsException (QuestVars has 6 vars)")
		return self.vars[index]

	def set_by_id(self, index: int, value: int) -> None:
		self.get(index)
		self.vars[index] = value

	def set_var(self, value: int) -> None:
		"""QuestVars.setVar: six 6-bit groups, lowest first."""
		value = _int32(value)
		for i in range(6):
			self.vars[i] = value & 0x3F
			value >>= 6

	def update(self) -> None:
		"""AbstractQuestHandler.updateQuestStatus."""
		self.updates += 1
		if self.status in ("REWARD", "COMPLETE"):
			self.nearby += 1

	def packed_total(self, first_var: int, end_var: int) -> tuple[int, int]:
		"""The do-while of MonsterHunt/SkillUse: the count spread over as many 6-bit vars as end_var needs; (total, first var after)."""
		total = 0
		var_id = first_var
		end = end_var
		while True:
			total = _int32(total + (self.get(var_id) << ((var_id - first_var) * 6)))
			end >>= 6
			var_id += 1
			if end <= 0:
				break
		return total, var_id


def monster_hunt_kill(state: SimState, monsters: list[Monster], target: int, data_driven: bool, aggro: bool, reward: bool,
                      reward_next_step: bool) -> bool:
	"""MonsterHunt.onKillEvent (MonsterHunt.java:174-239) on `state` for a kill of npc `target`; the return value is Java's."""
	if state.status != "START":
		return False
	current_total = 0
	total_end = 0
	cur_step = state.get(0)
	last_step = 0
	for m in monsters:
		last_step = max(last_step, m.step)
		if data_driven and m.step != cur_step:
			continue
		if target in m.npc_ids:
			total, var_id = state.packed_total(m.var, m.end_var)
			total += 1
			if total <= m.end_var:
				if aggro:
					state.status = "REWARD"
					state.update()
					return True
				tmp = total
				for used in range(m.var, var_id):
					state.set_by_id(used, total & 0x3F)
					total >>= 6
				state.update()
				if not data_driven:
					if tmp == m.end_var and (reward or reward_next_step):
						if reward_next_step:
							state.set_by_id(0, state.get(0) + 1)
						state.status = "REWARD"
						state.update()
					return True
		total_end += m.end_var
		current_total += state.get(m.var)
	if current_total >= total_end and data_driven:
		state.set_var(cur_step + 1)
		if cur_step >= last_step:
			state.status = "REWARD"
		state.update()
		return True
	return False


def monster_hunt_report_ok(state: SimState, monsters: list[Monster]) -> bool:
	"""MonsterHunt.onDialogEvent, SELECT_QUEST_REWARD in START (MonsterHunt.java:130-146): every monster's packed total reaches its end var."""
	for m in monsters:
		total, _ = state.packed_total(m.var, m.end_var)
		if m.end_var > total:
			return False
	return True


def kill_spawned_kill(state: SimState, monsters: list[Monster], target: int) -> bool:
	"""KillSpawned.onKillEvent (KillSpawned.java:115-137)."""
	if state.status != "START":
		return False
	for m in monsters:
		if target in m.npc_ids:
			if state.get(m.var) < m.end_var:
				state.set_by_id(m.var, state.get(m.var) + 1)
				for n in monsters:
					if state.get(n.var) < n.end_var:
						state.update()
						return True
				state.status = "REWARD"
				state.update()
				return True
	return False


def skill_use_event(state: SimState, skills: list[JObj], skill_id: int) -> bool:
	"""SkillUse.onUseSkillEvent (SkillUse.java:86-126)."""
	if state.status != "START":
		return False
	reward_count = 0
	success = False
	for qd in skills:
		if skill_id in (qd["skillIds"] or []):
			total, var_id = state.packed_total(qd["varNum"], qd["endVar"])
			total += 1
			if total <= qd["endVar"]:
				for used in range(qd["varNum"], var_id):
					state.set_by_id(used, total & 0x3F)
					total >>= 6
				if state.get(qd["varNum"]) == qd["endVar"]:
					reward_count += 1
				state.update()
				success = True
	if reward_count == len(skills):
		if state.get(0) == 0:
			state.set_by_id(0, 1)
		state.status = "REWARD"
		state.update()
	return success


def ranked_kill(state: SimState, start_var: int, end_var: int, reward: bool, data_driven: bool) -> bool:
	"""AbstractQuestHandler.defaultOnKillRankedEvent (AbstractQuestHandler.java:753-783), which KillInWorld and KillInZone call with
	startVar 0, endVar = amount, reward true."""
	if state.status != "START":
		return False
	var = state.get(0)
	if data_driven:
		var_kill = state.get(1)
		if start_var <= var_kill < end_var - 1:
			state.set_by_id(1, var_kill + 1)
			state.update()
			return True
		if var_kill == end_var - 1:
			if reward:
				state.status = "REWARD"
			state.set_var(var + 1)
	else:
		if start_var <= var < end_var - 1:
			state.set_by_id(0, var + 1)
			state.update()
			return True
		if var == end_var - 1:
			if reward:
				state.status = "REWARD"
			else:
				state.set_by_id(0, var + 1)
	state.update()
	return True


def _snapshot(state: SimState, number: int, target: int, changed: bool, updates_before: int, nearby_before: int, **extra) -> dict:
	row = {"n": number, "target": target, "handled": changed, "vars": list(state.vars), "status": state.status,
	       "questActionUpdates": state.updates - updates_before, "nearbyUpdates": state.nearby - nearby_before}
	row.update(extra)
	return row


def simulate_monster_hunt(monsters: list[Monster], data_driven: bool, aggro: bool, reward: bool, reward_next_step: bool) -> list[dict]:
	"""A kill-by-kill run from a fresh START: each monster's first npc id until its count is credited, plus one kill more (which shows what an
	extra kill does), then the next monster; stops when the quest leaves START. A data-driven hunt counts only the monsters of the current
	step (var0), so its monsters are killed step by step (0, 1, ...), in list order within a step."""
	state = SimState()
	rows = []
	number = 0
	order = list(enumerate(monsters))
	if data_driven:
		order.sort(key=lambda item: item[1].step)  # stable: list order within a step
	for index, m in order:
		if not m.npc_ids:
			continue
		extra_done = False
		guard = 0
		while state.status == "START" and not extra_done:
			guard += 1
			if guard > 4096:
				raise OracleError("monster_hunt simulation does not end")
			number += 1
			before = (state.updates, state.nearby)
			total_before, _ = state.packed_total(m.var, m.end_var)
			handled = monster_hunt_kill(state, monsters, m.npc_ids[0], data_driven, aggro, reward, reward_next_step)
			rows.append(_snapshot(state, number, m.npc_ids[0], handled, *before, monster=index))
			total_after, _ = state.packed_total(m.var, m.end_var) if state.status == "START" else (total_before + 1, 0)
			if total_before >= m.end_var or total_after == total_before:
				extra_done = True
		if state.status != "START":
			break
	return rows


class Flow:
	"""The happy path of one handler, step by step, on a simulated QuestState."""

	def __init__(self, ctx, quest_id: int, template: JObj):
		self.ctx = ctx
		self.dialogs = ctx.dialogs
		self.qid = quest_id
		self.template = template
		self.state = SimState(status=None)
		self.reward_group: int | None = None
		self.steps: list[dict] = []
		self.not_modelled: list[str] = []

	@property
	def data_driven(self) -> bool:
		return bool(self.template["dataDriven"])

	def action(self, *names: str) -> dict | list[dict]:
		actions = [self.dialogs.action(n) for n in names]
		return actions[0] if len(actions) == 1 else actions

	def window(self, page: int, quest: bool = True) -> dict:
		return {"page": page, "questId": self.qid if quest else 0}

	def quest_dialog(self, page: int) -> dict | None:
		"""sendQuestDialog: a reward window outside REWARD is not sent (AbstractQuestHandler.java:331-339)."""
		reward_windows = {self.dialogs.pages[p] for p in REWARD_WINDOW_PAGES}
		if page in reward_windows and self.state.status != "REWARD":
			return None
		return self.window(page)

	def selection(self) -> dict:
		return self.window(10, quest=False)

	def close(self) -> dict:
		return self.window(0, quest=False)

	def effect(self, **extra) -> dict:
		result = {"status": self.state.status, "statusValue": self.dialogs.quest_status.get(self.state.status) if self.state.status else None,
		          "vars": list(self.state.vars)}
		result.update(extra)
		return result

	def add(self, phase: str, at: dict, status: str, action, java: str, window: dict | None = None, effect: dict | None = None,
	        handled: bool = True, guard: str | None = None, other: str | None = None, note: str | None = None) -> None:
		step = {"phase": phase, "at": at, "status": status, "action": action, "handled": handled, "window": window}
		if effect is not None:
			step["effect"] = effect
		if guard:
			step["guard"] = guard
		if other:
			step["otherActions"] = other
		if note:
			step["note"] = note
		step["java"] = java
		self.steps.append(step)

	# --- the shared pieces ----------------------------------------------------------------------------------------------------------------

	def start_quest(self, give: list[dict] | None = None, status: str = "START") -> dict:
		"""QuestService.startQuest (QuestService.java:400-444): a new QuestState (vars 0), SM_QUEST_ACTION(ADD), updateNearbyQuests."""
		self.state = SimState(status=status)
		return self.effect(startQuest=True, questAction="ADD", nearbyUpdate=True, giveItems=give or [])

	def accept(self, at: dict, work_item: dict | None, java: str) -> None:
		"""sendQuestStartDialog(env, workItem) for QUEST_ACCEPT / QUEST_ACCEPT_1 / QUEST_ACCEPT_SIMPLE (AbstractQuestHandler.java:373-398)."""
		effect = self.start_quest(given_items([work_item] if work_item else []))
		self.add("start", at, "startable", self.action("QUEST_ACCEPT_1", "QUEST_ACCEPT", "QUEST_ACCEPT_SIMPLE"), java, self.window(1003), effect,
		         note="QUEST_ACCEPT_SIMPLE closes the window (page 0, quest 0) instead of page 1003; startQuest refuses when checkStartConditions "
		              "fails or the journal holds CustomConfig.BASIC_QUEST_SIZE_LIMIT normal quests; giveQuestItem adds the count minus the count "
		              "already held",
		         other="sendQuestStartDialog")

	def reward_steps(self, npcs: list[int], java: str, reward_group_fixed: bool = True) -> None:
		"""A handler that answers every REWARD-state dialog at its end npc with sendQuestEndDialog."""
		if reward_group_fixed:
			self.fix_reward_group()
		page = self.dialogs.reward_page(self.reward_group)
		self.add("reward", {"npcs": npcs}, "REWARD", self.action("USE_OBJECT", "QUEST_SELECT", "SELECT_QUEST_REWARD"), java, self.window(page),
		         note="sendQuestEndDialog: the reward page of DialogPage.getRewardPageByIndex(rewardGroup)")
		self.finish(npcs, java)

	def fix_reward_group(self) -> None:
		"""QuestService.validateAndFixRewardGroup (QuestService.java:123-142) in REWARD: group 0 when the template has reward groups and none
		was set (a warning when there are several), the last group when the set index is out of range, null when there are none."""
		groups = self.template["rewards"] or []
		if self.reward_group is None:
			self.reward_group = 0 if groups else None
		elif not 0 <= self.reward_group < len(groups):
			self.reward_group = len(groups) - 1 if groups else None

	def finish(self, npcs: list[int], java: str, action=None, note: str | None = None, window: dict | None = None,
	           follow_up: bool = True) -> None:
		"""QuestService.finishQuest (QuestService.java:77-118): rewards, COMPLETE, vars 0, SM_QUEST_ACTION(UPDATE), updateNearbyQuests.
		Through sendQuestEndDialog the window that follows is the follow-up (AbstractQuestHandler.java:422-457, `followUp` of the report)."""
		self.state.status = "COMPLETE"
		self.state.set_var(0)
		effect = self.effect(finishQuest=True, questAction="UPDATE", nearbyUpdate=True, completeCount=1, rewardGroup=self.reward_group,
		                     followUp=follow_up)
		actions = action or [self.dialogs.action("SELECTED_QUEST_NOREWARD")] + [self.dialogs.action(f"SELECTED_QUEST_REWARD{i}") for i in range(1, 16)]
		self.add("reward", {"npcs": npcs}, "REWARD", actions, java, window, effect,
		         note=note or "sendQuestEndDialog -> finishQuest; SELECTED_QUEST_REWARDn also pays selectable item n - 1")

	def report_to_reward(self, npcs: list[int], java: str, var: int = 1) -> None:
		"""setQuestVar + REWARD + updateQuestStatus, then sendQuestEndDialog's SELECT_QUEST_REWARD arm."""
		self.state.set_var(var)
		self.state.status = "REWARD"
		self.fix_reward_group()
		return self.effect(questAction="UPDATE", nearbyUpdate=True, rewardGroup=self.reward_group)

	def start_at(self, npcs: list[int], any_talk_npc: bool, talk_npcs: list[int]) -> dict:
		if any_talk_npc:
			return {"npcs": talk_npcs, "anyTalkNpc": True}
		return {"npcs": npcs}


def _work_item(flow: Flow, index: int = 0) -> dict | None:
	"""AbstractQuestHandler.workItems (loadWorkItems, :66-69): the template's <quest_work_items>; the single-item templates take the first."""
	items = flow.ctx.work_items(flow.template)
	if items is None:
		return None
	if not items:
		raise OracleError(f"quest {flow.qid}: an empty <quest_work_items> makes the template constructor throw (workItems.get(0))")
	return items[index] if index < len(items) else None


def given_items(items: list[dict]) -> list[dict]:
	"""The items giveQuestItem really adds: it does nothing for item id 0 or count 0 (AbstractQuestHandler.java:626-641, the guard of
	sendQuestStartDialog at :383 is the same)."""
	return [i for i in items if i["itemId"] != 0 and i["count"] != 0]


def start_page(flow: Flow, start_dialog_id: int) -> int:
	return start_dialog_id if start_dialog_id != 0 else (4762 if flow.data_driven else 1011)


def _npcs(value) -> list[int]:
	return _dedup(value or [])


def _end_npcs(q: JObj) -> list[int]:
	"""The handlers' `if (endNpcIds != null) add them, else add the start npcs`."""
	return _npcs(q["endNpcIds"]) if q["endNpcIds"] is not None else _npcs(q["startNpcIds"])


# --- registration (QuestEngine.init -> XMLQuest.register -> the handler's constructor and register()) --------------------------------------


def _check_single_work_item(ctx, quest_id: int, template: JObj) -> None:
	"""ReportTo/MonsterHunt/ItemCollecting/ItemOrders constructors: `workItem = workItems.get(0)` when <quest_work_items> exists."""
	items = ctx.work_items(template)
	if items is not None and not items:
		raise OracleError(f"quest {quest_id}: an empty <quest_work_items> makes the template constructor throw IndexOutOfBoundsException "
		                  "(workItems.get(0)) at startup")


def register_report_to(ctx, quest_id, q, t, reg):
	"""ReportTo.register (ReportTo.java:50-60)."""
	_check_single_work_item(ctx, quest_id, t)
	start = _npcs(q["startNpcIds"])
	reg.add("quest_start", start)
	reg.add("talk", start)
	reg.add("talk", _end_npcs(q))


def register_monster_hunt(ctx, quest_id, q, t, reg, mentor=False):
	"""MonsterHunt.register (MonsterHunt.java:78-103); MentorMonsterHunt passes no aggro npcs, invasion world, zone or distance npc."""
	_check_single_work_item(ctx, quest_id, t)
	start = _npcs(q["startNpcIds"])
	reg.add("quest_start", start)
	reg.add("talk", start)
	for m in monsters_from_quest_kill(t):
		reg.add("kill", m.npc_ids)
	reg.add("talk", _end_npcs(q))
	if mentor:
		return
	reg.add("aggro", _npcs(q["aggroNpcIds"]))
	reg.enter_world = q["invasionWorld"] != 0
	if q["startZone"] is not None and q["startZone"].upper() != "NONE":
		reg.enter_zone = q["startZone"]
	if q["startDistanceNpcId"]:
		reg.at_distance.append([q["startDistanceNpcId"], 300])


def register_item_collecting(ctx, quest_id, q, t, reg):
	"""ItemCollecting.register (ItemCollecting.java:66-86); the action items are the 7xxxxx quest_drop npcs (loadActionItems)."""
	_check_single_work_item(ctx, quest_id, t)
	start = _npcs(q["startNpcIds"])
	reg.add("quest_start", start)
	reg.add("talk", start)
	if q["nextNpcId"]:
		reg.add("talk", [q["nextNpcId"]])
	reg.add("talk", _end_npcs(q))
	actions = ctx.action_items(t)
	reg.add("talk", actions)
	reg.add("can_act", actions)
	if q["startZone"] is not None and q["startZone"].upper() != "NONE":
		reg.enter_zone = q["startZone"]


def register_report_to_many(ctx, quest_id, q, t, reg):
	"""ReportToMany.register (ReportToMany.java:50-67): a start item replaces the start npcs."""
	if q["mission"]:
		reg.level_changed = True
	if q["startItemId"]:
		reg.add("quest_items", [q["startItemId"]])
	else:
		start = _npcs(q["startNpcIds"])
		reg.add("quest_start", start)
		reg.add("talk", start)
	for info in q["npcInfos"]:
		reg.add("talk", _npcs(info["npcIds"]))


def register_kill_spawned(ctx, quest_id, q, t, reg):
	"""KillSpawned.register (KillSpawned.java:44-59)."""
	start = _npcs(q["startNpcIds"])
	reg.add("quest_start", start)
	reg.add("talk", start)
	reg.add("talk", _end_npcs(q))
	for m in q["monster"] or []:
		if m["npcIds"] is None:
			raise OracleError(f"kill_spawned {quest_id}: a <monster> without npc_ids makes KillSpawned.register throw NullPointerException "
			                  "(`for (Integer id : spawnedMonster.getNpcIds())`, KillSpawned.java:52-55) at startup")
		reg.add("kill", _npcs(m["npcIds"]))
	reg.add("talk", _dedup([m["spawnerObjectId"] for m in q["monster"] or []]))


def register_skill_use(ctx, quest_id, q, t, reg):
	"""SkillUse.register (SkillUse.java:37-50)."""
	start = _npcs(q["startNpcIds"])
	reg.add("quest_start", start)
	reg.add("talk", start)
	reg.add("talk", _end_npcs(q))
	for s in q["skills"] or []:
		reg.add("skills", s["skillIds"] or [])


def register_report_on_levelup(ctx, quest_id, q, t, reg):
	"""ReportOnLevelUp.register (ReportOnLevelUp.java:28-34)."""
	reg.add("talk", _npcs(q["endNpcIds"]))
	reg.enter_world = True
	reg.level_changed = True


def register_kill_in_world(ctx, quest_id, q, t, reg):
	"""KillInWorld.register (KillInWorld.java:76-93); no worlds = every world map."""
	start = _npcs(q["startNpcIds"])
	reg.add("quest_start", start)
	reg.add("talk", start)
	reg.add("talk", _end_npcs(q))
	reg.kill_in_world = _dedup(q["worldIds"]) if q["worldIds"] is not None else "every world map (WorldMapsData)"
	reg.enter_world = q["invasionWorld"] != 0
	if q["startDistanceNpcId"]:
		reg.at_distance.append([q["startDistanceNpcId"], 300])


def register_kill_in_zone(ctx, quest_id, q, t, reg):
	"""KillInZone.register (KillInZone.java:64-77); no zones = every zone."""
	if q["endNpcIds"] is None and q["startNpcIds"] is None:
		raise OracleError(f"kill_in_zone {quest_id}: no npc lists (the KillInZone constructor throws NullPointerException at startup)")
	start = _npcs(q["startNpcIds"])
	reg.add("quest_start", start)
	reg.add("talk", start)
	reg.add("talk", _end_npcs(q))
	reg.kill_in_zone = _dedup(q["zones"]) if q["zones"] is not None else "every zone (ZoneData)"
	if q["startDistanceNpc"]:
		reg.at_distance.append([q["startDistanceNpc"], 300])


def register_crafting_rewards(ctx, quest_id, q, t, reg):
	"""CraftingRewards.register (CraftingRewards.java:35-43)."""
	start = q["startNpcId"]
	end = q["endNpcId"] or start
	if start:
		reg.add("quest_start", [start])
		reg.add("talk", [start])
	if end != start:
		reg.add("talk", [end])


def register_start_npcs_only(ctx, quest_id, q, t, reg):
	"""RelicRewards.register (RelicRewards.java:33-38), FountainRewards.register (FountainRewards.java:31-36), WorkOrders.register
	(WorkOrders.java:40-45)."""
	start = _npcs(q["startNpcIds"])
	reg.add("quest_start", start)
	reg.add("talk", start)


def register_item_order(ctx, quest_id, q, t, reg):
	"""ItemOrders.register (ItemOrders.java:46-54): the start item is the first work item (0 without work items)."""
	_check_single_work_item(ctx, quest_id, t)
	items = ctx.work_items(t)
	reg.add("quest_items", [items[0]["itemId"] if items else 0])
	for npc in (q["talkNpcId1"], q["talkNpcId2"], q["endNpcId"]):
		if npc:
			reg.add("talk", [npc])


def register_xml_quest(ctx, quest_id, q, t, reg):
	"""XmlQuest.register (XmlQuest.java:46-68): the start and end npcs, every on_talk_event's ids, every on_kill_event monster."""
	start = _npcs(q["startNpcIds"])
	reg.add("quest_start", start)
	reg.add("talk", start)
	reg.add("talk", _end_npcs(q))
	for event in q["onTalkEvents"] or []:  # QuestEvent.getIds() makes a missing ids an empty list
		reg.add("talk", [java_integer(v, 32, f"xml_quest {quest_id} on_talk_event ids") for v in event.get("attributes", {}).get("ids", "").split()])
	for event in q["onKillEvents"] or []:
		for child in event.get("children", []):
			if child["tag"] == "monster":
				npc_ids = child.get("attributes", {}).get("npc_ids")
				if npc_ids is None:
					raise OracleError(f"xml_quest {quest_id}: an on_kill_event <monster> without npc_ids makes XmlQuest.register throw "
					                  "NullPointerException (`for (Integer id : monster.getNpcIds())`, XmlQuest.java:62-66) at startup")
				reg.add("kill", [java_integer(v, 32, f"xml_quest {quest_id} monster npc_ids") for v in npc_ids.split()])


REGISTER = {
	"report_to": register_report_to,
	"monster_hunt": register_monster_hunt,
	"mentor_monster_hunt": lambda ctx, quest_id, q, t, reg: register_monster_hunt(ctx, quest_id, q, t, reg, mentor=True),
	"item_collecting": register_item_collecting,
	"report_to_many": register_report_to_many,
	"kill_spawned": register_kill_spawned,
	"skill_use": register_skill_use,
	"report_on_levelup": register_report_on_levelup,
	"kill_in_world": register_kill_in_world,
	"kill_in_zone": register_kill_in_zone,
	"crafting_rewards": register_crafting_rewards,
	"relic_rewards": register_start_npcs_only,
	"fountain_rewards": register_start_npcs_only,
	"work_order": register_start_npcs_only,
	"item_order": register_item_order,
	"xml_quest": register_xml_quest,
}


def registration(ctx, kind: str, quest_id: int, q: JObj, template: JObj) -> Registration:
	reg = Registration()
	REGISTER[kind](ctx, quest_id, q, template, reg)
	return reg


# --- the steps of each kind --------------------------------------------------------------------------------------------------------------


def kind_report_to(flow: Flow, q: JObj, reg: Registration) -> dict:
	"""ReportTo (ReportTo.java:32-107)."""
	start, end = _npcs(q["startNpcIds"]), _end_npcs(q)
	work = _work_item(flow)
	at = flow.start_at(start, not start, reg.talk)
	flow.add("start", at, "startable", flow.action("QUEST_SELECT"), "ReportTo.java:72-73", flow.window(start_page(flow, q["startDialogId"])),
	         other="AbstractQuestHandler.onDialogEvent")
	flow.accept(at, work, "ReportTo.java:74-77")
	flow.add("report", {"npcs": end}, "START", flow.action("QUEST_SELECT"), "ReportTo.java:85-86", flow.window(10002 if flow.data_driven else 2375))
	effect = flow.report_to_reward(end, "")
	if work:
		effect["removeItems"] = [dict(work, count="all")]
	flow.add("report", {"npcs": end}, "START", flow.action("SELECT_QUEST_REWARD"), "ReportTo.java:87-98",
	         flow.window(flow.dialogs.reward_page(flow.reward_group)), effect,
	         guard=f"the work item {work['itemId']} x{work['count']} is in the inventory, else page 10 (quest 0) and no change" if work else None)
	flow.reward_steps(end, "ReportTo.java:101-104, AbstractQuestHandler.java:413-474")
	return {"talk": {"start": start, "end": end}}


def kind_monster_hunt(flow: Flow, q: JObj, reg: Registration, mentor: bool = False) -> dict:
	"""MonsterHunt (MonsterHunt.java:32-289); MentorMonsterHunt passes 0/null for the dialog, aggro, invasion, zone and distance arguments."""
	t = flow.template
	start, end = _npcs(q["startNpcIds"]), _end_npcs(q)
	monsters = monsters_from_quest_kill(t)
	aggro = [] if mentor else _npcs(q["aggroNpcIds"])
	invasion = 0 if mentor else q["invasionWorld"]
	start_zone = None if mentor else q["startZone"]
	distance_npc = 0 if mentor else q["startDistanceNpcId"]
	start_dialog = 0 if mentor else q["startDialogId"]
	end_dialog = 0 if mentor else q["endDialogId"]
	work = _work_item(flow)
	faction = t["category"] == "FACTION"
	at = flow.start_at(start, not start or faction, reg.talk)
	flow.add("start", at, "startable", flow.action("QUEST_SELECT"), "MonsterHunt.java:113-117", flow.window(start_page(flow, start_dialog)),
	         other="AbstractQuestHandler.onDialogEvent",
	         note="category FACTION: any npc whose talk events name the quest" if faction else None)
	flow.accept(at, work, "MonsterHunt.java:118-121")
	dd = flow.data_driven
	kills = simulate_monster_hunt(monsters, dd, bool(aggro), q["reward"], q["rewardNextStep"])
	if mentor:
		flow.not_modelled.append("progress: MentorMonsterHunt.onKillEvent (MentorMonsterHunt.java:31-58) counts a kill only in a group with a "
		                         "mentor/mentee in range; the kill column is MonsterHunt.onKillEvent's for such a group")
	last = kills[-1] if kills else None
	flow.state = SimState("START")
	for row in kills:
		flow.state.vars = list(row["vars"])
		flow.state.status = row["status"]
	if flow.state.status == "START":
		flow.add("report", {"npcs": end}, "START", flow.action("QUEST_SELECT"), "MonsterHunt.java:128-129", flow.window(end_dialog or 1352))
		ok = monster_hunt_report_ok(flow.state, monsters)
		if not ok:
			raise OracleError(f"quest {flow.qid}: the kill run did not satisfy MonsterHunt's report check")
		flow.state.status = "REWARD"
		effect = flow.effect(questAction="UPDATE", nearbyUpdate=True)
		flow.add("report", {"npcs": end}, "START", flow.action("SELECT_QUEST_REWARD"), "MonsterHunt.java:130-149", flow.quest_dialog(5), effect,
		         guard="every monster's kill total (6-bit vars from its var) reaches its count, else the handler answers false (DialogService "
		               "sends page 1009) and nothing changes")
	flow.fix_reward_group()
	if aggro or dd:
		flow.add("reward", {"npcs": end}, "REWARD", flow.action("QUEST_SELECT", "USE_OBJECT"), "MonsterHunt.java:154-158", flow.window(10002))
		note = "SELECT_QUEST_REWARD first removes the work item" if work else None
		flow.add("reward", {"npcs": end}, "REWARD", flow.action("SELECT_QUEST_REWARD"), "MonsterHunt.java:159-167",
		         flow.window(flow.dialogs.reward_page(flow.reward_group)), note=note)
		flow.finish(end, "MonsterHunt.java:167, AbstractQuestHandler.java:413-474")
	else:
		flow.reward_steps(end, "MonsterHunt.java:152-167, AbstractQuestHandler.java:413-474")
	return {"kill": [m.as_json() for m in monsters], "killRun": kills, "lastKill": last, "talk": {"start": start, "end": end},
	        "autoStart": {"aggroNpcs": aggro, "invasionWorld": invasion, "startZone": start_zone, "startDistanceNpc": distance_npc}}


def kind_item_collecting(flow: Flow, q: JObj, reg: Registration) -> dict:
	"""ItemCollecting (ItemCollecting.java:25-178)."""
	t = flow.template
	start, end = _npcs(q["startNpcIds"]), _end_npcs(q)
	next_npc = q["nextNpcId"]
	action_items = flow.ctx.action_items(t)
	work = _work_item(flow)
	dd = flow.data_driven
	faction = t["category"] == "FACTION"
	at = flow.start_at(start, not start or faction, reg.talk)
	flow.add("start", at, "startable", flow.action("QUEST_SELECT"), "ItemCollecting.java:99-100",
	         flow.window(start_page(flow, q["startDialogId"])), other="AbstractQuestHandler.onDialogEvent",
	         note="SETPRO1 starts the quest without the work item and closes the window (:101-103); SELECT1_1 plays the movie and shows "
	              "page 1012 (:104-108)")
	flow.accept(at, work, "ItemCollecting.java:109-112")
	if next_npc:
		flow.add("progress", {"npcs": [next_npc]}, "START", flow.action("QUEST_SELECT"), "ItemCollecting.java:119-122", flow.window(1352),
		         guard="var0 == 0")
		flow.state.set_by_id(0, 1)
		flow.state.update()
		flow.add("progress", {"npcs": [next_npc]}, "START", flow.action("SETPRO1"), "ItemCollecting.java:123-124, AbstractQuestHandler.java:510-529",
		         flow.close(), flow.effect(questAction="UPDATE"))
	flow.add("report", {"npcs": end}, "START", flow.action("QUEST_SELECT"), "ItemCollecting.java:128-129",
	         flow.window(q["startDialogId2"] or (1011 if dd else 2375)))
	ok_page = q["checkOkDialogId"] or (10000 if dd else 5)
	fail_page = q["checkFailDialogId"] or (10001 if dd else 2716)
	flow.state.status = "REWARD"
	collect = flow.ctx.collect_items(t)
	effect = flow.effect(questAction="UPDATE", nearbyUpdate=True, removeItems=[dict(c) for c in collect] if collect else "inventory_items check")
	flow.add("report", {"npcs": end}, "START", flow.action("CHECK_USER_HAS_QUEST_ITEM"), "ItemCollecting.java:130-133, AbstractQuestHandler.java:536-555",
	         flow.quest_dialog(ok_page), effect,
	         guard=f"QuestService.collectItemCheck(env, true): every collect item in the inventory (kinah against getKinah), removed; else page "
	               f"{fail_page} and no change",
	         note="CHECK_USER_HAS_QUEST_ITEM_SIMPLE and SETPRO1..SETPRO4 take checkQuestItemsSimple with pages 5..8 and close on a failed check; "
	              "SET_SUCCEED sets REWARD without a check and closes; FINISH_DIALOG shows page 10")
	flow.fix_reward_group()
	flow.add("reward", {"npcs": end}, "REWARD", flow.action("USE_OBJECT", "QUEST_SELECT", "SELECT_QUEST_REWARD"),
	         "ItemCollecting.java:154-161, AbstractQuestHandler.java:413-474", flow.window(flow.dialogs.reward_page(flow.reward_group)),
	         note="every REWARD dialog at the end npc first removes the work item" if work else None)
	flow.finish(end, "ItemCollecting.java:161, AbstractQuestHandler.java:413-474")
	return {"collect": collect, "drops": flow.ctx.drops(t), "actionItems": action_items, "talk": {"start": start, "next": next_npc or None, "end": end},
	        "autoStart": {"startZone": q["startZone"]}}


def kind_report_to_many(flow: Flow, q: JObj, reg: Registration) -> dict:
	"""ReportToMany (ReportToMany.java:26-187)."""
	start = _npcs(q["startNpcIds"])
	infos = [(_npcs(n["npcIds"]), n["movie"]) for n in q["npcInfos"]]
	start_item = q["startItemId"]
	if q["mission"]:
		flow.not_modelled.append("mission=\"true\": ReportToMany.onLevelChangedEvent -> defaultOnLevelChangedEvent is not modelled")
	if not infos:
		raise OracleError(f"report_to_many {flow.qid}: no <npc_infos> (getMaxStep() is -1 and every START dialog throws)")
	max_step = len(infos) - 1
	dd = flow.data_driven
	work_items = flow.ctx.work_items(flow.template) or []
	at = flow.start_at(start, not start, reg.talk)
	if start_item:
		flow.add("start", {"item": start_item}, "startable", {"name": "use item", "id": None}, "ReportToMany.java:173-182",
		         flow.window(4), note="CM_USE_ITEM -> QuestEngine.onItemUseEvent: ASK_QUEST_ACCEPT_WINDOW (page 4)")
	guard = f"the start item {start_item} is in the inventory" if start_item else None
	flow.add("start", at, "startable", flow.action("QUEST_SELECT"), "ReportToMany.java:84-85", flow.window(start_page(flow, q["startDialogId"])),
	         guard=guard, other="AbstractQuestHandler.onDialogEvent")
	effect = flow.start_quest([])
	flow.add("start", at, "startable", flow.action("QUEST_ACCEPT_1", "QUEST_ACCEPT", "QUEST_ACCEPT_SIMPLE"), "ReportToMany.java:80-83",
	         flow.window(1003), effect, guard=guard, other="sendQuestStartDialog",
	         note="accepted from the start item's window (no npc) the window closes with page 0 and quest 0 (sendQuestStartDialog: the visible "
	              "object is not an Npc)" if start_item else None)
	for step in range(max_step):
		npcs, movie = infos[step]
		page = (1011 if dd else 1352) + step * 341
		flow.add("progress", {"npcs": npcs}, "START", flow.action("QUEST_SELECT"), "ReportToMany.java:101-102, 153-158", flow.window(page),
		         guard=f"var0 == {step}")
		flow.state.set_by_id(0, step + 1)
		flow.state.update()
		give = given_items([work_items[step]] if step < len(work_items) else [])
		flow.add("progress", {"npcs": npcs}, "START", flow.action(*[f"SETPRO{i}" for i in range(1, 13)]), "ReportToMany.java:103-118",
		         flow.close(), flow.effect(questAction="UPDATE", giveItems=give),
		         note=f"any other action plays movie {movie} and falls to AbstractQuestHandler.onDialogEvent" if movie else None)
	npcs, movie = infos[max_step]
	flow.add("report", {"npcs": npcs}, "START", flow.action("QUEST_SELECT"), "ReportToMany.java:101-102, 153-158",
	         flow.window(10002 if dd else 2375), guard=f"var0 == {max_step}")
	flow.state.set_by_id(0, max_step)
	flow.state.status = "REWARD"
	flow.fix_reward_group()
	removes = (flow.ctx.collect_items(flow.template) or []) + ([{"itemId": start_item, "count": 1}] if start_item else []) + work_items
	flow.add("report", {"npcs": npcs}, "START", flow.action("SELECT_QUEST_REWARD", "CHECK_USER_HAS_QUEST_ITEM", "CHECK_USER_HAS_QUEST_ITEM_SIMPLE"),
	         "ReportToMany.java:119-132, 160-170", flow.window(flow.dialogs.reward_page(flow.reward_group)),
	         flow.effect(questAction="UPDATE", nearbyUpdate=True, removeItems=removes),
	         guard="validateAndRemoveItems: collectItemCheck(env, true), the start item and every work item removed; else page 10 (quest 0)",
	         note="SET_SUCCEED at the npc of var0 counts one step more and closes the window (a pre-end npc) - and clears the handler's "
	              "rewardStatusFromRewardNpc flag, after which USE_OBJECT at the end npc in REWARD shows page 2375/10002 first (:139-143)")
	flow.add("reward", {"npcs": npcs}, "REWARD", flow.action("USE_OBJECT", "QUEST_SELECT", "SELECT_QUEST_REWARD"),
	         "ReportToMany.java:138-144, AbstractQuestHandler.java:413-474", flow.window(flow.dialogs.reward_page(flow.reward_group)))
	flow.finish(npcs, "ReportToMany.java:144, AbstractQuestHandler.java:413-474")
	return {"talk": {"start": start, "steps": [n for n, _ in infos]}, "startItem": start_item or None,
	        "movies": [m for _, m in infos]}


def kind_kill_spawned(flow: Flow, q: JObj, reg: Registration) -> dict:
	"""KillSpawned (KillSpawned.java:21-138); KillSpawnedData passes only the npc lists and the <monster>s."""
	start, end = _npcs(q["startNpcIds"]), _end_npcs(q)
	monsters = [Monster(_npcs(m["npcIds"]), m["var"], m["endVar"], m["step"], m["spawnerObjectId"]) for m in q["monster"] or []]
	at = flow.start_at(start, not start, reg.talk)
	flow.add("start", at, "startable", flow.action("QUEST_SELECT"), "KillSpawned.java:70-71", flow.window(4762 if flow.data_driven else 1011),
	         other="sendQuestStartDialog", note="with no start npc the quest has no onQuestStart registration (no quest marker) and starts "
	                                           "only when the client selects it at an npc that has its talk events")
	flow.accept(at, None, "KillSpawned.java:72-73")
	for m in monsters:
		flow.add("progress", {"npcs": [m.spawner]}, "START", flow.action("USE_OBJECT"), "KillSpawned.java:77-91", None,
		         note=f"spawns npc {m.npc_ids[0] if m.npc_ids else None} for five minutes at the spawner's first spawn spot of the map")
	state = SimState()
	kills = []
	number = 0
	for index, m in enumerate(monsters):
		for _ in range(m.end_var + 1):
			if state.status != "START" or not m.npc_ids:
				break
			number += 1
			before = (state.updates, state.nearby)
			handled = kill_spawned_kill(state, monsters, m.npc_ids[0])
			kills.append(_snapshot(state, number, m.npc_ids[0], handled, *before, monster=index))
	flow.state = state
	if state.status == "START":
		# KillSpawned sets REWARD only in onKillEvent; in START the end npc answers SELECT_QUEST_REWARD with sendQuestDialog(env, 5), which the
		# reward-window guard refuses outside REWARD (KillSpawned.java:95-103, AbstractQuestHandler.java:330-339)
		raise OracleError(f"kill_spawned {flow.qid}: the kill run does not reach REWARD (a <monster> without npc ids or with end_var 0), so the "
		                  "quest cannot be finished")
	flow.reward_steps(end, "KillSpawned.java:106-108, AbstractQuestHandler.java:413-474")
	return {"kill": [m.as_json() for m in monsters], "killRun": kills, "talk": {"start": start, "end": end}}


def kind_skill_use(flow: Flow, q: JObj, reg: Registration) -> dict:
	"""SkillUse (SkillUse.java:19-127)."""
	start, end = _npcs(q["startNpcIds"]), _end_npcs(q)
	skills = q["skills"] or []
	at = flow.start_at(start, not start, reg.talk)
	flow.add("start", at, "startable", flow.action("QUEST_SELECT"), "SkillUse.java:61-62", flow.window(4762), other="sendQuestStartDialog")
	flow.accept(at, None, "SkillUse.java:63-64")
	state = SimState()
	uses = []
	number = 0
	for index, s in enumerate(skills):
		skill = (s["skillIds"] or [None])[0]
		for _ in range(s["endVar"] + 1):
			if state.status != "START" or skill is None:
				break
			number += 1
			before = (state.updates, state.nearby)
			handled = skill_use_event(state, skills, skill)
			uses.append(_snapshot(state, number, skill, handled, *before, skill=index))
			if not handled:
				break
	flow.state = state
	if state.status == "START":
		flow.add("report", {"npcs": end}, "START", flow.action("QUEST_SELECT"), "SkillUse.java:70-71", flow.window(10002))
		flow.state.status = "REWARD"
		flow.state.update()
		flow.add("report", {"npcs": end}, "START", flow.action("SELECT_QUEST_REWARD"), "SkillUse.java:72-74", flow.quest_dialog(5),
		         flow.effect(questAction="UPDATE", nearbyUpdate=True), note="no use count is checked here (SkillUse.java:67 'TODO')")
	flow.reward_steps(end, "SkillUse.java:77-79, AbstractQuestHandler.java:413-474")
	return {"skills": [{"skillIds": s["skillIds"], "varNum": s["varNum"], "count": s["endVar"]} for s in skills], "skillRun": uses,
	        "talk": {"start": start, "end": end}}


def kind_report_on_levelup(flow: Flow, q: JObj, reg: Registration) -> dict:
	"""ReportOnLevelUp (ReportOnLevelUp.java:16-66)."""
	end = _npcs(q["endNpcIds"])
	effect = flow.start_quest([], status="REWARD")
	flow.add("start", {"trigger": "enter world or level change"}, "none", {"name": "onEnterWorld/onLevelChanged", "id": None},
	         "ReportOnLevelUp.java:52-65", None, effect,
	         guard="the quest is not in the quest list and QuestService.checkStartConditions(player, id, false) passes (level, race, ...)",
	         note="startQuest(env, REWARD, false): the quest enters the journal in REWARD state")
	flow.reward_steps(end, "ReportOnLevelUp.java:37-49, AbstractQuestHandler.java:413-474")
	return {"talk": {"end": end}}


def _ranked(flow: Flow, q: JObj, amount: int) -> list[dict]:
	state = SimState()
	rows = []
	for number in range(1, amount + 2):
		if state.status != "START":
			break
		before = (state.updates, state.nearby)
		handled = ranked_kill(state, 0, amount, True, flow.data_driven)
		rows.append(_snapshot(state, number, 0, handled, *before))
	flow.state = state
	return rows


def kind_kill_in_world(flow: Flow, q: JObj, reg: Registration) -> dict:
	"""KillInWorld (KillInWorld.java:29-168)."""
	start, end = _npcs(q["startNpcIds"]), _end_npcs(q)
	amount = q["amount"] or 1
	at = flow.start_at(start, not start, reg.talk)
	flow.add("start", at, "startable", flow.action("QUEST_SELECT"), "KillInWorld.java:105-106", flow.window(start_page(flow, q["startDialogId"])),
	         other="AbstractQuestHandler.onDialogEvent")
	flow.accept(at, None, "KillInWorld.java:107-110")
	kills = _ranked(flow, q, amount)
	page = q["endDialogId"] or (10002 if flow.data_driven else 2375)
	flow.fix_reward_group()
	flow.add("reward", {"npcs": end}, "REWARD", flow.action("QUEST_SELECT"), "KillInWorld.java:117-118", flow.window(page))
	flow.add("reward", {"npcs": end}, "REWARD", flow.action("USE_OBJECT", "SELECT_QUEST_REWARD"), "KillInWorld.java:120",
	         flow.window(flow.dialogs.reward_page(flow.reward_group)))
	flow.finish(end, "KillInWorld.java:120, AbstractQuestHandler.java:413-474")
	return {"pvpKills": {"amount": amount, "worlds": reg.kill_in_world, "minRank": q["minRank"], "levelDiff": q["levelDiff"]}, "killRun": kills,
	        "talk": {"start": start, "end": end}, "autoStart": {"invasionWorld": q["invasionWorld"], "startDistanceNpc": q["startDistanceNpcId"]}}


def kind_kill_in_zone(flow: Flow, q: JObj, reg: Registration) -> dict:
	"""KillInZone (KillInZone.java:23-130)."""
	start, end = _npcs(q["startNpcIds"]), _end_npcs(q)
	amount = q["amount"] or 1
	at = flow.start_at(start, not start, reg.talk)
	flow.add("start", at, "startable", flow.action("QUEST_SELECT"), "KillInZone.java:89-90", flow.window(4762 if flow.data_driven else 1011),
	         other="AbstractQuestHandler.onDialogEvent")
	flow.accept(at, None, "KillInZone.java:91-94")
	kills = _ranked(flow, q, amount)
	flow.fix_reward_group()
	if flow.data_driven:
		flow.add("reward", {"npcs": end}, "REWARD", flow.action("USE_OBJECT"), "KillInZone.java:101-102", flow.window(10002))
		report_actions = flow.action("QUEST_SELECT", "SELECT_QUEST_REWARD")
	else:  # without data_driven USE_OBJECT also goes to sendQuestEndDialog, which shows the reward page
		report_actions = flow.action("USE_OBJECT", "QUEST_SELECT", "SELECT_QUEST_REWARD")
	flow.add("reward", {"npcs": end}, "REWARD", report_actions, "KillInZone.java:101-103",
	         flow.window(flow.dialogs.reward_page(flow.reward_group)))
	flow.finish(end, "KillInZone.java:103, AbstractQuestHandler.java:413-474")
	return {"pvpKills": {"amount": amount, "zones": reg.kill_in_zone, "minRank": q["minRank"], "levelDiff": q["levelDiff"]}, "killRun": kills,
	        "talk": {"start": start, "end": end}}


def kind_crafting_rewards(flow: Flow, q: JObj, reg: Registration) -> dict:
	"""CraftingRewards (CraftingRewards.java:16-91)."""
	start = q["startNpcId"]
	end = q["endNpcId"] or start
	if q["levelReward"] not in CRAFTING_LEVEL_REWARDS:
		raise OracleError(f"crafting_rewards {flow.qid}: level_reward {q['levelReward']} makes canLearn throw IllegalStateException in every "
		                  "dialog at its npcs (CraftingRewards.java:84-90; QuestEngine.onDialog catches it and answers false), so no step runs")
	guard =("CraftSkillUpdateService.canLearnMoreExpertCraftingSkill" if q["levelReward"] == 400 else
	         "CraftSkillUpdateService.canLearnMoreMasterCraftingSkill")
	flow.add("start", {"npcs": [start]}, "startable", flow.action("QUEST_SELECT"), "CraftingRewards.java:53-56",
	         flow.window(4762 if flow.data_driven else 1011), guard=guard, other="sendQuestStartDialog")
	flow.accept({"npcs": [start]}, None, "CraftingRewards.java:57-58")
	flow.add("report", {"npcs": [end]}, "START", flow.action("QUEST_SELECT"), "CraftingRewards.java:62-65",
	         flow.window(1011 if flow.data_driven else 2375), guard=guard)
	effect = flow.report_to_reward([end], "", var=0)
	effect["learnSkill"] = {"skillId": q["skillId"], "level": q["levelReward"]}
	if q["questMovie"]:
		effect["movie"] = q["questMovie"]
	flow.add("report", {"npcs": [end]}, "START", flow.action("SELECT_QUEST_REWARD"), "CraftingRewards.java:66-73",
	         flow.window(flow.dialogs.reward_page(flow.reward_group)), effect, guard=guard)
	flow.reward_steps([end], "CraftingRewards.java:76-78, AbstractQuestHandler.java:413-474")
	return {"talk": {"start": start, "end": end}, "skill": {"skillId": q["skillId"], "levelReward": q["levelReward"]}}


def kind_relic_rewards(flow: Flow, q: JObj, reg: Registration) -> dict:
	"""RelicRewards (RelicRewards.java:20-103): the reward group is the relic the player turns in (SELECT1..SELECT4)."""
	start = _npcs(q["startNpcIds"])
	t = flow.template
	collect = flow.ctx.collect_items(t) or []
	start_check = t["collectItems"]["startCheck"] if t["collectItems"] is not None else None
	flow.add("start", {"npcs": start}, "startable", flow.action("EXCHANGE_COIN"), "RelicRewards.java:50-58",
	         flow.window(4762 if flow.data_driven else 1011),
	         flow.start_quest([]) if start_check else None,
	         guard="level >= minlevel_permitted and QuestService.checkAndGetCollectItemQuestRewardCategory(env) (QuestService.java:629-664): one "
	               "collect item with its count in the inventory; else page 3398",
	         note=None if start_check else "collect_items start_check is not true: the category check does not start the quest, so the "
	                                        "START branch below is never reached through this handler")
	if not start_check:
		flow.not_modelled.append("relic_rewards without start_check: the quest is never started by RelicRewards")
		return {"relics": collect, "talk": {"start": start}}
	flow.reward_group = 0
	flow.state.set_var(1)
	flow.state.status = "REWARD"
	flow.add("report", {"npcs": start}, "START", flow.action("SELECT1", "SELECT2", "SELECT3", "SELECT4"), "RelicRewards.java:61-87",
	         flow.window(5), flow.effect(questAction="UPDATE", nearbyUpdate=True, rewardGroup="index i of SELECT(i+1)"),
	         guard="var0 == 0; checkAndGetCollectItemQuestRewardCategory(env, i): the i-th collect item with its count, removed; else page 1009",
	         note="SELECT(i+1) sets rewardGroup i, var0 i + 1, REWARD and shows page i + 5 (the effect shows i = 0)")
	flow.add("reward", {"npcs": start}, "REWARD", flow.action("USE_OBJECT"), "RelicRewards.java:93-94", flow.window(5),
	         note="page var0 + 4")
	flow.finish(start, "RelicRewards.java:95-97, AbstractQuestHandler.java:413-474", action=flow.action("SELECTED_QUEST_NOREWARD"),
	            note="only SELECTED_QUEST_NOREWARD finishes; reward group i")
	return {"relics": [dict(c, rewardGroup=i) for i, c in enumerate(collect)], "talk": {"start": start}}


def kind_fountain_rewards(flow: Flow, q: JObj, reg: Registration) -> dict:
	"""FountainRewards (FountainRewards.java:20-81)."""
	start = _npcs(q["startNpcIds"])
	flow.add("start", {"npcs": start}, "startable", flow.action("USE_OBJECT"), "FountainRewards.java:48-52", flow.selection(),
	         guard="QuestService.inventoryItemCheck(env, true); without the inventory items the handler answers true with only a system message")
	effect = flow.start_quest([])
	flow.state.status = "REWARD"
	flow.state.update()
	flow.fix_reward_group()
	flow.add("start", {"npcs": start}, "startable", flow.action("SETPRO1"), "FountainRewards.java:53-66", flow.quest_dialog(5),
	         dict(flow.effect(questAction="ADD then UPDATE", nearbyUpdate=True), startQuest=True),
	         guard="collectItemCheck(env, false) and a special cube with room; else page 10")
	collect = flow.ctx.collect_items(flow.template)
	flow.finish(start, "FountainRewards.java:69-75", action=flow.action("SELECTED_QUEST_NOREWARD"),
	            note=f"collectItemCheck(env, true) removes {collect} first; any other REWARD action abandons the quest (QuestService.abandonQuest)")
	return {"coins": collect, "talk": {"start": start}}


def kind_item_order(flow: Flow, q: JObj, reg: Registration) -> dict:
	"""ItemOrders (ItemOrders.java:22-121): the start item is the first work item."""
	work_items = flow.ctx.work_items(flow.template)
	start_item = work_items[0]["itemId"] if work_items else 0
	talk1, talk2, end = q["talkNpcId1"], q["talkNpcId2"], q["endNpcId"]
	flow.add("start", {"item": start_item}, "startable", {"name": "use item", "id": None}, "ItemOrders.java:113-120", flow.window(4))
	effect = flow.start_quest([])
	flow.add("start", {"item": start_item}, "startable", flow.action("QUEST_ACCEPT_1", "QUEST_ACCEPT", "QUEST_ACCEPT_SIMPLE"),
	         "ItemOrders.java:63-74", flow.close(), effect, guard=f"item {start_item} is in the inventory (else STR_QUEST_ACQUIRE_ERROR_INVENTORY_ITEM)")
	talks = [n for n in (talk1, talk2) if n]
	for index, npc in enumerate(talks):
		flow.add("progress", {"npcs": [npc]}, "START", flow.action("QUEST_SELECT"), "ItemOrders.java:80-82", flow.window(1352))
		reward = (index == 0 and talk2 == 0) or (index == 1 and talk2 != 0)
		flow.state.set_by_id(0, flow.state.get(0) + 1)
		if reward:
			flow.state.status = "REWARD"
		flow.state.update()
		flow.add("progress", {"npcs": [npc]}, "START", flow.action("SETPRO1"), "ItemOrders.java:83-89", flow.close(),
		         flow.effect(questAction="UPDATE", nearbyUpdate=reward))
	if flow.state.status == "START":
		flow.add("report", {"npcs": [end]}, "START", flow.action("QUEST_SELECT"), "ItemOrders.java:92-93", flow.window(2375))
		flow.state.set_by_id(0, 1)
		flow.state.status = "REWARD"
		flow.fix_reward_group()
		flow.add("report", {"npcs": [end]}, "START", flow.action("SELECT_QUEST_REWARD"), "ItemOrders.java:94-95, AbstractQuestHandler.java:510-529",
		         flow.window(flow.dialogs.reward_page(flow.reward_group)), flow.effect(questAction="UPDATE", nearbyUpdate=True),
		         guard="var0 == 0 (defaultCloseDialog(env, 0, 1, true, true))")
	flow.fix_reward_group()
	flow.add("reward", {"npcs": [end]}, "REWARD", flow.action("USE_OBJECT"), "ItemOrders.java:101-102", flow.window(2375))
	flow.add("reward", {"npcs": [end]}, "REWARD", flow.action("QUEST_SELECT", "SELECT_QUEST_REWARD"), "ItemOrders.java:103-104",
	         flow.window(flow.dialogs.reward_page(flow.reward_group)))
	flow.finish([end], "ItemOrders.java:104, AbstractQuestHandler.java:413-474")
	if start_item == 0:
		flow.not_modelled.append("no work item: ItemOrders registers item 0 as its start item and the quest cannot start")
	return {"startItem": start_item or None, "talk": {"talk": talks, "end": end}}


def kind_work_order(flow: Flow, q: JObj, reg: Registration) -> dict:
	"""WorkOrders (WorkOrders.java:26-105)."""
	start = _npcs(q["startNpcIds"])
	components = [{"itemId": c["itemId"], "count": c["count"]} for c in q["giveComponents"] or []]
	flow.add("start", {"npcs": start}, "startable", flow.action("QUEST_SELECT"), "WorkOrders.java:57-58", flow.window(flow.dialogs.pages["ASK_QUEST_ACCEPT_WINDOW"]))
	effect = flow.start_quest(components)
	effect["recipe"] = q["recipeId"]
	flow.add("start", {"npcs": start}, "startable", flow.action("QUEST_ACCEPT_1"), "WorkOrders.java:59-69", flow.close(), effect,
	         guard="RecipeService.validateNewRecipe(player, recipe) != null", note="the components go through ItemService.addItem; COMBINE_TASK "
	                                                                             "shows COMBINETASK_WINDOW with quest 0")
	flow.state.status = "REWARD"
	flow.state.update()
	flow.fix_reward_group()
	flow.add("report", {"npcs": start}, "START", flow.action("QUEST_SELECT"), "WorkOrders.java:74-83",
	         flow.quest_dialog(flow.dialogs.pages["SELECT_QUEST_REWARD_WINDOW1"]), flow.effect(questAction="UPDATE", nearbyUpdate=True),
	         guard="collectItemCheck(env, false) (the crafted items), else page 10; the work items are removed")
	flow.finish(start, "WorkOrders.java:85-100", action=flow.action("USE_OBJECT"), window=flow.window(1008), follow_up=False,
	            note="REWARD: every dialog removes the collect items and the recipe; USE_OBJECT calls finishQuest itself and shows page 1008, any "
	                 "other action goes through sendQuestEndDialog")
	return {"components": components, "recipe": q["recipeId"], "collect": flow.ctx.collect_items(flow.template), "talk": {"start": start}}


def kind_xml_quest(flow: Flow, q: JObj, reg: Registration) -> dict:
	"""XmlQuest (XmlQuest.java:22-104): only the registration is modelled."""
	start, end = _npcs(q["startNpcIds"]), _end_npcs(q)
	flow.not_modelled.append("steps: the xmlQuest operation language (on_talk_event/on_kill_event conditions and operations, "
	                         "questEngine/handlers/models/xmlQuest) is not modelled; its events run before the template's own dialog")
	return {"talk": {"start": start, "end": end}, "events": {"onTalkEvent": q["onTalkEvents"], "onKillEvent": q["onKillEvents"]}}


KINDS = {
	"report_to": kind_report_to,
	"monster_hunt": kind_monster_hunt,
	"item_collecting": kind_item_collecting,
	"report_to_many": kind_report_to_many,
	"kill_spawned": kind_kill_spawned,
	"skill_use": kind_skill_use,
	"report_on_levelup": kind_report_on_levelup,
	"kill_in_world": kind_kill_in_world,
	"kill_in_zone": kind_kill_in_zone,
	"crafting_rewards": kind_crafting_rewards,
	"relic_rewards": kind_relic_rewards,
	"fountain_rewards": kind_fountain_rewards,
	"item_order": kind_item_order,
	"work_order": kind_work_order,
	"xml_quest": kind_xml_quest,
	"mentor_monster_hunt": lambda flow, q, reg: kind_monster_hunt(flow, q, reg, mentor=True),
}

# The data class fields each handler's constructor takes (the rest are bound by JAXB and ignored by the handler)
USED_FIELDS = {
	"report_to": {"id", "startNpcIds", "endNpcIds", "startDialogId"},
	"monster_hunt": {"id", "startNpcIds", "endNpcIds", "startDialogId", "endDialogId", "aggroNpcIds", "invasionWorld", "startZone",
	                 "startDistanceNpcId", "reward", "rewardNextStep"},
	"item_collecting": {"id", "startNpcIds", "endNpcIds", "nextNpcId", "startZone", "questMovie", "startDialogId", "startDialogId2",
	                    "checkOkDialogId", "checkFailDialogId"},
	"report_to_many": {"id", "startItemId", "startNpcIds", "npcInfos", "startDialogId", "mission"},
	"kill_spawned": {"id", "startNpcIds", "endNpcIds", "monster"},
	"skill_use": {"id", "startNpcIds", "endNpcIds", "skills"},
	"report_on_levelup": {"id", "endNpcIds"},
	"kill_in_world": {"id", "endNpcIds", "startNpcIds", "worldIds", "amount", "minRank", "levelDiff", "invasionWorld", "startDialogId",
	                  "startDistanceNpcId", "endDialogId"},
	"kill_in_zone": {"id", "endNpcIds", "startNpcIds", "zones", "amount", "minRank", "levelDiff", "startDistanceNpc"},
	"crafting_rewards": {"id", "startNpcId", "skillId", "levelReward", "endNpcId", "questMovie"},
	"relic_rewards": {"id", "startNpcIds"},
	"fountain_rewards": {"id", "startNpcIds"},
	"item_order": {"id", "talkNpcId1", "talkNpcId2", "endNpcId"},
	"work_order": {"id", "startNpcIds", "giveComponents", "recipeId"},
	"xml_quest": {"id", "startNpcIds", "endNpcIds", "onTalkEvents", "onKillEvents"},
	"mentor_monster_hunt": {"id", "startNpcIds", "endNpcIds", "minMenteLevel", "maxMenteLevel", "reward", "rewardNextStep"},
}
