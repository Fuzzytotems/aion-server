#include "aion/gameserver/dataholders/SkillAliasLocationData.h"

namespace aion::gameserver::dataholders {

void SkillAliasLocationData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const skillengine::model::SkillAliasLocation& loc : skillAliasLocationData)
		skillAliasLocations.insert_or_assign(loc.getAliasName(), &loc);
	// Java: skillAliasLocationData = null (the C++ index points into the storage, which stays)
}

const skillengine::model::SkillAliasLocation* SkillAliasLocationData::getSkillAliasLocation(std::string_view alias) const {
	auto it = skillAliasLocations.find(alias);
	return it != skillAliasLocations.end() ? it->second : nullptr;
}

int32_t SkillAliasLocationData::size() const {
	return static_cast<int32_t>(skillAliasLocations.size());
}

} // namespace aion::gameserver::dataholders
