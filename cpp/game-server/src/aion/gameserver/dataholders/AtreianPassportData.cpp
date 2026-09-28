#include "aion/gameserver/dataholders/AtreianPassportData.h"

#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"

namespace aion::gameserver::dataholders {

void AtreianPassportData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	detail::JavaHashMapOrder<int32_t, const model::templates::event::AtreianPassport*> order;
	for (const model::templates::event::AtreianPassport& passport : list)
		order.put(passport.getId(), &passport, detail::javaHashCode(passport.getId()));
	passportData = detail::toLinkedMap<decltype(passportData)>(order);
	// Java: list = null (the C++ index points into the storage, which stays)
}

int32_t AtreianPassportData::size() const {
	return static_cast<int32_t>(passportData.size());
}

const detail::LinkedMap<int32_t, const model::templates::event::AtreianPassport*>& AtreianPassportData::getAll() const {
	return passportData;
}

const model::templates::event::AtreianPassport* AtreianPassportData::getAtreianPassportId(int32_t id) const {
	auto* passport = passportData.get(id);
	return passport != nullptr ? *passport : nullptr;
}

} // namespace aion::gameserver::dataholders
