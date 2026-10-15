#pragma once

#include <cstdint>

namespace aion::chatserver::model {

/**
 * Unused in the Java chat server as well (kept so the packages match).
 * <p>
 * Java: com.aionemu.chatserver.model.RestartFrequency
 *
 * @author nrg
 */
enum class RestartFrequency : int8_t {
	DAILY,
	WEEKLY,
	MONTHLY
};

} // namespace aion::chatserver::model
