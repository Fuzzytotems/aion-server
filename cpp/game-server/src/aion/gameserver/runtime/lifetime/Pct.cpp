// PCT scheduler (see Pct.h). Controlled threads hand a single "turn" to each other under one mutex; hooks installed through base's
// YieldPoint.h route every yield point and blocking bracket of a controlled thread here.

#include "aion/gameserver/runtime/lifetime/Pct.h"

#include <algorithm>
#include <condition_variable>
#include <cstdlib>
#include <exception>
#include <memory>
#include <mutex>
#include <random>
#include <stdexcept>
#include <thread>
#include <utility>

#include "aion/gameserver/runtime/lifetime/detail/Epoch.h"

namespace aion::gameserver::runtime::pct {

namespace {

enum class ThreadState { STARTING, READY, RUNNING, BLOCKED, FINISHED };

struct ControlledThread {
	ThreadState state = ThreadState::STARTING;
	int64_t priority = 0;
	std::condition_variable turn;
};

struct Schedule {
	Options options;
	std::mutex mutex;
	std::vector<std::unique_ptr<ControlledThread>> threads;
	std::condition_variable progress;
	size_t started = 0;
	size_t finished = 0;
	int32_t running = -1;
	uint32_t steps = 0;
	/** (step, new priority), in generation order */
	std::vector<std::pair<uint32_t, int64_t>> changePoints;
	bool abandoned = false;
	size_t scriptIndex = 0;
	uint32_t scriptHits = 0;
	std::string failure;
	std::vector<std::string> trace;
};

thread_local Schedule* currentSchedule = nullptr;
thread_local int32_t currentIndex = -1;
std::atomic<bool> schedulerActive{false};

void fail(Schedule& s, std::string message) {
	if (s.failure.empty())
		s.failure = std::move(message);
}

/** Next thread to run (lock held): the script target if it can run, else the highest priority READY/RUNNING thread; -1 if none. */
int32_t choose(Schedule& s) {
	const std::vector<ScriptStep>& script = s.options.script;
	while (s.scriptIndex < script.size()) {
		const ScriptStep& step = script[s.scriptIndex];
		if (step.thread >= s.threads.size()) {
			fail(s, "PCT script step " + std::to_string(s.scriptIndex) + ": thread index " + std::to_string(step.thread) + " out of range");
			s.scriptIndex = script.size();
			break;
		}
		ControlledThread& target = *s.threads[step.thread];
		if (target.state == ThreadState::FINISHED) {
			if (step.untilSite.empty()) {
				++s.scriptIndex;
				s.scriptHits = 0;
				continue;
			}
			fail(s, "PCT script step " + std::to_string(s.scriptIndex) + ": thread " + std::to_string(step.thread) + " finished before reaching '" +
				step.untilSite + "' (" + std::to_string(s.scriptHits) + "/" + std::to_string(step.occurrences) + ")");
			s.scriptIndex = script.size();
			break;
		}
		if (target.state == ThreadState::READY || target.state == ThreadState::RUNNING)
			return static_cast<int32_t>(step.thread);
		break; // blocked (or not started): let others run by priority
	}
	int32_t best = -1;
	for (size_t i = 0; i < s.threads.size(); ++i) {
		const ControlledThread& thread = *s.threads[i];
		if ((thread.state == ThreadState::READY || thread.state == ThreadState::RUNNING) &&
			(best < 0 || thread.priority > s.threads[static_cast<size_t>(best)]->priority))
			best = static_cast<int32_t>(i);
	}
	return best;
}

void transferTo(Schedule& s, int32_t next) {
	s.running = next;
	if (next >= 0) {
		ControlledThread& thread = *s.threads[static_cast<size_t>(next)];
		thread.state = ThreadState::RUNNING;
		thread.turn.notify_one();
	}
}

void waitForTurn(Schedule& s, std::unique_lock<std::mutex>& lock, int32_t self) {
	s.threads[static_cast<size_t>(self)]->turn.wait(lock, [&] { return s.running == self || s.abandoned; });
}

void onYield(const char* site) noexcept {
	Schedule* s = currentSchedule;
	if (s == nullptr)
		return;
	try {
		std::unique_lock lock(s->mutex);
		if (s->abandoned)
			return;
		const int32_t self = currentIndex;
		++s->steps;
		if (s->options.keepTrace)
			s->trace.push_back("t" + std::to_string(self) + ":" + site);
		if (s->scriptIndex < s->options.script.size()) {
			const ScriptStep& step = s->options.script[s->scriptIndex];
			if (static_cast<int32_t>(step.thread) == self && !step.untilSite.empty() && step.untilSite == site &&
				++s->scriptHits >= std::max<uint32_t>(1, step.occurrences)) {
				++s->scriptIndex;
				s->scriptHits = 0;
			}
		}
		for (const auto& [step, priority] : s->changePoints) {
			if (step == s->steps)
				s->threads[static_cast<size_t>(self)]->priority = priority;
		}
		int32_t next = choose(*s);
		if (next == self || next < 0)
			return;
		s->threads[static_cast<size_t>(self)]->state = ThreadState::READY;
		transferTo(*s, next);
		waitForTurn(*s, lock, self);
	} catch (...) {
	}
}

void onBeforeBlocking(const char* site) noexcept {
	Schedule* s = currentSchedule;
	if (s == nullptr)
		return;
	try {
		std::unique_lock lock(s->mutex);
		if (s->abandoned)
			return;
		const int32_t self = currentIndex;
		++s->steps;
		if (s->options.keepTrace)
			s->trace.push_back("t" + std::to_string(self) + ":blocking:" + site);
		s->threads[static_cast<size_t>(self)]->state = ThreadState::BLOCKED;
		s->running = -1;
		transferTo(*s, choose(*s));
	} catch (...) {
	}
}

void onAfterBlocking() noexcept {
	Schedule* s = currentSchedule;
	if (s == nullptr)
		return;
	try {
		std::unique_lock lock(s->mutex);
		if (s->abandoned)
			return;
		const int32_t self = currentIndex;
		s->threads[static_cast<size_t>(self)]->state = ThreadState::READY;
		if (s->running < 0)
			transferTo(*s, choose(*s));
		waitForTurn(*s, lock, self);
	} catch (...) {
	}
}

const PctHooks hooks{&onYield, &onBeforeBlocking, &onAfterBlocking};

void threadMain(const std::shared_ptr<Schedule>& schedule, int32_t index, std::function<void()>& body) {
	Schedule& s = *schedule;
	currentSchedule = &s;
	currentIndex = index;
	{
		std::unique_lock lock(s.mutex);
		s.threads[static_cast<size_t>(index)]->state = ThreadState::READY;
		++s.started;
		s.progress.notify_all();
		waitForTurn(s, lock, index);
	}
	try {
		body();
	} catch (const std::exception& e) {
		std::scoped_lock lock(s.mutex);
		fail(s, std::string("thread ") + std::to_string(index) + ": " + e.what());
	} catch (...) {
		std::scoped_lock lock(s.mutex);
		fail(s, "thread " + std::to_string(index) + ": unknown exception");
	}
	runtime::detail::flushThreadRetireList(); // inside the schedule (it has a yield point)
	{
		std::scoped_lock lock(s.mutex);
		s.threads[static_cast<size_t>(index)]->state = ThreadState::FINISHED;
		++s.finished;
		if (!s.abandoned) {
			if (s.running == index)
				s.running = -1;
			if (s.running < 0)
				transferTo(s, choose(s));
		}
		s.progress.notify_all();
	}
	currentSchedule = nullptr;
	currentIndex = -1;
}

} // namespace

PctScheduler::PctScheduler(Options options) : options(std::move(options)) {
}

PctScheduler::~PctScheduler() = default;

ScheduleResult PctScheduler::run(std::vector<std::function<void()>> threadBodies) {
	bool expected = false;
	if (!schedulerActive.compare_exchange_strong(expected, true))
		throw std::logic_error("PctScheduler::run: another schedule is running");

	auto schedule = std::make_shared<Schedule>();
	Schedule& s = *schedule;
	s.options = options;
	const size_t n = threadBodies.size();
	std::mt19937_64 random(options.seed);
	std::vector<int64_t> priorities(n);
	const int64_t depth = std::max<int64_t>(1, options.depth);
	for (size_t i = 0; i < n; ++i)
		priorities[i] = depth + static_cast<int64_t>(i);
	for (size_t i = n; i > 1; --i)
		std::swap(priorities[i - 1], priorities[random() % i]);
	for (size_t i = 0; i < n; ++i) {
		s.threads.push_back(std::make_unique<ControlledThread>());
		s.threads.back()->priority = priorities[i];
	}
	const uint32_t k = options.maxSteps > 0 ? options.maxSteps : 1000;
	for (int64_t i = 0; i + 1 < depth && static_cast<uint64_t>(i) < k; ++i) {
		uint32_t step;
		do {
			step = 1 + static_cast<uint32_t>(random() % k);
		} while (std::ranges::any_of(s.changePoints, [step](const auto& point) { return point.first == step; }));
		s.changePoints.emplace_back(step, i + 1);
	}

	installHooks(&hooks);
	std::vector<std::thread> threads;
	threads.reserve(n);
	for (size_t i = 0; i < n; ++i)
		threads.emplace_back([schedule, index = static_cast<int32_t>(i), body = std::move(threadBodies[i])]() mutable { threadMain(schedule, index, body); });

	ScheduleResult result;
	result.seed = options.seed;
	const auto deadline = std::chrono::steady_clock::now() + options.timeout;
	bool timedOut = false;
	{
		std::unique_lock lock(s.mutex);
		if (!s.progress.wait_until(lock, deadline, [&] { return s.started == n; })) {
			timedOut = true;
		} else {
			transferTo(s, choose(s));
			if (!s.progress.wait_until(lock, deadline, [&] { return s.finished == n; }))
				timedOut = true;
		}
		if (timedOut) {
			s.abandoned = true;
			for (auto& thread : s.threads)
				thread->turn.notify_all();
			fail(s, "PCT schedule timed out after " + std::to_string(options.timeout.count()) + " ms (deadlock or livelock)");
			s.progress.wait_for(lock, std::chrono::seconds(5), [&] { return s.finished == n; });
		}
		if (s.scriptIndex < s.options.script.size() && s.failure.empty())
			fail(s, "PCT script not completed: stopped at step " + std::to_string(s.scriptIndex));
		result.steps = s.steps;
		result.failure = s.failure;
		result.trace = s.trace;
	}
	for (size_t i = 0; i < n; ++i) {
		std::thread& thread = threads[i];
		bool done;
		{
			std::scoped_lock lock(s.mutex);
			done = !timedOut || s.threads[i]->state == ThreadState::FINISHED;
		}
		if (done)
			thread.join();
		else
			thread.detach(); // still blocked after the grace period: leaked on purpose (the shared schedule keeps its state alive)
	}
	installHooks(nullptr);
	schedulerActive.store(false);
	result.deadlock = timedOut;
	result.completed = result.failure.empty() && !timedOut;
	return result;
}

ScheduleResult explore(uint32_t schedules, uint64_t baseSeed, const ScenarioFactory& scenario, const std::function<void()>& check,
	const Options& optionsTemplate) {
	ScheduleResult last;
	uint32_t calibratedSteps = 0;
	for (uint32_t i = 0; i < schedules; ++i) {
		Options options = optionsTemplate;
		options.seed = baseSeed + i;
		if (options.maxSteps == 0)
			options.maxSteps = calibratedSteps > 0 ? calibratedSteps : 1000;
		PctScheduler scheduler(options);
		last = scheduler.run(scenario(options.seed));
		calibratedSteps = std::max(calibratedSteps, last.steps);
		if (!last.completed || last.deadlock)
			return last;
		if (check) {
			try {
				check();
			} catch (const std::exception& e) {
				last.completed = false;
				last.failure = std::string("check: ") + e.what();
				return last;
			}
		}
	}
	return last;
}

uint32_t schedulesFromEnvironment(uint32_t defaultSchedules) {
	const char* value = std::getenv("AION_PCT_SCHEDULES"); // test-only helper (the runtime getenv ban applies to game code)
	if (value == nullptr || *value == '\0')
		return defaultSchedules;
	return static_cast<uint32_t>(std::strtoul(value, nullptr, 10));
}

int32_t controlledThreadIndex() noexcept {
	return currentSchedule != nullptr ? currentIndex : -1;
}

} // namespace aion::gameserver::runtime::pct
