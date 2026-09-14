#pragma once

#include <cstdint>

#include "aion/gameserver/services/abyss/fwd.h"

namespace aion::gameserver::services::abyss {

/**
 * C++: a static-only class (hub-headers.md §11.1).
 *
 * @author ViAl, Sykra
 */
class GloryPointsService {
private:
	GloryPointsService() = delete;
public:
	static void addGp(int32_t playerObjId, int32_t amount);
};

} // namespace aion::gameserver::services::abyss
