#pragma once

#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/instance/fwd.h"
#include "aion/gameserver/model/instance/instancescore/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::instance::handlers {

/**
 * Handler of a world map instance: receives the instance's game events (players entering and leaving, deaths, spawns, stages, ...).
 * <p>
 * Hub header (docs/design/hub-headers.md §9.2): an interface held by `runtime::Ref<InstanceHandler>` (HandlerRegistry.h InstanceFactory,
 * WorldMapInstance.instanceHandler), so it declares the reference count operations; GeneralInstanceHandler (RefCounted) forwards them.
 * Registered handlers provide `static runtime::Ref<C> create(world::WorldMapInstance& instance)` returning exactly their own type.
 *
 * @author ATracer
 */
class InstanceHandler {
public:
	/**
	 * Executed during instance creation.<br>
	 * This method will run after spawns are loaded
	 */
	virtual void onInstanceCreate() = 0;

	/**
	 * Executed during instance destroy.<br>
	 * This method will run after all spawns unloaded.<br>
	 * All class-shared objects should be cleaned in handler
	 */
	virtual void onInstanceDestroy() = 0;

	virtual void onPlayerLogin(model::gameobjects::player::Player& player) = 0;

	virtual void onPlayerLogout(model::gameobjects::player::Player& player) = 0;

	virtual void onEnterInstance(model::gameobjects::player::Player& player) = 0;

	virtual void leaveInstance(model::gameobjects::player::Player& player) = 0;

	virtual void onLeaveInstance(model::gameobjects::player::Player& player) = 0;

	virtual void onOpenDoor(int32_t door) = 0;

	virtual void onEnterZone(model::gameobjects::player::Player& player, world::zone::ZoneInstance& zone) = 0;

	virtual void onLeaveZone(model::gameobjects::player::Player& player, world::zone::ZoneInstance& zone) = 0;

	virtual void onPlayMovieEnd(model::gameobjects::player::Player& player, int32_t movieId) = 0;

	virtual bool onReviveEvent(model::gameobjects::player::Player& player) = 0;

	virtual void doReward(model::gameobjects::player::Player& player) = 0;

	virtual bool onDie(model::gameobjects::player::Player& player, model::gameobjects::Creature& lastAttacker) = 0;

	virtual void onStopTraining(model::gameobjects::player::Player& player) = 0;

	virtual void onDespawn(model::gameobjects::Npc& npc) = 0;

	virtual void onDie(model::gameobjects::Npc& npc) = 0;

	virtual void onSpawn(model::gameobjects::VisibleObject& obj) = 0;

	virtual void onChangeStage(model::instance::StageType type) = 0;

	virtual void onChangeStageList(model::instance::StageList list) = 0;

	virtual model::instance::StageType getStage() = 0;

	virtual void onDropRegistered(model::gameobjects::Npc& npc, int32_t winnerObj) = 0;

	virtual void onGather(model::gameobjects::player::Player& player, model::gameobjects::Gatherable& gatherable) = 0;

	/** Java: InstanceScore<?> (erased generic, hub-headers.md §8.1); null if the instance keeps no score */
	virtual runtime::Ptr<model::instance::instancescore::InstanceScore> getInstanceScore() = 0;

	virtual bool onPassFlyingRing(model::gameobjects::player::Player& player, std::string_view flyingRing) = 0;

	virtual void handleUseItemFinish(runtime::Ptr<model::gameobjects::player::Player> player, model::gameobjects::Npc& npc) = 0;

	virtual void onEndCastSkill(skillengine::model::Skill& skill) = 0;

	virtual void onAggro(model::gameobjects::Npc& npc) = 0;

	virtual void onStartEffect(runtime::Ptr<skillengine::model::Effect> effect) = 0;

	virtual void onEndEffect(skillengine::model::Effect& effect) = 0;

	virtual void onCreatureDetected(model::gameobjects::Npc& detector, model::gameobjects::Creature& detected) = 0;

	virtual void onSpecialEvent(model::gameobjects::Npc& npc) = 0;

	virtual void onBackHome(model::gameobjects::Npc& npc) = 0;

	virtual void portToStartPosition(model::gameobjects::player::Player& player) = 0;

	virtual bool canEnter(model::gameobjects::player::Player& player) = 0;

	virtual float getExpMultiplier() = 0;

	virtual float getApMultiplier() = 0;

	virtual bool allowSelfReviveBySkill() = 0;
	virtual bool allowSelfReviveByItem() = 0;
	virtual bool allowKiskRevive() = 0;
	virtual bool allowInstanceRevive() = 0;

	/** C++ only: Ref<InstanceHandler> retains the implementing object (hub-headers.md §9.2). */
	virtual void retain() const noexcept = 0;
	/** C++ only: Ref<InstanceHandler> releases the implementing object (hub-headers.md §9.2). */
	virtual void release() const noexcept = 0;

	virtual ~InstanceHandler() = default;

protected:
	InstanceHandler() = default;
	InstanceHandler(const InstanceHandler&) = default;
	InstanceHandler& operator=(const InstanceHandler&) = default;
};

} // namespace aion::gameserver::instance::handlers
