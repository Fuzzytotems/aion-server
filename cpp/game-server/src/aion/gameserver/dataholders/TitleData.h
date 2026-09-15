#pragma once

#include <cstdint>

#include "aion/gameserver/dataholders/TitleData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.TitleData. @author xavier */
class TitleData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/TitleData.xml.inc"
public:
	/** @return the title template, nullptr (Java null) if there is none */
	const model::templates::TitleTemplate* getTitleTemplate(int32_t titleId) const;
};

} // namespace aion::gameserver::dataholders
