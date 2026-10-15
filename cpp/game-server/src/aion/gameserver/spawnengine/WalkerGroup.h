#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/model/templates/zone/fwd.h"
#include "aion/gameserver/spawnengine/fwd.h"

namespace aion::gameserver::spawnengine {

/**
 * A group of npcs walking a route in formation.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `Npc::walkerGroup`), created with create(members). The
 * constructor sorts the members and reads their walk templates, so it stays unported (its initializer list gives the const members neutral
 * values until then). getLinePoint works on the Point2D zone templates as values (Java creates new Point2D objects).
 *
 * @author vlog, Rolandas
 */
class WalkerGroup : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::ArrayList<runtime::Ref<ClusteredNpc>> members{AION_LOCK_CLASS(WalkerGroup::members)};
	const WalkerGroupType type;
	const float walkerXpos;
	const float walkerYpos;
	const runtime::Ref<runtime::Array<int32_t>> memberSteps;
	runtime::Field<int32_t> groupStep{};
	const std::string versionId;
	runtime::Field<bool> isSpawned_{}; // Java: isSpawned (renamed: clashes with isSpawned())

protected:
	explicit WalkerGroup(const std::vector<runtime::Ptr<ClusteredNpc>>& members);
	~WalkerGroup() override;

public:
	/** Java: new WalkerGroup(members) */
	static runtime::Ref<WalkerGroup> create(const std::vector<runtime::Ptr<ClusteredNpc>>& members);

	void form();

private:
	float getSidesExtra(std::span<const int32_t> rows, int32_t startIndex, int32_t endIndex);

public:
	/**
	 * Returns coordinates of NPC in 2D from the initial spawn location
	 *
	 * @param origin initial spawn location
	 * @param destination point of next move
	 * @param shift distance from origin located in lines perpendicular to destination
	 */
	static model::templates::zone::Point2D getLinePoint(const model::templates::zone::Point2D& origin,
		const model::templates::zone::Point2D& destination, WalkerGroupShift& shift);

private:
	/** Return a normalized direction vector (Java: a new WalkerGroupShift) */
	static runtime::Ref<WalkerGroupShift> getShiftSigns(const model::templates::zone::Point2D& origin,
		const model::templates::zone::Point2D& destination);

public:
	void setStep(model::gameobjects::Npc& member, int32_t step);

	void targetReached(ai::NpcAI& npcAI); // synchronized (members)

	bool isSpawned() const { return isSpawned_.get(); }

	void spawn();

	void respawn(model::gameobjects::Npc& npc); // synchronized (members)

	void despawn();

	runtime::Ptr<ClusteredNpc> getClusterData(model::gameobjects::Npc& npc);

private:
	float getHeight(float x, float y, model::templates::spawns::SpawnTemplate& template_);

public:
	int32_t getPool();

	WalkerGroupType getWalkType() const { return type; }

	bool isLinearlyPositioned(model::gameobjects::Npc& npc);

	int32_t getGroupStep() const { return groupStep.get(); }

	std::string getVersionId() const { return versionId; }
};

} // namespace aion::gameserver::spawnengine
