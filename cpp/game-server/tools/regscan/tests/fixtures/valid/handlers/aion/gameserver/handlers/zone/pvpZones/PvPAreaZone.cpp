#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/handlers/zone/pvpZones/PvPZone.h"

namespace aion::gameserver::handlers::zone::pvpZones {

class PvPAreaZone final : public PvPZone {
	AION_MAKE_REF_FRIEND

public:
	static runtime::Ref<PvPAreaZone> create() { return runtime::makeRef<PvPAreaZone>(); }
	const char* arena() const override { return "area"; }

protected:
	PvPAreaZone() = default;
	~PvPAreaZone() override = default;
};
AION_ZONE_HANDLER(PvPAreaZone, "LC1_PVP DC1_PVP");

} // namespace aion::gameserver::handlers::zone::pvpZones
