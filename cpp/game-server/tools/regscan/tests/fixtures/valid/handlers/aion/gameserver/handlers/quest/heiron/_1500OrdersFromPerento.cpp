#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "regscan_fixture/FakeCore.h"

namespace aion::gameserver::handlers::quest::heiron {

class _1500OrdersFromPerento final : public questEngine::handlers::AbstractQuestHandler {
public:
	_1500OrdersFromPerento() : AbstractQuestHandler(1500) {}
};
AION_QUEST_HANDLER(_1500OrdersFromPerento, 1500);

} // namespace aion::gameserver::handlers::quest::heiron
