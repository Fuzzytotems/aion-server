#pragma once

#include <cstdint>

#include "aion/gameserver/dataholders/AtreianPassportData.xml.h"
#include "aion/gameserver/dataholders/detail/LinkedMap.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.AtreianPassportData.
 * <p>
 * C++: the @XmlTransient index points into the bound `list` storage, which stays after afterUnmarshal (static-data.md §2.6). getAll returns the
 * index as a read-only map that iterates in Java's HashMap order (computed by afterUnmarshal): AtreianPassportService.onLogin adds the
 * account's new passports in that order.
 *
 * @author Alcapwnd, ViAl
 */
class AtreianPassportData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/AtreianPassportData.xml.inc"
private:
	/** in Java's HashMap iteration order */
	detail::LinkedMap<int32_t, const model::templates::event::AtreianPassport*> passportData;

public:
	int32_t size() const;

	/** Java: the HashMap; C++: a read-only map that iterates in Java's HashMap order */
	const detail::LinkedMap<int32_t, const model::templates::event::AtreianPassport*>& getAll() const;

	/** @return the passport template, nullptr (Java null) if there is none */
	const model::templates::event::AtreianPassport* getAtreianPassportId(int32_t id) const;
};

} // namespace aion::gameserver::dataholders
