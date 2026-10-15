#pragma once

#include <cstdint>

#include "aion/gameserver/custom/instance/fwd.h"

namespace aion::gameserver::custom::instance {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Estrayl
 */
class CustomInstanceRank {
private:
	int32_t playerId{};
	int32_t rank{};
	int32_t maxRank{};
	int32_t dps{};
	int64_t lastEntry{};

public:
	CustomInstanceRank(int32_t playerId, int32_t rank, int64_t lastEntry, int32_t maxRank, int32_t dps);

	int32_t getPlayerId() const { return this->playerId; }

	int32_t getRank() const { return this->rank; }

	void setRank(int32_t value) { this->rank = value; }

	int64_t getLastEntry() const { return this->lastEntry; }

	void setLastEntry(int64_t value) { this->lastEntry = value; }

	int32_t getMaxRank() const { return this->maxRank; }

	void setMaxRank(int32_t value) { this->maxRank = value; }

	int32_t getDps() const { return this->dps; }

	void setDps(int32_t value) { this->dps = value; }
};

} // namespace aion::gameserver::custom::instance
