#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/TitleData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.TitleData.
 * <p>
 * C++: the index points into the bound `tts` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author xavier
 */
class TitleData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/TitleData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::TitleTemplate*> titles;

public:
	/** @return the title template, nullptr (Java null) if there is none */
	const model::templates::TitleTemplate* getTitleTemplate(int32_t titleId) const;

	/** @return titles.size() */
	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
