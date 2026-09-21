#include "aion/gameserver/taskmanager/AbstractCronTask.h"

#include <chrono>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/dao/ServerVariablesDAO.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/cron/CronExpression.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::taskmanager {

namespace {

using commons::database::Timestamp;

/** the simple name of a Java class name */
std::string simpleNameOf(std::string_view className) {
	size_t dot = className.rfind('.');
	return std::string(dot == std::string_view::npos ? className : className.substr(dot + 1));
}

Timestamp now() {
	return Timestamp(std::chrono::milliseconds(commons::utils::currentTimeMillis()));
}

/** Quartz CronExpression.getTimeAfter(Date): the next fire time after the date's second, null if there is none */
std::optional<Timestamp> timeAfter(const services::cron::CronExpression& expression, Timestamp date) {
	std::optional<std::chrono::sys_seconds> next =
		expression.getTimeAfter(std::chrono::floor<std::chrono::seconds>(date), configs::main::GSConfig::TIME_ZONE_ID.load());
	if (!next)
		return std::nullopt;
	return std::chrono::time_point_cast<std::chrono::milliseconds>(*next);
}

/** the exception Java throws when it dereferences a null Date */
Timestamp required(const std::optional<Timestamp>& date) {
	if (!date)
		throw runtime::NullPointerException("Cron expression has no next fire time");
	return *date;
}

} // namespace

runtime::Semaphore AbstractCronTask::semaphore{AION_LOCK_CLASS(AbstractCronTask::semaphore), 1};

std::optional<int64_t> AbstractCronTask::serverStopMillis() {
	// Java class initialization: loaded once, before the first cron task is created
	static const std::optional<int64_t> value = dao::ServerVariablesDAO::loadLong("serverLastRun");
	return value;
}

AbstractCronTask::AbstractCronTask(const services::cron::CronExpression* cronExpression, std::string_view className)
	: log(commons::logging::LoggerFactory::getLogger(className)), simpleClassName(simpleNameOf(className)), cronExpression(cronExpression),
	  lastPlannedRunBeforeServerStart(initialLastPlannedRun()) {
	if (this->cronExpression == nullptr) {
		log.info(simpleClassName + " is deactivated");
		return;
	}
	// Java computes nextRun first and lastPlannedRunBeforeServerStart from it; initialLastPlannedRun() did the same computation with the same "now"
	// second unless the clock crossed a fire time in between (then Java's own order would see the same effect)
	nextRun.set(getNextRunAfter(now()));
}

AbstractCronTask::~AbstractCronTask() = default;

std::optional<Timestamp> AbstractCronTask::initialLastPlannedRun() const {
	if (cronExpression == nullptr)
		return std::nullopt;
	return findLastPlannedRun(required(timeAfter(*cronExpression, now())));
}

void AbstractCronTask::postConstruct() {
	if (cronExpression == nullptr)
		return;
	runAndScheduleAsyncWithLock();
}

// lambda at AbstractCronTask.java:44 (fieldmap key AbstractCronTask@L44): executeLongRunning, pin {this}
void AbstractCronTask::runAndScheduleAsyncWithLock() {
	semaphore.acquireUninterruptibly();
	utils::ThreadPoolManager::getInstance().executeLongRunning({this}, [this] {
		struct ReleaseSemaphore { // C++ fix: released even if the start run or the scheduling throws (class comment)
			~ReleaseSemaphore() { semaphore.release(); }
		} releaseSemaphore;
		if (shouldRunOnStart())
			run();
		services::cron::CronService::getInstance().schedule(services::cron::CronJob({this}, [this] { run(); }), *cronExpression, true);
		log.info("Scheduled " + simpleClassName + " with cron expression: " + cronExpression->toString());
	});
}

bool AbstractCronTask::shouldRunOnStart() {
	std::optional<int64_t> serverStop = serverStopMillis();
	return serverStop && lastPlannedRunBeforeServerStart && *serverStop < lastPlannedRunBeforeServerStart->time_since_epoch().count();
}

std::optional<Timestamp> AbstractCronTask::getLastRun() const {
	return lastRun.get();
}

std::optional<Timestamp> AbstractCronTask::getLastPlannedRun() const {
	std::optional<Timestamp> last = lastRun.get();
	return !last ? lastPlannedRunBeforeServerStart : last;
}

int64_t AbstractCronTask::getMillisSinceLastRun() const {
	std::optional<Timestamp> last = lastRun.get();
	return !last ? -1 : commons::utils::currentTimeMillis() - last->time_since_epoch().count();
}

std::optional<Timestamp> AbstractCronTask::getNextRun() const {
	return nextRun.get();
}

std::optional<Timestamp> AbstractCronTask::getNextRunAfter(Timestamp date) const {
	if (cronExpression == nullptr)
		throw runtime::NullPointerException("Cron expression of " + simpleClassName + " is null");
	return timeAfter(*cronExpression, date);
}

int64_t AbstractCronTask::getMillisUntilNextRun() const {
	return required(nextRun.get()).time_since_epoch().count() - commons::utils::currentTimeMillis();
}

void AbstractCronTask::run() {
	Timestamp started = now();
	lastRun.set(started);
	nextRun.set(getNextRunAfter(started));
	executeTask();
}

std::optional<Timestamp> AbstractCronTask::findLastPlannedRun(Timestamp nextRunValue) const {
	int64_t interval = (required(getNextRunAfter(nextRunValue)) - nextRunValue).count();
	int64_t nowMillis = commons::utils::currentTimeMillis();
	int64_t millis = nowMillis;
	Timestamp last;
	do {
		millis -= interval / 2;
		last = required(timeAfter(*cronExpression, Timestamp(std::chrono::milliseconds(millis))));
	} while (last.time_since_epoch().count() >= nowMillis);
	return last;
}

} // namespace aion::gameserver::taskmanager
