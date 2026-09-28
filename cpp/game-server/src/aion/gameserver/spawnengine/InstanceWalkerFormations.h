#pragma once

#include <string>
#include <string_view>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/spawnengine/fwd.h"

namespace aion::gameserver::spawnengine {

/**
 * The walker candidates, walker groups and route variants of one world map instance.
 * <p>
 * C++: RefCounted (fieldmap K4, WorldWalkerFormations.formations), created with create(). Java's protected methods are public (same package).
 *
 * @author Rolandas
 */
class InstanceWalkerFormations : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::HashMap<std::string, runtime::Ref<runtime::RcArrayList<runtime::Ref<ClusteredNpc>>>> groupedSpawnObjects{
		AION_LOCK_CLASS(InstanceWalkerFormations::groupedSpawnObjects)};
	runtime::HashMap<std::string, runtime::Ref<WalkerGroup>> walkFormations{AION_LOCK_CLASS(InstanceWalkerFormations::walkFormations)};
	runtime::HashMap<std::string, runtime::Ref<runtime::RcArrayList<runtime::Ref<WalkerGroup>>>> formationVariants{
		AION_LOCK_CLASS(InstanceWalkerFormations::formationVariants)};
	runtime::HashMap<std::string, runtime::Ref<runtime::RcArrayList<runtime::Ref<ClusteredNpc>>>> walkerVariants{
		AION_LOCK_CLASS(InstanceWalkerFormations::walkerVariants)};

protected:
	InstanceWalkerFormations();
	~InstanceWalkerFormations() override;

public:
	/** Java: new InstanceWalkerFormations() */
	static runtime::Ref<InstanceWalkerFormations> create();

	runtime::Ptr<WalkerGroup> getSpawnWalkerGroup(std::string_view walkerId);

	/** Java protected synchronized */
	bool cacheWalkerCandidate(ClusteredNpc& npcWalker);

	/** Organizes spawns in all processed walker groups. Must be called only when spawning all npcs for the instance of world. (Java protected) */
	void organizeAndSpawn();

	/** Java protected */
	void changeCluster(WalkerGroup& walkerGroup);

	/** Java protected */
	void changeWalker(model::gameobjects::Npc& npc);

	/** Java protected synchronized */
	void onInstanceDestroy();
};

} // namespace aion::gameserver::spawnengine
