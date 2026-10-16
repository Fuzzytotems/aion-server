#include <cstdint>
#include <string>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

/* AION_AI(CommentedOutAI, "commented"); is not a marker */
namespace aion::gameserver::handlers::ai {

class AggressiveNpcAI final : public GeneralNpcAI {
public:
	using GeneralNpcAI::GeneralNpcAI;
	std::string describe() const override { return "aggressive:" + getOwner().name + " " + std::to_string(spawnHelpers()); }

private:
	static constexpr const char* NOT_A_MARKER = "AION_AI(StringAI, \"string\");";

	static int32_t spawn(int32_t npcId) { return npcId; }
	static int32_t sp(int32_t npcId, float delay) { return delay > 0 ? npcId : 0; }

	int32_t spawnHelpers() const {
		bool elyos = true;
		// spawn(299999) in a comment counts, like QuestSpawnAnalyzer's scan of the Java source
		return spawn(elyos ? 206001 : 206002) + sp(215074, 1.5f);
	}
};
// AION_AI(AnotherCommentedAI, "commented_too");
AION_AI(AggressiveNpcAI, "aggressive"); // trailing comments are allowed

} // namespace aion::gameserver::handlers::ai
