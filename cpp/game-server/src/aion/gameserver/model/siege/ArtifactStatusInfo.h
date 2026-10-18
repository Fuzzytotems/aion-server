#pragma once

#include <cstdint>

#include "aion/gameserver/model/siege/ArtifactStatus.h"

namespace aion::gameserver::model::siege {

/**
 * Companion of the generated enum ArtifactStatus (docs/design/static-data.md §2.5, ArtifactStatus.java): Java's constructor data as a free
 * function found by ADL (`getValue(status)` for Java `status.getValue()`). Pure data, so it is ported as a whole.
 */

/** Java: ArtifactStatus.getValue() - the constructor's id: IDLE(0), ACTIVATION(1), CASTING(2), ACTIVATED(3) */
constexpr int32_t getValue(ArtifactStatus status) noexcept {
	switch (status) {
		case ArtifactStatus::IDLE:
			return 0;
		case ArtifactStatus::ACTIVATION:
			return 1;
		case ArtifactStatus::CASTING:
			return 2;
		case ArtifactStatus::ACTIVATED:
			return 3;
	}
	return 0;
}

} // namespace aion::gameserver::model::siege
