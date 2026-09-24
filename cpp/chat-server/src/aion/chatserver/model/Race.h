#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace aion::chatserver::model {

/**
 * The two playable races. A race received from the game server or a client may be unknown, which Java represents as null; the C++ code uses
 * std::optional&lt;Race&gt; for such values.
 * <p>
 * Java: com.aionemu.chatserver.model.Race
 *
 * @author ATracer
 */
enum class Race : int8_t {
	ELYOS,
	ASMODIANS
};

/** Java: getRaceId() */
constexpr int32_t getRaceId(Race race) noexcept {
	return race == Race::ELYOS ? 0 : 1;
}

/** Java: Race.getById(id) - ELYOS for 0, ASMODIANS for 1, null (std::nullopt) for any other id */
constexpr std::optional<Race> getById(int32_t id) noexcept {
	if (id == getRaceId(Race::ELYOS))
		return Race::ELYOS;
	if (id == getRaceId(Race::ASMODIANS))
		return Race::ASMODIANS;
	return std::nullopt;
}

/** Java: name() */
constexpr std::string_view name(Race race) noexcept {
	return race == Race::ELYOS ? "ELYOS" : "ASMODIANS";
}

/** Java: String.valueOf(race) - the name, or "null" */
inline std::string toString(std::optional<Race> race) {
	return race ? std::string(name(*race)) : std::string("null");
}

} // namespace aion::chatserver::model
