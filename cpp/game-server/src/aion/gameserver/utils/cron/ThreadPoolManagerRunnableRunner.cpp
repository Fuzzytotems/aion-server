#include "aion/gameserver/utils/cron/ThreadPoolManagerRunnableRunner.h"

#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::utils::cron {

void ThreadPoolManagerRunnableRunner::executeRunnable(const services::cron::CronJob& job) {
	// the copied PinnedCallback keeps the job's Pin while the task is pending; the task itself pins nothing else
	ThreadPoolManager::getInstance().execute(runtime::Pin(), [job] { job(); }, job.getTaskInfo().where);
}

void ThreadPoolManagerRunnableRunner::executeLongRunningRunnable(const services::cron::CronJob& job) {
	ThreadPoolManager::getInstance().executeLongRunning(runtime::Pin(), [job] { job(); }, job.getTaskInfo().where);
}

} // namespace aion::gameserver::utils::cron
