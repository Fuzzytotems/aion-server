#pragma once

#include <cstdint>
#include <map>

#include "aion/gameserver/dataholders/SignetDataTemplates.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.SignetDataTemplates.
 * <p>
 * C++: the index (Java EnumMap) points into the bound `signetDataTemplateList` storage, which stays after afterUnmarshal (static-data.md §2.6).
 */
class SignetDataTemplates : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/SignetDataTemplates.xml.inc"
private:
	std::map<skillengine::model::SignetEnum, const skillengine::model::SignetDataTemplate*> signets;

public:
	/** @return the signet data of the level, nullptr (Java null) if there is none */
	const skillengine::model::SignetData* getSignetData(skillengine::model::SignetEnum signet, int32_t level) const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
