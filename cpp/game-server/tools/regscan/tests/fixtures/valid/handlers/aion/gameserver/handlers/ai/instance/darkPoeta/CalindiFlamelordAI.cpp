#include <cstdint>
#include <string>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::darkPoeta {

// same simple name as in the other instance directory: namespaces keep them apart
class CalindiFlamelordAI final : public GeneralNpcAI {
public:
	using GeneralNpcAI::GeneralNpcAI;
	std::string describe() const override { return "calindi_flamelord:" + getOwner().name; }

private:
	static int32_t spawn(int32_t npcId, float x) { return x > 0 ? npcId : 0; }
	int32_t summonAdds() const { return spawn(700001, 1.0f); }
};
AION_AI(CalindiFlamelordAI, "calindi_flamelord");

} // namespace aion::gameserver::handlers::ai::instance::darkPoeta
