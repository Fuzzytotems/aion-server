#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/services/siege/fwd.h"

namespace aion::gameserver::services::siege {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 * Siege<?> is the erased Siege (hub-headers.md §8.1).
 *
 * @author synchro2, Luzien, Estrayl
 *         (Java TODO) Send Peace Dredgion without assault
 *         (Java TODO) Dredgion Battleship as real NPC
 */
class BalaurAssaultService : public runtime::Immortal {
private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<FortressAssault>> fortressAssaults{AION_LOCK_CLASS(BalaurAssaultService::fortressAssaults#stripe)}; // Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<ArtifactAssault>> artifactAssaults{AION_LOCK_CLASS(BalaurAssaultService::artifactAssaults#stripe)}; // Java: = new ConcurrentHashMap<>()
	BalaurAssaultService();
	~BalaurAssaultService();
public:
	static BalaurAssaultService& getInstance(); // Java singleton
	void onSiegeStart(Siege& siege);
	void onSiegeFinish(Siege& siege);
private:
	bool calculateFortressAssault(model::siege::FortressLocation& fortress);
public:
	bool startAssault(int32_t location, int32_t delay);
private:
	void newAssault(Siege& siege, int32_t delay);
public:
	void spawnDredgion(int32_t spawnId);
	runtime::Ptr<FortressAssault> getFortressAssaultBySiegeId(int32_t siegeId);
};

} // namespace aion::gameserver::services::siege
