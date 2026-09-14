#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "aion/gameserver/runtime/collections/ConcurrentLinkedQueue.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/controllers/movement/fwd.h"
#include "aion/gameserver/model/SkillElement.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/model/summons/SummonMode.h"
#include "aion/gameserver/model/summons/fwd.h"
#include "aion/gameserver/model/templates/npc/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * Hub header (docs/design/hub-headers.md). The constructor stores the members (the master, the live time, a new WorldPosition of the spawn's
 * world); postConstruct() runs the rest of the Java constructor body in Java order: `controller.setOwner(*this)`, the move controller
 * (SiegeWeaponMoveController for a SiegeWeaponController, else SummonMoveController), game and life stats, the always-resist element.
 * getMaster() and getActingCreature() are real covariant overrides in Java (return type Player): they keep `Ptr<Creature>` (§8.2), callers
 * that need the Player cast (`runtime::cast<player::Player>(summon.getMaster())`).
 *
 * @author ATracer
 */
class Summon : public Creature {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<player::Player> master;
	runtime::Field<summons::SummonMode> mode{summons::SummonMode::GUARD};
	runtime::Field<summons::SummonMode> modeBeforeRelease{summons::SummonMode::GUARD}; // Java: = mode
	runtime::ConcurrentLinkedQueue<runtime::Ref<summons::SkillOrder>> skillOrders{};
	runtime::Field<runtime::Ref<summons::SummonRelease>> pendingRelease{};
	runtime::Field<SkillElement> alwaysResistElement{SkillElement::NONE};
	runtime::Field<int32_t> summonedBySkillId{};
	runtime::Field<int32_t> liveTime{};

protected:
	Summon(CreateKey key, int32_t objId, std::unique_ptr<controllers::SummonController> controller, templates::spawns::SpawnTemplate& spawnTemplate,
		const templates::npc::NpcTemplate* objectTemplate, player::Player& master, int32_t time);
	~Summon() override;

	/**
	 * Java constructor body after super(...): controller.setOwner(this), the move controller, setGameStats/setLifeStats,
	 * setAlwaysResistElement(objectTemplate)
	 */
	void postConstruct() override;

private:
	void setAlwaysResistElement(const templates::npc::NpcTemplate* template_);

protected:
	std::unique_ptr<controllers::attack::AggroList> createAggroList() override;

public:
	/** Narrows Creature::getGameStats (Java cast-only override) */
	runtime::Ptr<stats::container::SummonGameStats> getGameStats() const;

	/** Java return type Player (real covariant override: returns the master field) */
	runtime::Ptr<Creature> getMaster() override;

	int8_t getLevel() override;

	/** Narrows VisibleObject::getObjectTemplate (Java cast-only override) */
	const templates::npc::NpcTemplate* getObjectTemplate() const;

	int32_t getNpcId();

	std::string getL10n();

	NpcObjectType getNpcObjectType() override;

	/** Narrows Creature::getController (Java cast-only override) */
	controllers::SummonController& getController() const;

	summons::SummonMode getMode() const { return mode.get(); }

	/**
	 * @return The mode to report to the master's client, hiding a pending release the master was never told about (see SummonsService's handling
	 *         of UnsummonType#isCancelableByMaster()).
	 */
	summons::SummonMode getVisibleMode();

	void setMode(summons::SummonMode mode);

	bool isEnemy(Creature& creature) override;

	/** C++: keeps Creature::isEnemyFrom(Creature&) visible next to the overrides (no hiding in Java) */
	using Creature::isEnemyFrom;

	bool isEnemyFrom(Npc& npc) override;

	bool isEnemyFrom(player::Player& player) override;

	bool isPvpTarget(Creature& creature) override;

	std::optional<TribeClass> getTribe() override;

	CreatureType getType(Creature& creature);

	/** Narrows Creature::getMoveController (Java cast-only override) */
	runtime::Ptr<controllers::movement::SummonMoveController> getMoveController() const;

	/** Java return type Player (real covariant override: returns getMaster()) */
	runtime::Ptr<Creature> getActingCreature() override;

	Race getRace() override;

	bool isPet();

	/** @return liveTime in sec. */
	int32_t getLiveTime() const { return liveTime.get(); }

	/** @param liveTime in sec. */
	void setLiveTime(int32_t value) { liveTime.set(value); }

	int32_t getSummonedBySkillId() const { return summonedBySkillId.get(); }

	void setSummonedBySkillId(int32_t value) { summonedBySkillId.set(value); }

	/**
	 * An instant release supersedes a scheduled one, a release which already started can never be superseded.
	 *
	 * @return True if the caller may go on releasing this summon.
	 */
	bool registerRelease(summons::SummonRelease& release);

	/** @return True if the given release is still the pending one, meaning the caller may go on despawning this summon. */
	bool startRelease(summons::SummonRelease& release);

	void cancelReleaseByMaster();

	bool isReleaseUncancelable();

	bool isBeingReleased();

	void addSkillOrder(int32_t skillId, int32_t skillLvl, Creature& target, int32_t hate, bool release);

	runtime::Ptr<summons::SkillOrder> retrieveNextSkillOrder();

	runtime::Ptr<summons::SkillOrder> getNextSkillOrder();

	void clearSkillOrders();

	SkillElement getAlwaysResistElement() const { return alwaysResistElement.get(); }
};

} // namespace aion::gameserver::model::gameobjects
