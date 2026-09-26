#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/controllers/effect/fwd.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/controllers/movement/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/gameobjects/state/fwd.h"
#include "aion/gameserver/model/items/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/model/templates/zone/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/world/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * This class is representing movable objects, its base class for all in game objects that may move
 * <p>
 * Hub header (docs/design/hub-headers.md). Construction is two-phase (handlers-and-porting-plan.md §1.7, amendment §4): the constructor only
 * stores members (Java field initializers included); postConstruct() runs the Java constructor body that needs the dynamic type: the AI is
 * created by `AIEngine::newAI(aiName, *this)` into the `ai` PartSlot (RetireTo::OWNER, `//ai set` replaces it through replaceAi), then the
 * aggro list through the virtual createAggroList(). The ObserveController has no dependency on the dynamic type and is created by the
 * constructor (it is a `const Ref`).
 * Parts: the AI, game/life stats, effect controller, move controller and aggro list are PartSlots (parts.json). getAi() and getAggroList() return
 * references (NullPointerException when empty); getGameStats(), getLifeStats(), getEffectController() and getMoveController() return Ptr,
 * because Java checks them for null (Equipment.java:608, Effect.java:807, SkillLearnService.java:31, CreatureController.java:540).
 * getMaster()/getActingCreature() are real covariant overrides in Summon and servants: they keep `Ptr<Creature>` (§8.2).
 *
 * @author -Nemesiss-
 */
class Creature : public VisibleObject {
	AION_MAKE_REF_FRIEND
private:
	/** Created for the owner by AIEngine.newAI(aiName, this) in postConstruct; //ai set replaces it (PartSlot OWNER, amendment §4) */
	runtime::PartSlot<gameserver::ai::AbstractAI> ai{*this};
	runtime::PartSlot<stats::container::CreatureGameStats> gameStats{*this};
	runtime::PartSlot<stats::container::CreatureLifeStats> lifeStats{*this};
	runtime::PartSlot<controllers::effect::EffectController> effectController{*this};

protected:
	runtime::PartSlot<controllers::movement::CreatureMoveController> moveController{*this};

private:
	runtime::Field<int32_t> state{};       // Java: = CreatureState.ACTIVE.getId() (constructor)
	runtime::Field<int32_t> visualState{}; // Java: = CreatureVisualState.VISIBLE.getId() (constructor)
	runtime::Field<int32_t> seeState{};    // Java: = CreatureSeeState.NORMAL.getId() (constructor)
	runtime::Field<runtime::Ref<skillengine::model::Skill>> castingSkill{};
	runtime::Field<runtime::Ref<runtime::RcConcurrentHashMap<int32_t, int64_t>>> skillCoolDowns{};
	const runtime::Ref<controllers::ObserveController> observeController;
	/** Created lazily for the owner by getTransformModel (Java: new TransformModel(this)) */
	runtime::PartSlot<TransformModel> transformModel{*this};
	/** Created for the owner by the virtual createAggroList() in postConstruct (fieldmap.toml override) */
	runtime::PartSlot<controllers::attack::AggroList> aggroList{*this};
	const runtime::Ref<runtime::Array<int8_t>> zoneTypes; // Java: = new byte[ZoneType.values().length] (constructor)
	runtime::Field<int32_t> skillNumber{};
	runtime::Field<int32_t> attackedCount{};
	const int64_t spawnTime; // Java: = System.currentTimeMillis() (constructor)

protected:
	Creature(CreateKey key, int32_t objId, std::unique_ptr<controllers::CreatureController> controller,
		runtime::Ptr<templates::spawns::SpawnTemplate> spawnTemplate, const CreatureTemplate* objectTemplate, runtime::Ptr<world::WorldPosition> position,
		bool autoReleaseObjectId);
	~Creature() override;

	/**
	 * Java constructor body after super(...): the AI (objectTemplate.getAiName(), overridden by spawnTemplate.getAiName(), NO_AI = no AI name),
	 * then aggroList = createAggroList().
	 */
	void postConstruct() override;

public:
	/** @return the move controller, null until a subclass sets it (Java checks it for null) */
	runtime::Ptr<controllers::movement::CreatureMoveController> getMoveController() const;

protected:
	/** Java `new AggroList(this)`: a new part, stored by postConstruct into the aggroList slot */
	virtual std::unique_ptr<controllers::attack::AggroList> createAggroList();

public:
	/** Narrows VisibleObject::getController (Java cast-only override) */
	controllers::CreatureController& getController() const;

	runtime::Ptr<stats::container::CreatureLifeStats> getLifeStats() const;

	void setLifeStats(std::unique_ptr<stats::container::CreatureLifeStats> lifeStats);

	runtime::Ptr<stats::container::CreatureGameStats> getGameStats() const;

	void setGameStats(std::unique_ptr<stats::container::CreatureGameStats> gameStats);

	virtual int8_t getLevel() = 0;

	runtime::Ptr<controllers::effect::EffectController> getEffectController() const;

	void setEffectController(std::unique_ptr<controllers::effect::EffectController> effectController);

	/** @throws NullPointerException if the AI was not created yet (postConstruct) */
	gameserver::ai::AbstractAI& getAi() const;

	/**
	 * C++ only (handlers-and-porting-plan.md §1.7, amendment §4): `//ai set` replaces the AI (Java replaced the final field reflectively, Ai.java).
	 * The previous AI stays with the creature until it is destroyed (PartSlot RetireTo::OWNER), since tasks it scheduled may still use it.
	 */
	void replaceAi(std::unique_ptr<gameserver::ai::AbstractAI> ai);

	bool isDead();

	/** @return True if the creature is a flag (symbol on map) */
	virtual bool isFlag();

	bool isCasting();

	/** Set current casting skill or null when skill ends */
	virtual void setCasting(runtime::Ptr<skillengine::model::Skill> castingSkill);

	int32_t getCastingSkillId();

	runtime::Ptr<skillengine::model::Skill> getCastingSkill() const { return castingSkill.get(); }

	bool isCastingItemSkill();

	/**
	 * @return The factor (in percent) scaling the chance to have the current cast interrupted by incoming damage. Players are always at 100, npcs
	 *         take it from their template.
	 */
	virtual int32_t getCancelLevel();

	int32_t getSkillNumber() const { return skillNumber.get(); }

	void setSkillNumber(int32_t value) { skillNumber.set(value); }

	int32_t getAttackedCount() const { return attackedCount.get(); }

	void incrementAttackedCount();

	void clearAttackedCount();

	/** All abnormal effects are checked that disable movements */
	virtual bool canPerformMove();

private:
	bool canUseSkillInMove();

public:
	/** All abnormal effects are checked that disable attack */
	bool canAttack();

	int32_t getState() const { return state.get(); }

	/** Sets the given state while keeping all present ones */
	void setState(gameobjects::state::CreatureState state);

	/** Sets the given state. If {@code replace} is true, previous states will be completely replaced. */
	void setState(gameobjects::state::CreatureState state, bool replace);

	/** @param state taken usually from templates */
	void setState(int32_t value) { state.set(value); }

	void unsetState(gameobjects::state::CreatureState state);

	bool isInState(gameobjects::state::CreatureState state);

	int32_t getVisualState() const { return visualState.get(); }

	void setVisualState(gameobjects::state::CreatureVisualState visualState);

	void unsetVisualState(gameobjects::state::CreatureVisualState visualState);

	bool isInVisualState(gameobjects::state::CreatureVisualState visualState);

	bool isInAnyHide();

	virtual int32_t getSeeState() { return seeState.get(); }

	void setSeeState(gameobjects::state::CreatureSeeState seeState);

	void unsetSeeState(gameobjects::state::CreatureSeeState seeState);

	bool isInSeeState(gameobjects::state::CreatureSeeState seeState);

	/** Creates the TransformModel part lazily (Java: new TransformModel(this)) */
	TransformModel& getTransformModel();

	void endTransformation();

	bool isTransformed();

	/** Java final */
	controllers::attack::AggroList& getAggroList() const;

	runtime::Ptr<controllers::ObserveController> getObserveController() const { return observeController; }

	virtual bool isEnemy(Creature& creature);

	virtual bool isEnemyFrom(Creature& creature);

	virtual bool isEnemyFrom(player::Player& player);

	virtual bool isEnemyFrom(Npc& npc);

	/** @return TribeClass.GENERAL; std::optional because Npc returns Java null for templates without a tribe */
	virtual std::optional<TribeClass> getTribe();

	virtual TribeClass getBaseTribe();

	bool canSee(runtime::Ptr<VisibleObject> object) override;

	/** @return NpcObjectType.NORMAL */
	virtual NpcObjectType getNpcObjectType();

	/**
	 * For summons and different kind of servants<br>
	 * it will return currently acting player.<br>
	 * This method is used for duel and enemy relations,<br>
	 * rewards<br>
	 *
	 * @return Master of this creature or self
	 */
	virtual runtime::Ptr<Creature> getMaster();

	/**
	 * For summons it will return summon object and for <br>
	 * servants - player object.<br>
	 * Used to find attackable target for npcs.<br>
	 *
	 * @return acting master - player in case of servants
	 */
	virtual runtime::Ptr<Creature> getActingCreature();

	virtual bool isSkillDisabled(const skillengine::model::SkillTemplate* template_);

	int64_t getSkillCoolDown(int32_t cooldownId);

	void setSkillCoolDown(int32_t cooldownId, int64_t time);

	/** @return the live cooldown map, null until the first setSkillCoolDown */
	runtime::Ptr<runtime::RcConcurrentHashMap<int32_t, int64_t>> getSkillCoolDowns() const { return skillCoolDowns.get(); }

	void removeSkillCoolDown(int32_t cooldownId);

	/** @return True if this creature can not receive any damage. */
	virtual bool isInvulnerable();

	virtual templates::item::ItemAttackType getAttackType();

	/** Creature is flying (FLY or GLIDE states) */
	virtual bool isFlying();

	virtual bool isInFlyingState();

	virtual bool isPvpTarget(Creature& creature);

	/**
	 * @return All zones the the creature currently is in (even if not currently spawned, so make sure to check isSpawned yourself if needed).
	 */
	std::vector<runtime::Ptr<world::zone::ZoneInstance>> findZones();

	void revalidateZones();

	bool isInsideZone(const world::zone::ZoneName* zoneName);

	bool isInsideItemUseZone(const world::zone::ZoneName* zoneName);

	/** Increments an internal counter for the given zone type, to support nested zones */
	void setInsideZoneType(templates::zone::ZoneType zoneType);

	/** Decrements an internal counter for the given zone type, to support nested zones */
	void unsetInsideZoneType(templates::zone::ZoneType zoneType);

	/** @return True, if the creature is inside one or more zones of the specified type. */
	bool isInsideZoneType(templates::zone::ZoneType zoneType);

	bool isInsidePvPZone();

	virtual Race getRace();

	virtual int32_t getSkillCooldown(const skillengine::model::SkillTemplate* template_);

	int64_t getMillisSinceSpawn();

	bool isNewSpawn();

	virtual bool isRaidMonster();

	bool isWorldRaidMonster();

	/** @return null (Npc overrides it) */
	virtual runtime::Ptr<items::NpcEquippedGear> getOverrideEquipment();
};

} // namespace aion::gameserver::model::gameobjects
