#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/base/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 * Base<?> is the erased Base (hub-headers.md §8.1).
 *
 * @author Source, Estrayl
 */
class BaseService : public runtime::Immortal {
private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::base::Base>> activeBases{AION_LOCK_CLASS(BaseService::activeBases#stripe)}; // Java: = new ConcurrentHashMap<>()
	runtime::HashMap<int32_t, runtime::Ref<model::base::BaseLocation>> allBaseLocations{AION_LOCK_CLASS(BaseService::allBaseLocations)}; // Java: = new HashMap<>()
	/** Initializes all base locations */
	BaseService();
	~BaseService();
public:
	/** Executes start of all casual and stained bases. */
	void initBases();
	/** Generates a new BaseObject for given id */
	void start(int32_t id);
	/**
	 * Removes base with given id from activeBases
	 * and stops it.
	 * Should only directly call for SiegeBases
	 */
	void stop(int32_t id);
private:
	runtime::Ref<model::base::Base> newBase(int32_t id);
public:
	void capture(int32_t id, model::base::BaseOccupier newOccupier);
private:
	void handleStainedFeatures(model::base::BaseColorType colorType, model::base::BaseOccupier newOccupier);
public:
	std::vector<runtime::Ptr<model::base::BaseLocation>> getBaseLocations();
	runtime::Ptr<model::base::Base> getActiveBase(int32_t id);
	runtime::Ptr<model::base::BaseLocation> getBaseLocation(int32_t id);
	bool isActive(int32_t id);
	static BaseService& getInstance(); // Java singleton
};

} // namespace aion::gameserver::services
