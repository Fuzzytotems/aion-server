#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>

#include "aion/gameserver/network/sequrity/FloodManager_Result.h"
#include "aion/gameserver/network/sequrity/fwd.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::network::sequrity {

/**
 * Counts events (connection attempts) per key in fixed ticks and rejects or warns about keys over the limits of its filters.
 * <p>
 * C++: RefCounted (fieldmap K4, GameConnectionFactoryImpl.floodAcceptor); the flush task registered with NetFlusher retains it for the rest of
 * the run, like Java's Timer task. The public Java logger `log` is the .cpp logger.
 *
 * @author NB4L1
 */
class FloodManager : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	/** Java: public static final class FloodFilter (fieldmap K3: immutable, held by Ref in _filters) */
	class FloodFilter : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	public:
		const int32_t _warnLimit;
		const int32_t _rejectLimit;
		const int32_t _tickLimit;

		/** Java: new FloodFilter(warnLimit, rejectLimit, tickLimit) */
		static runtime::Ref<FloodFilter> create(int32_t warnLimit, int32_t rejectLimit, int32_t tickLimit);

		int32_t getRejectLimit() const { return _rejectLimit; }

		int32_t getTickLimit() const { return _tickLimit; }

		int32_t getWarnLimit() const { return _warnLimit; }

	protected:
		FloodFilter(int32_t warnLimit, int32_t rejectLimit, int32_t tickLimit);
		~FloodFilter() override;
	};

	/** Java: private final class LogEntry (inner class: the enclosing FloodManager is its captured this$0) */
	class LogEntry;

	using Result = FloodManager_Result;

private:
	/** Java: System.currentTimeMillis() - TimeUnit.DAYS.toMillis(1) at class initialization */
	static const int64_t ZERO;

	runtime::HashMap<std::string, runtime::Ref<FloodManager::LogEntry>> _entries{AION_LOCK_CLASS(FloodManager::_entries)};
	runtime::Monitor _lock{AION_LOCK_CLASS(FloodManager::_lock)};
	const int32_t _tickLength;
	const int32_t _tickAmount;
	const runtime::Ref<runtime::Array<runtime::Ref<FloodManager::FloodFilter>>> _filters;

protected:
	FloodManager(int32_t msecPerTick, std::initializer_list<runtime::Ptr<FloodFilter>> filters);
	~FloodManager() override;

public:
	/** Java: new FloodManager(msecPerTick, filters...) */
	static runtime::Ref<FloodManager> create(int32_t msecPerTick, std::initializer_list<runtime::Ptr<FloodFilter>> filters = {});

private:
	void flush();

public:
	/** @param key Java null or "" is rejected */
	Result isFlooding(std::string_view key, bool increment);
};

/** Java: private final class FloodManager.LogEntry (defined here: FloodManager's members need the complete FloodFilter first) */
class FloodManager::LogEntry : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	const runtime::Ref<runtime::Array<int16_t>> _ticks;
	runtime::Field<int32_t> _lastTick{};
	// captured variables:
	const runtime::Ref<FloodManager> floodManager;

	/** Java: new LogEntry() inside FloodManager */
	static runtime::Ref<LogEntry> create(FloodManager& floodManager);

	int32_t getCurrentTick();

	bool isActive();

	Result isFlooding(bool increment);

protected:
	explicit LogEntry(FloodManager& floodManager);
	~LogEntry() override;
};

} // namespace aion::gameserver::network::sequrity
