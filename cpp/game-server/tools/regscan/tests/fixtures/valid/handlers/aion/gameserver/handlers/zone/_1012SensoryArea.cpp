#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "regscan_fixture/FakeCore.h"

namespace aion::gameserver::handlers::zone {

class _1012SensoryArea final : public world::zone::handler::QuestZoneHandler {
	AION_MAKE_REF_FRIEND

public:
	static runtime::Ref<_1012SensoryArea> create(int32_t questId) { return runtime::makeRef<_1012SensoryArea>(questId); }

protected:
	explicit _1012SensoryArea(int32_t questId) : QuestZoneHandler(questId) {}
	~_1012SensoryArea() override = default;
};
AION_ZONE_HANDLER(_1012SensoryArea, "LF1A_A LF1A_B LF1A_C", 1012);

} // namespace aion::gameserver::handlers::zone
