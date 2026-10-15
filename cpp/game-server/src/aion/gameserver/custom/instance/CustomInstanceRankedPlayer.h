#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/custom/instance/CustomInstanceRank.h"
#include "aion/gameserver/custom/instance/fwd.h"
#include "aion/gameserver/model/fwd.h"

namespace aion::gameserver::custom::instance {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Neon
 */
class CustomInstanceRankedPlayer : public CustomInstanceRank {
private:
	std::string name{};
	model::PlayerClass playerClass{};

public:
	CustomInstanceRankedPlayer(int32_t playerId, int32_t rank, int64_t lastEntry, int32_t maxRank, int32_t dps, std::string_view name,
		model::PlayerClass playerClass);

	std::string getName() const { return this->name; }

	model::PlayerClass getPlayerClass() const { return this->playerClass; }
};

} // namespace aion::gameserver::custom::instance
