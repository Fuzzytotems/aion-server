#include "aion/gameserver/skillengine/model/ChainSkill.h"

#include "aion/commons/utils/TimeUtils.h"

namespace aion::gameserver::skillengine::model {

ChainSkill::ChainSkill(std::string_view categoryValue) : category(std::string(categoryValue)) {
}

ChainSkill::~ChainSkill() = default;

runtime::Ref<ChainSkill> ChainSkill::create(std::string_view categoryValue) {
	return runtime::makeRef<ChainSkill>(categoryValue);
}

void ChainSkill::clear() {
	this->category.set("");
	this->useCount.set(0);
	this->lastUseTime.set(0);
}

void ChainSkill::increaseUseCount() {
	// java-race: Java's `useCount++` is an unsynchronized read-modify-write; ChainSkills belongs to one player, whose casts run one at a time
	this->useCount.set(this->useCount.get() + 1);
	this->lastUseTime.set(commons::utils::currentTimeMillis());
}

} // namespace aion::gameserver::skillengine::model
