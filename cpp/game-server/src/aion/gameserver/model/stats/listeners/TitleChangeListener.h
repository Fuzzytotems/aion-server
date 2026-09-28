#pragma once

#include <cstdint>

#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/model/stats/listeners/fwd.h"

namespace aion::gameserver::model::stats::listeners {

/**
 * Adds or removes the stat functions of a bonus title.
 * <p>
 * C++: a static-only class.
 *
 * @author xavier
 */
class TitleChangeListener {
public:
	TitleChangeListener() = delete;

	static void onBonusTitleChange(container::CreatureGameStats& cgs, int32_t titleId, bool isSet);
};

} // namespace aion::gameserver::model::stats::listeners
