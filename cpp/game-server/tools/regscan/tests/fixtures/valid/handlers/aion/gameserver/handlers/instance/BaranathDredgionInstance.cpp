#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "regscan_fixture/FakeCore.h"

namespace aion::gameserver::handlers::instance {

class BaranathDredgionInstance final : public gameserver::instance::handlers::InstanceHandler {
	AION_MAKE_REF_FRIEND

public:
	static runtime::Ref<BaranathDredgionInstance> create(world::WorldMapInstance& instance) { return runtime::makeRef<BaranathDredgionInstance>(instance); }
	int32_t getMapId() const override { return mapId; }

protected:
	explicit BaranathDredgionInstance(world::WorldMapInstance& instance) : mapId(instance.mapId) {}
	~BaranathDredgionInstance() override = default;

private:
	const int32_t mapId;
};
AION_INSTANCE_HANDLER(BaranathDredgionInstance, 300110000);

} // namespace aion::gameserver::handlers::instance
