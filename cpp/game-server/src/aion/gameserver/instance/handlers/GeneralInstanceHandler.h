#pragma once

#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/instance/StageType.h"
#include "aion/gameserver/model/instance/fwd.h"
#include "aion/gameserver/model/instance/instancescore/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/world/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::commons::logging {
class Logger;
} // namespace aion::commons::logging

namespace aion::gameserver::instance::handlers {

/**
 * Default instance handler (instances without a registered handler) and base of every registered instance handler.
 * <p>
 * Hub header (docs/design/hub-headers.md). RefCounted (handlers-and-porting-plan.md amendment §2): created per WorldMapInstance through
 * `static runtime::Ref<C> create(world::WorldMapInstance& instance)`; every registered subclass declares its own `create` returning
 * `Ref<Subclass>` (HandlerRegistry.h InstanceHandlerClass rejects an inherited one). The first InstanceHandler implementor with a runtime base,
 * so it forwards retain()/release(). The protected logger "INSTANCE_LOG" is a class member (§11.3). Empty Java bodies and literal returns are
 * ported inline. sendMsg takes the packet by reference and forwards temporaries (`sendMsg(SM_SYSTEM_MESSAGE::STR_X())`) through a template
 * (§12).
 *
 * @author ATracer
 */
class GeneralInstanceHandler : public runtime::RefCounted, public InstanceHandler {
	AION_MAKE_REF_FRIEND
protected:
	/** Java: protected static final Logger log = LoggerFactory.getLogger("INSTANCE_LOG") */
	static const commons::logging::Logger log;
	const runtime::Ref<world::WorldMapInstance> instance;
	const int32_t mapId;

	explicit GeneralInstanceHandler(world::WorldMapInstance& instance);
	~GeneralInstanceHandler() override;

public:
	/** Java: new GeneralInstanceHandler(instance) (InstanceEngine's fallback) */
	static runtime::Ref<GeneralInstanceHandler> create(world::WorldMapInstance& instance);

	void onInstanceCreate() override {}

	void onInstanceDestroy() override {}

	void onPlayerLogin(model::gameobjects::player::Player& player) override {}

	void onPlayerLogout(model::gameobjects::player::Player& player) override {}

	void onEnterInstance(model::gameobjects::player::Player& player) override {}

	void leaveInstance(model::gameobjects::player::Player& player) override {}

	void onLeaveInstance(model::gameobjects::player::Player& player) override;

	void onOpenDoor(int32_t door) override {}

	void onEnterZone(model::gameobjects::player::Player& player, world::zone::ZoneInstance& zone) override {}

	void onLeaveZone(model::gameobjects::player::Player& player, world::zone::ZoneInstance& zone) override {}

	void onPlayMovieEnd(model::gameobjects::player::Player& player, int32_t movieId) override {}

	bool onReviveEvent(model::gameobjects::player::Player& player) override { return false; }

protected:
	runtime::Ptr<model::gameobjects::VisibleObject> spawn(int32_t npcId, float x, float y, float z, int8_t heading);

	runtime::Ptr<model::gameobjects::VisibleObject> spawn(int32_t npcId, float x, float y, float z, int8_t heading, int32_t staticId);

	runtime::Ptr<model::gameobjects::VisibleObject> spawnAndSetRespawn(int32_t npcId, float x, float y, float z, int8_t heading, int32_t respawnTime);

	runtime::Ptr<model::gameobjects::Npc> getNpc(int32_t npcId);

	void deleteAliveNpcs(std::initializer_list<int32_t> npcIds);

	/**
	 * Sends a message to all players in this instance.
	 */
	void sendMsg(network::aion::serverpackets::SM_SYSTEM_MESSAGE& msg);

	/** C++ only: sendMsg for a temporary packet (hub-headers.md §12). */
	template <std::same_as<network::aion::serverpackets::SM_SYSTEM_MESSAGE> P>
	void sendMsg(P&& msg) {
		sendMsg(static_cast<network::aion::serverpackets::SM_SYSTEM_MESSAGE&>(msg));
	}

	/**
	 * Sends a message to all players in this instance, after the specified delay (in milliseconds).
	 */
	void sendMsg(network::aion::serverpackets::SM_SYSTEM_MESSAGE& msg, int32_t delay);

	/** C++ only: sendMsg for a temporary packet (hub-headers.md §12). */
	template <std::same_as<network::aion::serverpackets::SM_SYSTEM_MESSAGE> P>
	void sendMsg(P&& msg, int32_t delay) {
		sendMsg(static_cast<network::aion::serverpackets::SM_SYSTEM_MESSAGE&>(msg), delay);
	}

public:
	void doReward(model::gameobjects::player::Player& player) override {}

	bool onDie(model::gameobjects::player::Player& player, model::gameobjects::Creature& lastAttacker) override { return false; }

	void onStopTraining(model::gameobjects::player::Player& player) override {}

	void onDespawn(model::gameobjects::Npc& npc) override;

	void onDie(model::gameobjects::Npc& npc) override;

	void logNpcWithReason(model::gameobjects::Npc& npc, std::string_view reason);

	virtual bool isBoss(model::gameobjects::Npc& npc);

	void onSpawn(model::gameobjects::VisibleObject& object) override {}

	void onAggro(model::gameobjects::Npc& npc) override {}

	void onChangeStage(model::instance::StageType type) override {}

	void onChangeStageList(model::instance::StageList list) override {}

	model::instance::StageType getStage() override { return model::instance::StageType::DEFAULT; }

	void onDropRegistered(model::gameobjects::Npc& npc, int32_t winnerObj) override {}

	void onGather(model::gameobjects::player::Player& player, model::gameobjects::Gatherable& gatherable) override {}

	runtime::Ptr<model::instance::instancescore::InstanceScore> getInstanceScore() override { return nullptr; }

	bool onPassFlyingRing(model::gameobjects::player::Player& player, std::string_view flyingRing) override { return false; }

	void handleUseItemFinish(runtime::Ptr<model::gameobjects::player::Player> player, model::gameobjects::Npc& npc) override {}

	void onEndCastSkill(skillengine::model::Skill& skill) override {}

	void onStartEffect(runtime::Ptr<skillengine::model::Effect> effect) override {}

	void onEndEffect(skillengine::model::Effect& effect) override {}

	void onCreatureDetected(model::gameobjects::Npc& detector, model::gameobjects::Creature& detected) override {}

	void onSpecialEvent(model::gameobjects::Npc& npc) override {}

	void onBackHome(model::gameobjects::Npc& npc) override {}

	void portToStartPosition(model::gameobjects::player::Player& player) override;

	bool canEnter(model::gameobjects::player::Player& player) override { return true; }

	float getExpMultiplier() override;

	float getApMultiplier() override { return 1.0f; }

	bool allowSelfReviveBySkill() override {
		return true; // see skill_prohibit_set_id in client /data/world/worldid.xml + data/skills/client_skill_prohibit.xml
	}

	bool allowSelfReviveByItem() override { return true; }

	bool allowKiskRevive() override;

	bool allowInstanceRevive() override;

protected:
	virtual bool isRestrictedToInstance(model::gameobjects::Item& item);

private:
	void removeInstanceItems(model::gameobjects::player::Player& player);

public:
	/** C++ only: InstanceHandler held by Ref retains this object (hub-headers.md §9.2). */
	void retain() const noexcept override { runtime::RefCounted::retain(); }
	/** C++ only: InstanceHandler held by Ref releases this object (hub-headers.md §9.2). */
	void release() const noexcept override { runtime::RefCounted::release(); }
};

} // namespace aion::gameserver::instance::handlers
