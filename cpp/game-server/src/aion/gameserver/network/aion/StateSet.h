#pragma once

#include <cstdint>
#include <initializer_list>

#include "aion/gameserver/network/aion/AionConnection_State.h"

namespace aion::gameserver::network::aion {

/**
 * Java: `Set<AionConnection.State>` (an EnumSet built by AionClientPacketFactory's PacketInfo) of the connection states in which a client packet
 * is accepted. C++ only as a class: HandlerRegistry.h forward-declares it for ClientPacketFactory, and the X-macro list
 * ClientPacketInfo.gen.inc names the states of each packet. An immutable bit set over the three states.
 * <p>
 * Thread-safety: a value type; safe to share once constructed.
 */
class StateSet final {
public:
	using State = AionConnection_State;

	constexpr StateSet() noexcept = default;

	/** Java: EnumSet.of(states...) */
	constexpr StateSet(std::initializer_list<State> states) noexcept {
		for (State state : states)
			bits |= bitOf(state);
	}

	/** Java: Set.contains */
	constexpr bool contains(State state) const noexcept { return (bits & bitOf(state)) != 0; }

	/** Java: Set.isEmpty */
	constexpr bool isEmpty() const noexcept { return bits == 0; }

	/** Java: Set.size */
	constexpr int32_t size() const noexcept { return ((bits & 1u) != 0) + ((bits & 2u) != 0) + ((bits & 4u) != 0); }

	friend constexpr bool operator==(const StateSet&, const StateSet&) noexcept = default;

private:
	static constexpr uint32_t bitOf(State state) noexcept { return uint32_t{1} << static_cast<uint32_t>(state); }

	uint32_t bits = 0;
};

} // namespace aion::gameserver::network::aion
