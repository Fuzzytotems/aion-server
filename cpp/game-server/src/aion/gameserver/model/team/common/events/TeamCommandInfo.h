#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "aion/gameserver/model/team/common/events/TeamCommand.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::team::common::events {

/** Companion of the generated enum TeamCommand (docs/design/static-data.md §2.5). */

/** Java: TeamCommand.getCodeId() in ordinal order */
constexpr int32_t getCodeId(TeamCommand command) noexcept {
	constexpr int32_t CODES[] = {2, 3, 6, 9, 10, 11, 14, 16, 17, 20, 21, 22, 23, 24, 25, 26, 27, 29, 30, 31, 32};
	return CODES[static_cast<size_t>(command)];
}

/**
 * Java: TeamCommand.getCommand(commandCode) - the command of a code; an unknown code is Objects.requireNonNull's NullPointerException
 * ("Invalid team command code " + code, TeamCommand.java:52-56)
 */
inline TeamCommand getCommand(int32_t commandCode) {
	for (size_t i = 0; i <= static_cast<size_t>(TeamCommand::LEAGUE_SET_LEADER); ++i) {
		const auto command = static_cast<TeamCommand>(i);
		if (getCodeId(command) == commandCode)
			return command;
	}
	throw runtime::NullPointerException("Invalid team command code " + std::to_string(commandCode));
}

} // namespace aion::gameserver::model::team::common::events
