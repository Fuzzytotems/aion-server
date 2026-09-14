#pragma once

#include "regscan_fixture/FakeCore.h"

namespace aion::gameserver::handlers::zone::pvpZones {

/** abstract handler base class (not registered) */
class PvPZone : public world::zone::handler::ZoneHandler {
public:
	virtual const char* arena() const = 0;

protected:
	PvPZone() = default;
	~PvPZone() override = default;
};

} // namespace aion::gameserver::handlers::zone::pvpZones
