#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/walker/fwd.h"
#include "aion/gameserver/spawnengine/WalkerGroupShift.h"
#include "aion/gameserver/spawnengine/fwd.h"

namespace aion::gameserver::spawnengine {

/**
 * Stores for the spawn needed information, used for forming walker groups and spawning NPCs
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5): the element type of `WalkerGroup::members`. RefCounted (fieldmap K4), created with
 * create. The constructor reads the npc's spawn position (member stores only) and is ported.
 *
 * @author vlog, Rolandas
 */
class ClusteredNpc : public WalkerGroupShift {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<runtime::Ref<model::gameobjects::Npc>> npc{};
	const int32_t instance;
	const model::templates::walker::WalkerTemplate* walkTemplate;
	runtime::Field<float> x{};
	runtime::Field<float> y{};

protected:
	ClusteredNpc(model::gameobjects::Npc& npc, int32_t instance, const model::templates::walker::WalkerTemplate* walkTemplate);
	~ClusteredNpc() override;

public:
	/** Java: new ClusteredNpc(npc, instance, walkTemplate) */
	static runtime::Ref<ClusteredNpc> create(model::gameobjects::Npc& npc, int32_t instance,
		const model::templates::walker::WalkerTemplate* walkTemplate);

	runtime::Ptr<model::gameobjects::Npc> getNpc() const { return npc.get(); }

	int32_t getInstance() const { return instance; }

	void spawn(float z);

	void despawn();

	void setNpc(model::gameobjects::Npc& npc, const model::templates::walker::RouteStep* step);

	/** @param other null-checked in Java */
	bool hasSamePosition(runtime::Ptr<ClusteredNpc> other);

	int32_t getPositionHash();

	/**
	 * @return the x
	 */
	float getX() const { return x.get(); }

	float getXDelta();

	void setX(float value) { x.set(value); }

	/**
	 * @return the y
	 */
	float getY() const { return y.get(); }

	float getYDelta();

	void setY(float value) { y.set(value); }

	/**
	 * @return the walkTemplate
	 */
	const model::templates::walker::WalkerTemplate* getWalkTemplate() const { return walkTemplate; }

	/** Java: Integer (null when the spawn has no walker index) */
	std::optional<int32_t> getWalkerIndex();
};

} // namespace aion::gameserver::spawnengine
