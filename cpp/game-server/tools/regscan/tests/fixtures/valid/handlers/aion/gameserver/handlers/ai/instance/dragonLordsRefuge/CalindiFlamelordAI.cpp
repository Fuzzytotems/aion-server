#include <cstdint>
#include <string>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

// same simple name as in the other instance directory: namespaces keep them apart
class CalindiFlamelordAI final : public GeneralNpcAI {
public:
	using GeneralNpcAI::GeneralNpcAI;
	std::string describe() const override { return "calindi_flamelord_drl:" + getOwner().name; }
};
AION_AI(CalindiFlamelordAI, "calindi_flamelord_drl");

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
