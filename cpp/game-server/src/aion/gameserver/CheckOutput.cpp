#include "aion/gameserver/CheckOutput.h"

#include <algorithm>
#include <fstream>
#include <optional>
#include <thread>
#include <unordered_set>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/LiveInstanceCounters.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"
#include "aion/gameserver/skillengine/effect/AbstractHealEffect.h"
#include "aion/gameserver/skillengine/effect/BleedEffect.h"
#include "aion/gameserver/skillengine/effect/DPTransferEffect.h"
#include "aion/gameserver/skillengine/effect/DamageEffect.h"
#include "aion/gameserver/skillengine/effect/EffectTemplate.h"
#include "aion/gameserver/skillengine/effect/FpAttackInstantEffect.h"
#include "aion/gameserver/skillengine/effect/HealOverTimeEffect.h"
#include "aion/gameserver/skillengine/effect/MpAttackInstantEffect.h"
#include "aion/gameserver/skillengine/effect/PoisonEffect.h"
#include "aion/gameserver/skillengine/effect/SpellAttackEffect.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver {

namespace {

const commons::logging::Logger& log() {
	static const auto* instance = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.CheckOutput"));
	return *instance;
}

std::ofstream openOutput(const std::filesystem::path& file) {
	std::filesystem::create_directories(file.parent_path());
	std::ofstream out(file, std::ios::binary | std::ios::trunc);
	if (!out)
		throw commons::utils::IOException("Cannot write " + file.string());
	return out;
}

/** the file name of a source location without its directories */
std::string_view fileName(std::string_view path) {
	size_t slash = path.find_last_of("/\\");
	return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

/**
 * runtime::liveInstancesOf's name rule (LiveInstanceCounters.h: equal to the counter's class name, or a trailing part of one after a "::")
 * without its `live != 0` filter, which summaryLiveCounts() must not have: a class at 0 live with a non-zero `created` is exactly the row that
 * turns a zeroLiveClasses() guard into an assertion. LiveInstanceCounters.cpp:118-132 is the source of truth for the rule;
 * CheckOutputTest.SummaryLiveCountsUseTheLiveInstancesOfNameRule pins the two against each other.
 */
bool matchesClassName(const std::string& className, const std::string& name) {
	return className == name || (className.size() > name.size() + 2 && className.ends_with(name) &&
									className.compare(className.size() - name.size() - 2, 2, "::") == 0);
}

/**
 * m5b2-plan.md G-07: what the creatures of the world legitimately hold of the skill classes when the summary is written, so that a gate can
 * compare the live counts of summaryLiveClasses() against a RELATION instead of a zero. The effect classes cannot be strict zero rows: every
 * spawn casts its post-spawn skills (NpcSkillList.getPostSpawnSkills), whose statup buffs last 86,400,000 ms and survive the shutdown with their
 * npcs (309 Effects and 309 Skills live in every gate run of 2026-09-24), which is the precedent of StatFunctionProxy (docs/deviations/P5-14.md).
 * What a leak breaks is the equality between what is alive and what something that is still alive is entitled to hold:
 * - effectsHeld: the distinct Effects of every creature's EffectController (EffectController::getAllEffects, the abnormal and the passive map),
 *   closed over the two Effect fields that retain another Effect (subEffect, designatedDispelEffect). An Effect that ended but is still
 *   retained - by an observer its endEffect did not remove, a stat function it did not take back, a periodic task it did not cancel - is alive
 *   and in no controller, so `live Effect > effectsHeld`;
 * - skillsHeld: the distinct Skills those Effects reference (Effect::skill), plus every creature's casting skill (an npc may be mid-cast when
 *   the stop file is written). A Skill kept past its cast (a DeathObserver or a cast task that was not released) breaks `live Skill ==
 *   skillsHeld`. Each Skill owns one StartMovingListener, so `live StartMovingListener == live Skill` holds too, but it is NOT a check of
 *   Skill.removeObservers: Skill.useSkill attaches the listener with ObserveController.attach, i.e. for one notification
 *   (ObserveController.java:31-34), so a listener removeObservers forgot is dropped at its caster's next move, and a player's whole
 *   ObserveController goes with the Player at logout. The row could only see a forgotten listener on an npc caster that never moves again
 *   before the stop; SkillCastPhasesTest is where removeObservers is checked (m5b2-plan.md §10.7: mutant R3 passed the whole gate);
 * - effectReservedCapacity: the templates of those Effects whose class stores an EffectReserved. Every Effect.setReserveds of the Java tree is
 *   in one of storesReserved()'s nine classes, called once per Effect from the template's calculate or startEffect, and only the Effect keeps
 *   the EffectReserved (fieldmap K3, Effect::reservedEffects), so `live EffectReserved <= effectReservedCapacity`. Still a bound and not an
 *   equality: Effect::reservedEffects is private and Effect.h has no accessor for it (the header request of m5b2-plan.md §7), so the walk
 *   cannot count what the held Effects really store. The bound is tight in practice because the post-spawn statup buffs that make up the held
 *   Effects store none: the capacity is 0 in the gates of 2026-09-24, and a reserved kept past its Effect is caught (§10.7: mutant R2, 6 live
 *   against 0). Counting every template instead (530 in those gates) made the row unable to fail.
 * std::nullopt when this process never created a world (DataManager::WORLD_MAPS_DATA is empty, e.g. a unit test): World::getInstance() would
 * try to build one from the missing static data. After RuntimeLifecycle::shutdown nothing runs any more, so the walk sees a still world; it
 * only borrows (Ptr), so it changes no live or created count.
 */
struct HeldEffects {
	size_t effects = 0;
	size_t skills = 0;
	size_t reservedCapacity = 0;
};

/**
 * The effect classes whose instances call Effect.setReserveds, each once per Effect. The Java tree has nine call sites and no other:
 * AttackUtil.java:401 (calculateEffectResult, reached only from calculateSkillResult, which only DamageEffect and its subclasses call),
 * AbstractHealEffect.java:30, BleedEffect.java:37, DPTransferEffect.java:30, FpAttackInstantEffect.java:36, HealOverTimeEffect.java:33 (in
 * startEffect, not in the periodic action), MpAttackInstantEffect.java:31, PoisonEffect.java:37 and SpellAttackEffect.java:31. Subclasses
 * match through their base (SkillAttackInstantEffect is a DamageEffect, HealEffect a HealOverTimeEffect, ...).
 */
bool storesReserved(const skillengine::effect::EffectTemplate& effectTemplate) {
	namespace effect = skillengine::effect;
	return dynamic_cast<const effect::DamageEffect*>(&effectTemplate) != nullptr ||
		   dynamic_cast<const effect::AbstractHealEffect*>(&effectTemplate) != nullptr ||
		   dynamic_cast<const effect::BleedEffect*>(&effectTemplate) != nullptr ||
		   dynamic_cast<const effect::DPTransferEffect*>(&effectTemplate) != nullptr ||
		   dynamic_cast<const effect::FpAttackInstantEffect*>(&effectTemplate) != nullptr ||
		   dynamic_cast<const effect::HealOverTimeEffect*>(&effectTemplate) != nullptr ||
		   dynamic_cast<const effect::MpAttackInstantEffect*>(&effectTemplate) != nullptr ||
		   dynamic_cast<const effect::PoisonEffect*>(&effectTemplate) != nullptr ||
		   dynamic_cast<const effect::SpellAttackEffect*>(&effectTemplate) != nullptr;
}

std::optional<HeldEffects> heldEffects() {
	if (!dataholders::DataManager::WORLD_MAPS_DATA)
		return std::nullopt;
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::SHUTDOWN));
	std::unordered_set<const skillengine::model::Effect*> effects;
	std::unordered_set<const skillengine::model::Skill*> skills;
	std::vector<runtime::Ptr<skillengine::model::Effect>> pending;
	world::World::getInstance().forEachObject([&effects, &skills, &pending](model::gameobjects::VisibleObject& object) {
		auto* creature = dynamic_cast<model::gameobjects::Creature*>(&object);
		if (creature == nullptr)
			return;
		if (runtime::Ptr<skillengine::model::Skill> casting = creature->getCastingSkill())
			skills.insert(casting.get());
		runtime::Ptr<controllers::effect::EffectController> controller = creature->getEffectController();
		if (!controller)
			return;
		for (runtime::Ptr<skillengine::model::Effect> effect : controller->getAllEffects())
			pending.push_back(effect);
	});
	HeldEffects held;
	while (!pending.empty()) {
		runtime::Ptr<skillengine::model::Effect> effect = pending.back();
		pending.pop_back();
		if (!effect || !effects.insert(effect.get()).second)
			continue;
		if (runtime::Ptr<skillengine::model::Skill> skill = effect->getSkill())
			skills.insert(skill.get());
		for (const skillengine::effect::EffectTemplate* effectTemplate : effect->getEffectTemplates())
			if (storesReserved(*effectTemplate))
				held.reservedCapacity++;
		pending.push_back(effect->getSubEffect());
		pending.push_back(effect->getDesignatedDispelEffect());
	}
	held.effects = effects.size();
	held.skills = skills.size();
	return held;
}

/** the three G-07 rows of m5a_summary.txt (heldEffects), "unknown" where no world was walked */
void writeHeldEffects(std::ostream& out, const std::optional<HeldEffects>& held) {
	const auto value = [&held](size_t HeldEffects::*field) { return held ? std::to_string((*held).*field) : std::string("unknown"); };
	out << "effectsHeld " << value(&HeldEffects::effects) << '\n';
	out << "skillsHeld " << value(&HeldEffects::skills) << '\n';
	out << "effectReservedCapacity " << value(&HeldEffects::reservedCapacity) << '\n';
}

} // namespace

void CheckOutput::writeUnportedTrace(const std::filesystem::path& dir) {
	std::ofstream out = openOutput(dir / "unported_trace.txt");
	runtime::writeUnportedTrace(out);
}

void CheckOutput::writePartialTrace(const std::filesystem::path& dir) {
	std::ofstream out = openOutput(dir / "partial_trace.txt");
	runtime::writePartialTrace(out);
}

void CheckOutput::writeLiveCounts(const std::filesystem::path& file) {
	std::ofstream out = openOutput(file);
	runtime::writeLiveCounts(out);
}

std::vector<runtime::LeakCensus::LeakReport> CheckOutput::runFinalCensus(const std::filesystem::path& dir, const std::function<bool()>& playersLeft,
	std::chrono::milliseconds logoutTimeout) {
	const auto deadline = std::chrono::steady_clock::now() + logoutTimeout;
	while (playersLeft() && std::chrono::steady_clock::now() < deadline)
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	drainPools(deadline);

	runtime::LeakCensus& census = runtime::LeakCensus::getInstance();
	runtime::LeakCensus::Config config = census.getConfig();
	config.censusAfter = std::chrono::milliseconds(0);
	config.checkInterval = std::chrono::milliseconds(0);
	config.zombieBreakerEnabled = false;
	census.configure(config);
	runtime::Reclaimer::getInstance().reclaimNow();
	runtime::Reclaimer::getInstance().reclaimNow();

	// An entry whose object has refcount 0 is NOT a leak: LeakCensus only ever reports an object that was still referenced when a census check
	// ran (`if (count == 0) continue; // waiting for reclamation`), so a 0 here means the last reference went away after that check and the
	// object is only waiting for the reclaimer to sweep it - which also removes its entry from the table. Under a loaded machine that window is
	// wide enough to put a Player into census.txt with refcount 0 while live_counts.txt already reports 0 live Players (measured in a full
	// `ctest -j 6`). Reclaim until no such entry is left, then report what is really still referenced.
	std::vector<runtime::LeakCensus::LeakReport> leaks;
	for (;;) {
		leaks = census.getLeaks();
		const bool pending = std::ranges::any_of(leaks, [](const runtime::LeakCensus::LeakReport& leak) { return leak.refCount == 0; });
		if (!pending || std::chrono::steady_clock::now() >= deadline)
			break;
		std::this_thread::sleep_for(std::chrono::milliseconds(25));
		runtime::Reclaimer::getInstance().reclaimNow();
	}
	std::erase_if(leaks, [](const runtime::LeakCensus::LeakReport& leak) { return leak.refCount == 0; });
	{
		std::ofstream out = openOutput(dir / "census.txt");
		writeCensus(out, leaks);
	}
	runBreakerPass();
	return leaks;
}

void CheckOutput::runBreakerPass() {
	runtime::LeakCensus& census = runtime::LeakCensus::getInstance();
	const runtime::LeakCensus::Config previous = census.getConfig();
	runtime::LeakCensus::Config config = previous;
	config.censusAfter = std::chrono::milliseconds(0);
	config.checkInterval = std::chrono::milliseconds(0);
	config.zombieBreakerEnabled = true;
	config.zombieBreakAfter = std::chrono::milliseconds(0);
	config.stalePinAfter = std::chrono::milliseconds(0);
	config.stalePinCheckInterval = std::chrono::milliseconds(0);
	census.configure(config); // resets the "next check" times, so the following scan runs both checks whatever the run did before
	runtime::Reclaimer::getInstance().reclaimNow();
	// the breaker bodies are posted to the instant pool: wait for them, so zombieCutCount() is final when this returns
	drainPools(std::chrono::steady_clock::now() + std::chrono::seconds(5));
	runtime::Reclaimer::getInstance().reclaimNow(); // frees what the cuts released
	// Restore the run's own thresholds. Without this the census stays armed at age 0 for the rest of the shutdown (RuntimeLifecycle::shutdown runs
	// after this step), so an object removed from the world during that shutdown could be cut and logged after main.cpp sampled zombieCutCount():
	// the summary and the log would then disagree. What this pass itself cut stays visible through zombieCuts, which §5.7 Q8 asserts is 0.
	census.configure(previous);
}

void CheckOutput::drainPools(std::chrono::steady_clock::time_point deadline) {
	// The plan's "drain the pools" step of F-07. There is no pool-wide quiesce call, so one barrier task per pool is queued behind everything
	// that was queued before it: when the barriers ran, no logout task of the network shutdown is still pending and pinning its Player. Periodic
	// tasks are untouched (RuntimeLifecycle::shutdown cancels them afterwards).
	utils::ThreadPoolManager& pool = utils::ThreadPoolManager::getInstance();
	std::vector<runtime::FutureRef> barriers;
	barriers.push_back(pool.submit([] {}));
	barriers.push_back(pool.submitLongRunning([] {}));
	for (const runtime::FutureRef& barrier : barriers) {
		if (!barrier) // the pools are already shut down: nothing is queued
			continue;
		while (!barrier->isDone() && std::chrono::steady_clock::now() < deadline)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

void CheckOutput::writeCensus(std::ostream& out, const std::vector<runtime::LeakCensus::LeakReport>& leaks) {
	out << "# final census v1\n";
	for (const runtime::LeakCensus::LeakReport& leak : leaks) {
		out << leak.className << '\t' << leak.objectId << '\t' << leak.refCount << '\t';
		for (size_t i = 0; i < leak.pinningTasks.size(); i++) {
			const runtime::TaskInfo& task = leak.pinningTasks[i];
			if (i > 0)
				out << "; ";
			out << fileName(task.where.file_name()) << ':' << task.where.line() << ' ' << task.kind;
		}
		out << '\n';
	}
}

size_t CheckOutput::writeLockdepReports(const std::filesystem::path& dir) {
	std::vector<runtime::LockOrderValidator::Report> reports = runtime::LockOrderValidator::getInstance().getReports();
	std::ofstream out = openOutput(dir / "lockdep.txt");
	out << "# lockdep reports v1\n";
	for (const runtime::LockOrderValidator::Report& report : reports)
		out << report.text << "\noccurrences " << report.occurrences << '\n';
	return reports.size();
}

void CheckOutput::writeWatchdogDumps(const std::filesystem::path& dir, const std::vector<std::string>& dumps) {
	std::ofstream out = openOutput(dir / "watchdog.txt");
	out << "# watchdog dumps v1\n";
	for (const std::string& dump : dumps)
		out << dump << '\n';
}

const std::vector<std::string>& CheckOutput::zeroLiveClasses() {
	// built once and never destroyed: the check runs from the ShutdownHook thread, after the runtime shut down
	static const auto* classes = new std::vector<std::string>{
		// the character and what hangs off it (every one of these was created and reached 0 in the M5a gate runs of stage 2). Item is NOT here:
		// it is bounded by the surviving accounts instead (accountBoundedLiveClasses)
		"model::gameobjects::player::Player",
		"model::gameobjects::player::AbyssRank",
		"model::gameobjects::player::BlockList",
		"model::gameobjects::player::Cooldowns",
		"model::gameobjects::player::Macros",
		"model::gameobjects::player::PlayerSettings",
		"model::gameobjects::player::QuestStateList",
		"model::gameobjects::player::RecipeList",
		"model::skill::PlayerSkillList",
		"model::skill::PlayerSkillEntry",
		// model::stats::calc::functions::StatFunctionProxy is NOT here any more (M5b-2 part 3): its premise was that only a character's game stats
		// hold stat functions, and closing D7 ended that - every spawn casts its post-spawn skills (NpcSkillList.getPostSpawnSkills), and the
		// statup buffs of those npcs register their functions for 86,400,000 ms and legitimately survive the shutdown with their npcs (measured
		// 1,171 of 1,171 live in gs.smoke.startup, against 0 before). A character's own proxies go with its Player, which is still a row here;
		// the live-count table still prints the class for whoever reads a run.
		// what the M5a visibility work added: the restore task that pins a Player in Q3. world::knownlist::KnownObject is NOT here any more
		// (M5b-1): its premise was that only a logged-in character builds a known list, and registering the AI handlers ended that - a walking
		// npc builds one of the npcs around it (WalkManager::targetReached -> updateKnownlist) and those entries legitimately survive the
		// shutdown with their npcs (measured 15,304 of 15,814 live, against 0 in live_counts_baseline.txt). It is bounded, not leaked -
		// KnownList::update forgets what moves out of range - and the live-count table still prints it for whoever reads a run.
		"services::LifeStatsRestoreService::HpMpRestoreTask",
		// per-session helpers: the packet blobs an enter world builds and the pending login-server request of a login
		"network::aion::iteminfo::ItemInfoBlob",
		"network::aion::skillinfo::SkillEntryWriter",
		"network::loginserver::LoginServer::LoginRequest",
		"questEngine::model::QuestEnv",
		// creatures and tasks a character owns. None of them is created by the M5a scenario (no summon, pet, kisk or gathering on the scripted
		// path), so these rows are guards for the stress run, the real client and M5b rather than assertions the gate exercises today.
		"model::gameobjects::Summon",
		"model::gameobjects::Pet",
		"model::gameobjects::Kisk",
		"skillengine::task::AbstractInteractionTask",
		"skillengine::task::GatheringTask",
		"GatheringTask_ActionObserver",
		"controllers::observer::StanceObserver",
		// M5b-1 E-03. AttackUtil makes one AttackResult per hit; it travels in the result list of SM_ATTACK and in DelayedOnAttack, and nothing
		// keeps one once the hit was applied (measured 0 live of 183 created in the M5a gate run after A-06, 0 of 93 in the geo run). This is the
		// row that catches m5b-plan.md §8 risk 2, the DelayedOnAttack that pins both creatures. DropNpc is the guard of the header note: its only
		// constructor call site is behind registerDrop's partial, so created stays 0 until M5b-3.
		"controllers::attack::AttackResult",
		"model::gameobjects::DropNpc",
	};
	return *classes;
}

const std::vector<std::string>& CheckOutput::summaryLiveClasses() {
	// built once and never destroyed, like zeroLiveClasses()
	static const auto* classes = new std::vector<std::string>{
		// the combat classes of m5b-plan.md Q2. AggroInfo and KnownObject are bounded, not zero (see the header); AttackResult and DropNpc are
		// zeroLiveClasses() rows whose `created` half only this row can show.
		"controllers::attack::AggroInfo",
		"controllers::attack::AttackResult",
		"model::gameobjects::DropNpc",
		"world::knownlist::KnownObject",
		// the npc conservation of m5b-plan.md Q3: a killed npc respawns as a NEW Npc, so `live` must come back to the baseline while `created`
		// grows by the number of respawns
		"model::gameobjects::Npc",
		// m5b2-plan.md G-07: the skill classes. None of them is a zeroLiveClasses() row - the post-spawn buffs keep 309 Effects and their 309
		// Skills alive at every shutdown - so each is a number the gate compares against a relation the summary writes beside it (heldEffects
		// above): Effect against effectsHeld, Skill against skillsHeld, StartMovingListener against Skill, EffectReserved against
		// effectReservedCapacity. The effect observers follow: each one retains its Effect (RootEffect_ActionObserver::effect,
		// Effect_ActionObserver, the AttackStatusObservers of AlwaysDodge/AlwaysResist), so an observer that endEffect did not remove keeps
		// its Effect alive and breaks the Effect relation; their own rows say which one it was.
		"skillengine::model::Effect",
		"skillengine::model::EffectReserved",
		"skillengine::model::Skill",
		"controllers::observer::StartMovingListener",
		"skillengine::model::Effect_ActionObserver",
		"skillengine::model::Effect_ActionObserver_2",
		"skillengine::effect::RootEffect_ActionObserver",
		"skillengine::effect::AlwaysDodgeEffect_AttackStatusObserver",
		"skillengine::effect::AlwaysResistEffect_AttackStatusObserver",
	};
	return *classes;
}

std::vector<runtime::LiveCount> CheckOutput::summaryLiveCounts(const std::vector<runtime::LiveCount>& counts) {
	std::vector<runtime::LiveCount> result;
	result.reserve(summaryLiveClasses().size());
	for (const std::string& className : summaryLiveClasses()) {
		runtime::LiveCount row;
		row.className = className; // the requested name, so the row's key is stable whatever the counter's qualified spelling is
		for (const runtime::LiveCount& counter : counts) {
			if (!matchesClassName(counter.className, className))
				continue;
			row.live += counter.live;
			row.created += counter.created;
		}
		result.push_back(std::move(row));
	}
	return result;
}

const std::vector<std::string>& CheckOutput::accountBoundedLiveClasses() {
	// see the header: the account warehouse of an Account that survives the shutdown keeps its Items, which is Java's own behaviour
	static const auto* classes = new std::vector<std::string>{
		"model::gameobjects::Item",
	};
	return *classes;
}

std::vector<runtime::LiveCount> CheckOutput::checkLiveCounts() {
	if constexpr (!runtime::LIVE_COUNTS_ENABLED)
		log().warn("Live instance leak check: nothing is counted in this build (AION_CHECKED is off), so the check reports nothing whatever the run "
				   "leaked; m5a_summary.txt says liveCountsEnabled false (m5a-plan.md §10.2)");
	return checkLiveCounts(runtime::liveCounts());
}

std::vector<runtime::LiveCount> CheckOutput::checkLiveCounts(const std::vector<runtime::LiveCount>& counts) {
	std::vector<std::string> checked = zeroLiveClasses();
	// The account-bounded classes are checked only while no Account survived: one that did keeps its account warehouse with its items, and how
	// many items that warehouse holds is not something this process can know while it writes its report (header, m5a-plan.md §10.1).
	const std::vector<runtime::LiveCount> accounts = runtime::liveInstancesOf(counts, {std::string("model::account::Account")});
	if (accounts.empty())
		checked.insert(checked.end(), accountBoundedLiveClasses().begin(), accountBoundedLiveClasses().end());
	else
		for (const runtime::LiveCount& unchecked : runtime::liveInstancesOf(counts, accountBoundedLiveClasses()))
			log().warn("Live instance leak check: {} of the {} {} instances are still alive, and an Account survived the shutdown (its connection "
					   "never reached LoginServer::onDisconnect), so its account warehouse may legitimately hold them: not checked here. The bound "
					   "is the scenario gate's, which knows the warehouses it filled (m5a-plan.md §10.1)",
				unchecked.live, unchecked.created, unchecked.className);

	std::vector<runtime::LiveCount> leaks = runtime::liveInstancesOf(counts, checked);
	for (const runtime::LiveCount& leak : leaks)
		log().error("Live instance leak: {} of the {} {} instances created since start are still alive after the runtime shut down (a class that "
					"belongs to a character must be at 0 once it logged out; m5a-plan.md D8)",
			leak.live, leak.created, leak.className);
	return leaks;
}

void CheckOutput::writeSummary(const std::filesystem::path& dir, const Summary& summary) {
	Summary checked = summary;
	std::optional<HeldEffects> held;
	if (summary.started) {
		checked.liveLeaks = checkLiveCounts();
		checked.summaryCounts = summaryLiveCounts(runtime::liveCounts());
		// m5b2-plan.md G-07: the relation side of the skill rows, walked right after the counts it is compared with (nothing runs any more)
		held = heldEffects();
	}
	std::ofstream out = openOutput(dir / "m5a_summary.txt");
	writeSummary(out, checked);
	writeHeldEffects(out, held);
}

void CheckOutput::writeSummary(std::ostream& out, const Summary& summary) {
	out << "started " << (summary.started ? "true" : "false") << '\n';
	out << "exitCode " << summary.exitCode << '\n';
	out << "unportedHits " << runtime::unportedHitCount() << '\n';
	out << "partialHits " << runtime::partialHitCount() << '\n';
	out << "partialSites " << runtime::partialHits().size() << '\n';
	out << "censusLeaks " << summary.censusLeaks << '\n';
	// m5b-client-session.md S-2. censusLeaks comes from runFinalCensus, which runs BEFORE the runtime shuts down; this is what the shutdown
	// itself left behind (RuntimeLifecycle::ShutdownReport::censusTracked, the number behind the "N objects removed from the world are still
	// alive" warning). Only the caller that performs the shutdown can measure it - LeakCensus::uninstall() empties the table before this file is
	// written - so a summary that says "unknown" means nobody passed it in, not that nothing survived.
	out << "censusTracked " << (summary.censusTracked ? std::to_string(*summary.censusTracked) : "unknown") << '\n';
	out << "zombieCuts " << summary.zombieCuts << '\n';
	out << "lockdepReports " << summary.lockdepReports << '\n';
	out << "watchdogDumps " << summary.watchdogDumps << '\n';
	// m5a-plan.md W-07: every notifySee/notifyNotSee/notifyNotKnow catch counts as a failure. Java logs it as log.error("", ex), i.e. with an
	// empty message, so without this counter a controller that throws on every notification is only an unattributable ERROR line.
	out << "knownListNotifyFailures " << world::knownlist::KnownList::notifyFailureCount() << '\n';
	out << "atreianPassportDisabled " << (!summary.atreianPassportDisabled ? "unknown" : *summary.atreianPassportDisabled ? "true" : "false") << '\n';
	std::vector<std::string> packets = summary.notPortedClientPackets;
	std::ranges::sort(packets);
	for (const std::string& packet : packets)
		out << "notPortedClientPacket " << packet << '\n';
	// m5a-plan.md D8: the classes of zeroLiveClasses() that are still alive. Every one of them is also an ERROR line in the server log
	// (checkLiveCounts), so a gate that only reads the log still fails; these rows say which class and how many.
	// liveCountsEnabled says whether the check could see anything at all: the live-instance counters are compiled out in a release build
	// (runtime::LIVE_COUNTS_ENABLED, AION_CHECKED), where "liveLeaks 0" means "not measured", not "nothing leaked". A gate that relies on the
	// check must assert this row is true (m5a-plan.md §10.2).
	out << "liveCountsEnabled " << (runtime::LIVE_COUNTS_ENABLED ? "true" : "false") << '\n';
	out << "liveLeaks " << summary.liveLeaks.size() << '\n';
	for (const runtime::LiveCount& leak : summary.liveLeaks)
		out << "liveLeak " << leak.className << ' ' << leak.live << '\n';
	// M5b-1 E-03: the classes a gate reads as numbers (summaryLiveClasses()). A liveLeak row only exists for a class that is already failing, so
	// without these rows nothing in the summary can say that an AttackResult was ever created - and "0 live" of a class that was never created is
	// a guard, not an assertion (m5a-plan.md §10.2).
	for (const runtime::LiveCount& count : summary.summaryCounts)
		out << "liveCount " << count.className << ' ' << count.live << ' ' << count.created << '\n';
}

} // namespace aion::gameserver
