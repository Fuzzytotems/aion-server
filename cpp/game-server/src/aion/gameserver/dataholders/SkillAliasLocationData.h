#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>

#include "aion/gameserver/dataholders/SkillAliasLocationData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.SkillAliasLocationData.
 * <p>
 * C++: the index points into the bound `skillAliasLocationData` storage, which stays after afterUnmarshal (static-data.md §2.6).
 */
class SkillAliasLocationData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/SkillAliasLocationData.xml.inc"
private:
	std::map<std::string, const skillengine::model::SkillAliasLocation*, std::less<>> skillAliasLocations;

public:
	/** @return the alias location, nullptr (Java null) if there is none */
	const skillengine::model::SkillAliasLocation* getSkillAliasLocation(std::string_view alias) const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
