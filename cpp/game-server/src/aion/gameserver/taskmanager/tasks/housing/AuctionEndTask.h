#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/services/cron/CronExpression.h"
#include "aion/gameserver/taskmanager/AbstractCronTask.h"
#include "aion/gameserver/taskmanager/tasks/housing/fwd.h"

namespace aion::gameserver::taskmanager::tasks::housing {

/**
 * Handles housing auction end and potential prolongations if there are new bids just before auction end.
 * <p>
 * C++: RefCounted through AbstractCronTask; getInstance() returns the object of a never-released Ref created on first use (Java's static
 * instance), which runs AbstractCronTask::postConstruct() after construction (two-phase construction, AbstractCronTask class comment). The
 * inner class ProlongedAuction is declared here and defined in the .cpp (only the bodies use it).
 *
 * @author Neon
 */
class AuctionEndTask : public AbstractCronTask {
	AION_MAKE_REF_FRIEND
public:
	class ProlongedAuction;

private:
	static constexpr int64_t PROLONGATION_MILLIS = 5 * 60 * 1000;      // Java: TimeUnit.MINUTES.toMillis(5)
	static constexpr int64_t MAX_PROLONGATION_MILLIS = 30 * 60 * 1000; // Java: TimeUnit.MINUTES.toMillis(30)
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<AuctionEndTask::ProlongedAuction>> prolongedAuctions{
		AION_LOCK_CLASS(AuctionEndTask::prolongedAuctions#stripe)}; // Java: = new ConcurrentHashMap<>()

public:
	static AuctionEndTask& getInstance(); // Java singleton

private:
	AuctionEndTask();

protected:
	~AuctionEndTask() override;

	bool shouldRunOnStart() override;

	void executeTask() override;

public:
	int32_t getRemainingAuctionSeconds(int32_t houseObjectId);

	void onAuctionEnd(int32_t houseObjectId);

	/** @return True if the auction did not need to be prolonged or was prolonged successfully. False if the auction just ended. */
	bool tryProlongAuction(int32_t houseObjectId);

	bool isAuctionProlonged(int32_t houseObjectId);

private:
	/** @return True if the auction could be prolonged. False if it just ended. */
	bool prolongAuction(int32_t houseObjectId, int64_t delayMillis);
};

} // namespace aion::gameserver::taskmanager::tasks::housing
