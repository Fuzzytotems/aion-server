#include "aion/gameserver/dataholders/GlobalNpcExclusionData.h"

namespace aion::gameserver::dataholders {

namespace {

template <class T>
const std::unordered_set<T>& orEmpty(const std::optional<std::unordered_set<T>>& set) {
	static const std::unordered_set<T> EMPTY; // Java Collections.emptySet()
	return set ? *set : EMPTY;
}

} // namespace

void GlobalNpcExclusionData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	isEmpty_ = !excludedNpcIds && !excludedNpcNames && !excludedTypes && !excludedTribes && !excludedAbyssTypes;
}

const std::unordered_set<int32_t>& GlobalNpcExclusionData::getNpcIds() const {
	return orEmpty(excludedNpcIds);
}

const std::unordered_set<std::string>& GlobalNpcExclusionData::getNpcNames() const {
	return orEmpty(excludedNpcNames);
}

const std::unordered_set<model::templates::npc::NpcTemplateType>& GlobalNpcExclusionData::getNpcTemplateTypes() const {
	return orEmpty(excludedTypes);
}

const std::unordered_set<model::TribeClass>& GlobalNpcExclusionData::getNpcTribes() const {
	return orEmpty(excludedTribes);
}

const std::unordered_set<model::templates::npc::AbyssNpcType>& GlobalNpcExclusionData::getNpcAbyssTypes() const {
	return orEmpty(excludedAbyssTypes);
}

bool GlobalNpcExclusionData::isEmpty() const {
	return isEmpty_;
}

} // namespace aion::gameserver::dataholders
