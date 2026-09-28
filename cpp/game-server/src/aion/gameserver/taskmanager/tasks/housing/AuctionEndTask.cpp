#include "aion/gameserver/taskmanager/tasks/housing/AuctionEndTask.h"

#include <algorithm>
#include <optional>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/HousingConfig.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/services/HousingBidService.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::taskmanager::tasks::housing {

// Defined here (hub-headers.md §9.3): only the AuctionEndTask bodies use it. The Java inner class reads no outer state.
class AuctionEndTask::ProlongedAuction final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	const int32_t houseObjectId;
	runtime::Field<int64_t> auctionEndMillis{};
	runtime::Field<runtime::FutureRef> task{};

protected:
	ProlongedAuction(int32_t houseObjectIdValue, int64_t delayMillis) : houseObjectId(houseObjectIdValue) {
		prolong(delayMillis);
	}

	~ProlongedAuction() override = default;

public:
	static runtime::Ref<ProlongedAuction> create(int32_t houseObjectIdValue, int64_t delayMillis) {
		return runtime::makeRef<ProlongedAuction>(houseObjectIdValue, delayMillis);
	}

	// lambda at AuctionEndTask.java:106 (fieldmap key AuctionEndTask.ProlongedAuction): schedule, pin {this}
	bool prolong(int64_t delayMillis) {
		runtime::FutureRef currentTask = task.get();
		if (currentTask && !currentTask->cancel(false))
			return false;
		auctionEndMillis.set(commons::utils::currentTimeMillis() + delayMillis);
		task.set(utils::ThreadPoolManager::getInstance().schedule({this}, [this] { services::HousingBidService::getInstance().endAuction(houseObjectId); },
			delayMillis));
		return true;
	}
};

AuctionEndTask::AuctionEndTask()
	: AbstractCronTask(configs::main::HousingConfig::HOUSE_AUCTION_END_TIME.load(), "com.aionemu.gameserver.taskmanager.tasks.housing.AuctionEndTask") {
}

AuctionEndTask::~AuctionEndTask() = default;

AuctionEndTask& AuctionEndTask::getInstance() {
	// Java: private static final AuctionEndTask instance = new AuctionEndTask() - one Ref that is never released; postConstruct() is the tail of
	// the Java constructor (AbstractCronTask class comment)
	static const runtime::Ref<AuctionEndTask>* const instance = [] {
		auto* created = new runtime::Ref<AuctionEndTask>(runtime::makeRef<AuctionEndTask>());
		(*created)->postConstruct();
		return created;
	}();
	return **instance;
}

bool AuctionEndTask::shouldRunOnStart() {
	if (AbstractCronTask::shouldRunOnStart()) // true if the server was down when auctions should have ended (SERVER_STOP_MILLIS < lastPlannedRun)
		return true;
	std::optional<int64_t> serverStop = serverStopMillis();
	if (serverStop) { // trigger auction end if the server shut down in the prolongation time frame (30min after regular auction end)
		std::optional<commons::database::Timestamp> lastPlannedRun = getLastPlannedRun();
		if (!lastPlannedRun) // Java: NullPointerException on getTime()
			throw runtime::NullPointerException("Cannot invoke \"java.util.Date.getTime()\" because the last planned run is null");
		return *serverStop - lastPlannedRun->time_since_epoch().count() <= MAX_PROLONGATION_MILLIS;
	}
	return false;
}

void AuctionEndTask::executeTask() {
	// Java: HousingBidService.getInstance().endAuctions()
	AION_PARTIAL("house auction end is not ported yet (HousingBidService.endAuctions, M5a E2-05)");
}

int32_t AuctionEndTask::getRemainingAuctionSeconds(int32_t houseObjectId) {
	runtime::Ptr<ProlongedAuction> prolongedAuction = prolongedAuctions.get(houseObjectId);
	int64_t auctionEndMillis = 0;
	if (!prolongedAuction) {
		std::optional<commons::database::Timestamp> next = getNextRun();
		if (!next) // Java: NullPointerException on getTime()
			throw runtime::NullPointerException("Cannot invoke \"java.util.Date.getTime()\" because the next run is null");
		auctionEndMillis = next->time_since_epoch().count();
	} else {
		auctionEndMillis = prolongedAuction->auctionEndMillis.get();
	}
	return static_cast<int32_t>((auctionEndMillis - commons::utils::currentTimeMillis()) / 1000);
}

void AuctionEndTask::onAuctionEnd(int32_t houseObjectId) {
	runtime::Ptr<ProlongedAuction> prolongedAuction = prolongedAuctions.remove(houseObjectId);
	if (prolongedAuction) {
		if (runtime::FutureRef prolongedTask = prolongedAuction->task.get())
			prolongedTask->cancel(false);
	}
}

bool AuctionEndTask::tryProlongAuction(int32_t houseObjectId) {
	int64_t millisUntilAuctionEnd = getMillisUntilNextRun();
	int64_t millisSinceLastAuctionEnd = getMillisSinceLastRun();
	int64_t delayMillis = 0;
	if (millisUntilAuctionEnd <= 5 * 60 * 1000) // initial extension is 5 minutes after regular auction end
		delayMillis = millisUntilAuctionEnd + PROLONGATION_MILLIS;
	else if (millisSinceLastAuctionEnd != -1 && millisSinceLastAuctionEnd < MAX_PROLONGATION_MILLIS) // max extension is 30 minutes
		delayMillis = std::min(int64_t{30 * 60 * 1000} - millisSinceLastAuctionEnd, PROLONGATION_MILLIS);
	return delayMillis == 0 || prolongAuction(houseObjectId, delayMillis);
}

bool AuctionEndTask::isAuctionProlonged(int32_t houseObjectId) {
	return prolongedAuctions.containsKey(houseObjectId);
}

bool AuctionEndTask::prolongAuction(int32_t houseObjectId, int64_t delayMillis) {
	runtime::Ptr<ProlongedAuction> prolongedAuction =
		prolongedAuctions.compute(houseObjectId, [houseObjectId, delayMillis](runtime::Ptr<ProlongedAuction> oldValue) -> runtime::Ref<ProlongedAuction> {
			if (!oldValue)
				return ProlongedAuction::create(houseObjectId, delayMillis);
			else if (!oldValue->prolong(delayMillis))
				return nullptr;
			return runtime::Ref<ProlongedAuction>(oldValue);
		});
	return static_cast<bool>(prolongedAuction);
}

} // namespace aion::gameserver::taskmanager::tasks::housing
