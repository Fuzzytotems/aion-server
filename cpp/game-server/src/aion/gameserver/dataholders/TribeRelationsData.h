#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/TribeRelationsData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.TribeRelationsData.
 * <p>
 * C++: the index points into the bound `tribeList` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author ATracer
 */
class TribeRelationsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/TribeRelationsData.xml.inc"
private:
	std::unordered_map<model::TribeClass, const model::templates::tribe::Tribe*> tribeNameMap;

	/** Java tribeNameMap.get(tribeName): nullptr for Java null */
	const model::templates::tribe::Tribe* get(model::TribeClass tribeName) const;

public:
	int32_t size() const;

	/** Java: NullPointerException if the tribe has no relation template */
	model::TribeClass getBaseTribe(model::TribeClass tribeName) const;

	bool isAggressiveRelation(model::TribeClass tribeName1, model::TribeClass tribeName2) const;

	bool isSupportRelation(model::TribeClass tribeName1, model::TribeClass tribeName2) const;

	bool isFriendlyRelation(model::TribeClass tribeName1, model::TribeClass tribeName2) const;

	bool isNeutralRelation(model::TribeClass tribeName1, model::TribeClass tribeName2) const;

	bool isNoneRelation(model::TribeClass tribeName1, model::TribeClass tribeName2) const;

	bool isHostileRelation(model::TribeClass tribeName1, model::TribeClass tribeName2) const;

	/** @return True, if tribeName can support tribeNameAskingForSupport */
	bool canSupport(model::TribeClass tribeName, model::TribeClass tribeNameAskingForSupport) const;
};

} // namespace aion::gameserver::dataholders
