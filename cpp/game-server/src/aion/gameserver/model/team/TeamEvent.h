#pragma once

#include <string>

#include "aion/gameserver/model/team/fwd.h"

namespace aion::gameserver::model::team {

/**
 * C++: a plain interface. Every team event is K5 (fieldmap.toml [kinds] for the leave events, inferred for the others): GeneralTeam::onEvent
 * receives it by reference and the events are created on the stack by their callers (runtime-architecture.md §14.2(d)).
 *
 * @author ATracer
 */
class TeamEvent {
public:
	virtual void handleEvent() = 0;

	virtual bool checkCondition() = 0;

	/** C++ only: Java's Object.toString() of the event in the "[TEAM] skipped event" warning (the simple class name, without the hash) */
	virtual std::string toString();

	virtual ~TeamEvent() = default;

protected:
	TeamEvent() = default;
	TeamEvent(const TeamEvent&) = default;
	TeamEvent& operator=(const TeamEvent&) = default;
};

} // namespace aion::gameserver::model::team
