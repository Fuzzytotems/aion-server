#include "aion/gameserver/dataholders/TribeRelationsData.h"

#include <algorithm>
#include <string>
#include <vector>

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dataholders {

using model::TribeClass;
using model::templates::tribe::Tribe;

namespace {

bool contains(const std::vector<TribeClass>& list, TribeClass tribe) {
	return std::find(list.begin(), list.end(), tribe) != list.end();
}

} // namespace

void TribeRelationsData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const Tribe& tribe : tribeList)
		tribeNameMap.insert_or_assign(tribe.getName(), &tribe);
	// Java: tribeList = null (the C++ index points into the storage, which stays)
}

const Tribe* TribeRelationsData::get(TribeClass tribeName) const {
	auto it = tribeNameMap.find(tribeName);
	return it != tribeNameMap.end() ? it->second : nullptr;
}

int32_t TribeRelationsData::size() const {
	return static_cast<int32_t>(tribeNameMap.size());
}

TribeClass TribeRelationsData::getBaseTribe(TribeClass tribeName) const {
	const Tribe* tribe = get(tribeName);
	if (tribe == nullptr)
		throw runtime::NullPointerException("Cannot invoke \"Tribe.getBase()\" because \"tribe\" is null (" + std::string(xml::enumName(tribeName)) +
		                                    ")");
	return tribe->getBase();
}

bool TribeRelationsData::isAggressiveRelation(TribeClass tribeName1, TribeClass tribeName2) const {
	const Tribe* tribe1 = get(tribeName1);
	const Tribe* tribe2 = get(tribeName2);
	if (tribe1 == nullptr || tribe2 == nullptr)
		return false;
	return contains(tribe1->getAggro(), tribe2->getBase()) || contains(tribe1->getAggro(), tribeName2) ||
	       contains(tribe2->getAggro(), tribe1->getBase()) || contains(tribe2->getAggro(), tribeName1);
}

bool TribeRelationsData::isSupportRelation(TribeClass tribeName1, TribeClass tribeName2) const {
	const Tribe* tribe1 = get(tribeName1);
	const Tribe* tribe2 = get(tribeName2);
	if (tribe1 == nullptr || tribe2 == nullptr)
		return false;
	return contains(tribe1->getSupport(), tribe2->getBase()) || contains(tribe1->getSupport(), tribeName2) ||
	       contains(tribe2->getSupport(), tribe1->getBase()) || contains(tribe2->getSupport(), tribeName1);
}

bool TribeRelationsData::isFriendlyRelation(TribeClass tribeName1, TribeClass tribeName2) const {
	const Tribe* tribe1 = get(tribeName1);
	const Tribe* tribe2 = get(tribeName2);
	if (tribe1 == nullptr || tribe2 == nullptr)
		return false;
	return contains(tribe1->getFriend(), tribe2->getBase()) || contains(tribe1->getFriend(), tribeName2) ||
	       contains(tribe2->getFriend(), tribe1->getBase()) || contains(tribe2->getFriend(), tribeName1);
}

bool TribeRelationsData::isNeutralRelation(TribeClass tribeName1, TribeClass tribeName2) const {
	const Tribe* tribe1 = get(tribeName1);
	const Tribe* tribe2 = get(tribeName2);
	if (tribe1 == nullptr || tribe2 == nullptr)
		return false;
	return contains(tribe1->getNeutral(), tribe2->getBase()) || contains(tribe1->getNeutral(), tribeName2) ||
	       contains(tribe2->getNeutral(), tribe1->getBase()) || contains(tribe2->getNeutral(), tribeName1);
}

bool TribeRelationsData::isNoneRelation(TribeClass tribeName1, TribeClass tribeName2) const {
	const Tribe* tribe1 = get(tribeName1);
	const Tribe* tribe2 = get(tribeName2);
	if (tribe1 == nullptr || tribe2 == nullptr)
		return false;
	return contains(tribe1->getNone(), tribe2->getBase()) || contains(tribe1->getNone(), tribeName2) ||
	       contains(tribe2->getNone(), tribe1->getBase()) || contains(tribe2->getNone(), tribeName1);
}

bool TribeRelationsData::isHostileRelation(TribeClass tribeName1, TribeClass tribeName2) const {
	const Tribe* tribe1 = get(tribeName1);
	const Tribe* tribe2 = get(tribeName2);
	if (tribe1 == nullptr || tribe2 == nullptr)
		return false;
	return contains(tribe1->getHostile(), tribe2->getBase()) || contains(tribe1->getHostile(), tribeName2) ||
	       contains(tribe2->getHostile(), tribe1->getBase()) || contains(tribe2->getHostile(), tribeName1);
}

bool TribeRelationsData::canSupport(TribeClass tribeName, TribeClass tribeNameAskingForSupport) const {
	const Tribe* tribe = get(tribeName);
	const Tribe* tribeAskingForSupport = get(tribeNameAskingForSupport);
	if (tribe == nullptr || tribeAskingForSupport == nullptr)
		return false;
	return contains(tribe->getSupport(), tribeNameAskingForSupport) || contains(tribe->getSupport(), tribeAskingForSupport->getBase());
}

} // namespace aion::gameserver::dataholders
