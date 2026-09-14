#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/rift/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/model/vortex/fwd.h"
#include "aion/gameserver/services/rift/fwd.h"

namespace aion::gameserver::services::rift {

/**
 * @author Source
 */
class RiftManager : public runtime::Immortal {
private:
	// C++: both maps are defined in RiftManager.cpp (their element destructors need the complete Npc and SpawnTemplate, hub-headers.md §3.1)
	static runtime::ConcurrentHashMap<int32_t, runtime::Ref<runtime::RcArrayList<runtime::Ref<model::gameobjects::Npc>>>> riftsPerWorld;
	static runtime::ConcurrentHashMap<std::string, runtime::Ref<model::templates::spawns::SpawnTemplate>> riftGroups;
public:
	static void addRiftSpawnTemplate(model::templates::spawns::SpawnGroup& spawn);
	void spawnRift(model::rift::RiftLocation& loc, bool isWithGuards);
	void spawnVortex(model::vortex::VortexLocation& loc);
private:
	void spawnRift(RiftEnum rift, runtime::Ptr<model::vortex::VortexLocation> vl, runtime::Ptr<model::rift::RiftLocation> rl, bool isWithGuards);
	runtime::Ptr<model::gameobjects::Npc> spawnInstance(int32_t instance, model::templates::spawns::SpawnTemplate& template_,
		controllers::RVController& controller);
	static void addSpawnedRift(model::gameobjects::Npc& rift);
public:
	static std::vector<runtime::Ptr<model::gameobjects::Npc>> getSpawnedRifts(int32_t worldId);
	static bool removeSpawnedRift(model::gameobjects::Npc& rift);
	static RiftManager& getInstance(); // Java singleton
};

} // namespace aion::gameserver::services::rift
