#pragma once

#include "aion/gameserver/services/cron/CronService.h"

namespace aion::gameserver::utils::cron {

/**
 * Java: com.aionemu.gameserver.utils.cron.ThreadPoolManagerRunnableRunner - runs fired cron jobs on the instant pool (execute) or the
 * long-running pool (executeLongRunning). The job's PinnedCallback is copied into the task, so its Pin stays retained while the task is pending.
 */
class ThreadPoolManagerRunnableRunner final : public services::cron::RunnableRunner {
public:
	void executeRunnable(const services::cron::CronJob& job) override;
	void executeLongRunningRunnable(const services::cron::CronJob& job) override;
};

} // namespace aion::gameserver::utils::cron
