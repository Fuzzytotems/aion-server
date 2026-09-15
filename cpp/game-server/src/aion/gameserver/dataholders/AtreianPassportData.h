#pragma once

#include <cstdint>

#include "aion/gameserver/dataholders/AtreianPassportData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.AtreianPassportData. @author Alcapwnd, ViAl */
class AtreianPassportData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/AtreianPassportData.xml.inc"
public:
	/** @return the passport template, nullptr (Java null) if there is none */
	const model::templates::event::AtreianPassport* getAtreianPassportId(int32_t id) const;
};

} // namespace aion::gameserver::dataholders
