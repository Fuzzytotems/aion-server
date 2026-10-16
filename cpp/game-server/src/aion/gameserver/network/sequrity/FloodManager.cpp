#include "aion/gameserver/network/sequrity/FloodManager.h"

#include <algorithm>
#include <chrono>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/network/sequrity/NetFlusher.h"

namespace aion::gameserver::network::sequrity {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.sequrity.FloodManager");

const int64_t FloodManager::ZERO = commons::utils::currentTimeMillis() - std::chrono::milliseconds(std::chrono::days(1)).count();

// ------------------------------------------------------------------------------------------------------------------------------ FloodFilter

FloodManager::FloodFilter::FloodFilter(int32_t warnLimit, int32_t rejectLimit, int32_t tickLimit)
	: _warnLimit(warnLimit), _rejectLimit(rejectLimit), _tickLimit(tickLimit) {
}

FloodManager::FloodFilter::~FloodFilter() = default;

runtime::Ref<FloodManager::FloodFilter> FloodManager::FloodFilter::create(int32_t warnLimit, int32_t rejectLimit, int32_t tickLimit) {
	return runtime::makeRef<FloodFilter>(warnLimit, rejectLimit, tickLimit);
}

// --------------------------------------------------------------------------------------------------------------------------------- LogEntry

FloodManager::LogEntry::LogEntry(FloodManager& floodManagerValue)
	: _ticks(runtime::Array<int16_t>::make(floodManagerValue._tickAmount)), floodManager(floodManagerValue) {
	_lastTick.set(getCurrentTick());
}

FloodManager::LogEntry::~LogEntry() = default;

runtime::Ref<FloodManager::LogEntry> FloodManager::LogEntry::create(FloodManager& floodManagerValue) {
	return runtime::makeRef<LogEntry>(floodManagerValue);
}

int32_t FloodManager::LogEntry::getCurrentTick() {
	return static_cast<int32_t>((commons::utils::currentTimeMillis() - ZERO) / floodManager->_tickLength);
}

bool FloodManager::LogEntry::isActive() {
	return getCurrentTick() - _lastTick.get() < floodManager->_tickAmount * 10;
}

FloodManager::Result FloodManager::LogEntry::isFlooding(bool increment) {
	const int32_t currentTick = getCurrentTick();
	const int32_t length = _ticks->length();

	if (currentTick - _lastTick.get() >= length) {
		_lastTick.set(currentTick);
		for (int32_t i = 0; i < length; i++)
			(*_ticks)[i] = int16_t{0};
	} else if (_lastTick.get() > currentTick) {
		log.warn("The current tick (" + std::to_string(currentTick) + ") is smaller than the last (" + std::to_string(_lastTick.get()) + ")!",
			commons::utils::IllegalStateException(""));
		_lastTick.set(currentTick);
	} else
		while (currentTick != _lastTick.get()) {
			++_lastTick;
			(*_ticks)[_lastTick.get() % length] = int16_t{0};
		}

	if (increment)
		++(*_ticks)[_lastTick.get() % length];

	for (runtime::Ptr<FloodFilter> filter : *floodManager->_filters) {
		int32_t previousSum = 0;
		int32_t currentSum = 0;

		for (int32_t i = 0; i <= filter->getTickLimit(); i++) {
			int32_t value = _ticks->get((_lastTick.get() - i) % length);

			if (i != 0)
				previousSum += value;

			if (i != filter->getTickLimit())
				currentSum += value;
		}

		if (previousSum > filter->getRejectLimit() || currentSum > filter->getRejectLimit())
			return Result::REJECTED;

		if (previousSum > filter->getWarnLimit() || currentSum > filter->getWarnLimit())
			return Result::WARNED;
	}

	return Result::ACCEPTED;
}

// ----------------------------------------------------------------------------------------------------------------------------- FloodManager

namespace {

int32_t tickAmountOf(std::initializer_list<runtime::Ptr<FloodManager::FloodFilter>> filters) {
	int32_t max = 1;

	for (runtime::Ptr<FloodManager::FloodFilter> filter : filters)
		max = std::max(filter->getTickLimit() + 1, max);

	return max;
}

runtime::Ref<runtime::Array<runtime::Ref<FloodManager::FloodFilter>>> filterArrayOf(
	std::initializer_list<runtime::Ptr<FloodManager::FloodFilter>> filters) {
	auto array = runtime::Array<runtime::Ref<FloodManager::FloodFilter>>::make(static_cast<int32_t>(filters.size()));
	int32_t index = 0;
	for (runtime::Ptr<FloodManager::FloodFilter> filter : filters)
		(*array)[index++] = filter;
	return array;
}

} // namespace

FloodManager::FloodManager(int32_t msecPerTick, std::initializer_list<runtime::Ptr<FloodFilter>> filters)
	: _tickLength(msecPerTick), _tickAmount(tickAmountOf(filters)), _filters(filterArrayOf(filters)) {
	// Java: the anonymous Runnable (FloodManager$1) capturing this; the pin retains the FloodManager for as long as the flush task exists
	NetFlusher::add(runtime::PinnedCallback<void()>(runtime::Pin(this), [this] { flush(); }), 60000);
}

FloodManager::~FloodManager() = default;

runtime::Ref<FloodManager> FloodManager::create(int32_t msecPerTick, std::initializer_list<runtime::Ptr<FloodFilter>> filters) {
	return runtime::makeRef<FloodManager>(msecPerTick, filters);
}

void FloodManager::flush() {
	_lock.lock();
	try {
		for (auto it = _entries.values().iterator(); it.hasNext();) {
			if (it.next()->isActive())
				continue;

			it.remove();
		}
	} catch (...) {
		_lock.unlock();
		throw;
	}
	_lock.unlock();
}

FloodManager::Result FloodManager::isFlooding(std::string_view key, bool increment) {
	if (key.empty())
		return Result::REJECTED;

	_lock.lock();
	try {
		const std::string entryKey(key);
		runtime::Ptr<LogEntry> entry = _entries.get(entryKey);

		if (!entry) {
			runtime::Ref<LogEntry> created = LogEntry::create(*this);
			entry = created;

			_entries.put(entryKey, created);
		}

		Result result = entry->isFlooding(increment);
		_lock.unlock();
		return result;
	} catch (...) {
		_lock.unlock();
		throw;
	}
}

} // namespace aion::gameserver::network::sequrity
