#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/model/guide/fwd.h"

namespace aion::gameserver::model::guide {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author xTz
 */
class Guide {
private:
	int32_t guide_id{};
	int32_t player_id{};
	std::string title{};

public:
	Guide(int32_t guide_id, int32_t player_id, std::string_view title);

	int32_t getGuideId() const { return this->guide_id; }

	int32_t getPlayerId() const { return this->player_id; }

	std::string getTitle() const { return this->title; }
};

} // namespace aion::gameserver::model::guide
