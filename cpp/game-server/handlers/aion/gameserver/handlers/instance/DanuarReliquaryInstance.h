#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::instance {

/**
 * Danuar Reliquary (301110000): the three guardians' deaths wake Cursed Queen Modor and start the 15 minute wipe; her real clone's death
 * removes the fakes and, the third time, brings Enraged Queen Modor, whose death ends the instance with the exit and the treasure box; the
 * wipe ends it without the box; a boss back home clears the room.
 * <p>
 * Java: data/handlers/instance/DanuarReliquaryInstance.java, @InstanceID(301110000). C++: the scheduled lambdas are pinned to the handler.
 *
 * @author Ritsu, Estrayl, Yeats
 */
class DanuarReliquaryInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::AtomicBoolean isCursedModorActive{AION_LOCK_CLASS(DanuarReliquaryInstance::isCursedModorActive)}; // Java: = new AtomicBoolean()
	runtime::AtomicInteger cloneKills{AION_LOCK_CLASS(DanuarReliquaryInstance::cloneKills)}; // Java: = new AtomicInteger()
	runtime::Field<runtime::FutureRef> wipeTask{};

protected:
	explicit DanuarReliquaryInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<DanuarReliquaryInstance> create(WorldMapInstance& instance);

protected:
	virtual int32_t getExitId();

	virtual int32_t getTreasureBoxId();

	virtual int32_t getEnragedModorId();

	virtual int32_t getCursedModorId();

	virtual int32_t getRealCloneId();

	virtual int32_t getFakeCloneId();

public:
	void onDie(Npc& npc) override;

private:
	void spawnClones();

protected:
	virtual void onInstanceEnd(bool successful);

private:
	void scheduleWipe();

protected:
	/** Java: package-private (the subclasses call it) */
	void cancelWipeTask();

public:
	void onPlayerLogout(Player& player) override;

	void onInstanceDestroy() override;

	void onBackHome(Npc& npc) override;
};

} // namespace aion::gameserver::handlers::instance
