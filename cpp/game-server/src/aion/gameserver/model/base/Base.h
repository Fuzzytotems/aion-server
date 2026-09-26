#pragma once

#include <cstdint>
#include <initializer_list>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/model/base/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/spawnengine/fwd.h"

namespace aion::gameserver::model::base {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 * Java generic Base<T>: erased to a non-template class, type variables spelled as their bounds (hub-headers.md §8.1).
 *
 * @author Source, Estrayl
 */
class Base : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<BaseLocation> bLoc;
	const int32_t id;
	runtime::ArrayList<runtime::Ref<gameobjects::Npc>> assaulter{AION_LOCK_CLASS(Base::assaulter)}; // Java: = new ArrayList<>()
	runtime::AtomicBoolean isStarted_{AION_LOCK_CLASS(Base::isStarted)}; // Java: = new AtomicBoolean()
	runtime::AtomicBoolean isStopped_{AION_LOCK_CLASS(Base::isStopped)}; // Java: = new AtomicBoolean()
	runtime::Field<runtime::FutureRef> assaultTask{};
	runtime::Field<runtime::FutureRef> assaultDespawnTask{};
	runtime::Field<runtime::FutureRef> bossSpawnTask{};
	runtime::Field<runtime::FutureRef> outriderSpawnTask{};
	runtime::Field<runtime::Ref<gameobjects::Npc>> flag{};

protected:
	virtual int32_t getAssaultDelay() = 0;

	virtual int32_t getAssaultDespawnDelay() = 0;

	virtual int32_t getBossSpawnDelay() = 0;

	virtual int32_t getNpcSpawnDelay() = 0;

	explicit Base(BaseLocation& bLoc);

public:
	void start();

	void stop();

protected:
	void handleStart();

	virtual void handleStop();

private:
	void despawnAllNpcs();

protected:
	void despawnByHandlerType(spawnengine::SpawnHandlerType type);

private:
	void despawnNpcs(spawnengine::SpawnHandlerType type);

	bool isSpawnForCurrentBase(templates::spawns::SpawnTemplate& spawnTemplate, spawnengine::SpawnHandlerType type);

protected:
	void scheduleOutriderSpawn();

	void scheduleBossSpawn();

private:
	void scheduleAssault();

protected:
	virtual BaseOccupier chooseAssaultRace();

private:
	void scheduleAssaultDespawn();

	void despawnAssaulter();

public:
	void spawnBySpawnHandler(spawnengine::SpawnHandlerType type, BaseOccupier occupier);

	virtual BaseOccupier getOccupier(runtime::Ptr<gameobjects::Creature> bossKiller);

private:
	network::aion::serverpackets::SM_SYSTEM_MESSAGE getBossSpawnMsg();

	network::aion::serverpackets::SM_SYSTEM_MESSAGE getAssaultMsg();

protected:
	void cancelTask(std::initializer_list<runtime::FutureRef> tasks = {});

public:
	runtime::Ptr<BaseLocation> getLocation() const { return this->bLoc; }

	int32_t getId() const { return this->id; }

	int32_t getWorldId();

	BaseOccupier getOccupier();

	bool isStarted();

	bool isStopped();

	bool isUnderAssault();

protected:
	~Base() override;
};

} // namespace aion::gameserver::model::base
