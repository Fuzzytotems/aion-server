#include "aion/gameserver/skillengine/model/ChainSkill.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::model {

ChainSkill::ChainSkill(std::string_view categoryValue) : category(std::string(categoryValue)) {
}

ChainSkill::~ChainSkill() = default;

runtime::Ref<ChainSkill> ChainSkill::create(std::string_view categoryValue) {
	return runtime::makeRef<ChainSkill>(categoryValue);
}

void ChainSkill::clear() {
	AION_UNPORTED();
}

void ChainSkill::increaseUseCount() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::model
