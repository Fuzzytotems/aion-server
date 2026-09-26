#include <cstdint>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "regscan_fixture/FakeCore.h"

// directory "template" is a C++ keyword: the namespace segment gets a trailing underscore
namespace aion::gameserver::handlers::quest::template_ {

class _9001KeywordPackage final : public questEngine::handlers::AbstractQuestHandler {
public:
	_9001KeywordPackage() : AbstractQuestHandler(9001) {}

private:
	static int32_t spawn(int32_t npcId) { return npcId; }
	int32_t onStart() const { return spawn(900001); }
};
AION_QUEST_HANDLER(_9001KeywordPackage, 9001);

} // namespace aion::gameserver::handlers::quest::template_
