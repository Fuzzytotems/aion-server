#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/controllers/movement/fwd.h"
#include "aion/gameserver/dataholders/loadingutils/adapters/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/items/fwd.h"
#include "aion/gameserver/model/skill/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/model/templates/npc/fwd.h"
#include "aion/gameserver/model/templates/npcskill/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/skillengine/effect/fwd.h"
#include "aion/gameserver/spawnengine/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * This class is a base class for all in-game NPCs, what includes: monsters and npcs that player can talk to (aka Citizens)
 * <p>
 * Hub header (docs/design/hub-headers.md). Java `new Npc(...)` is `VisibleObject::create<Npc>(...)`. The constructor stores the members
 * (objectId from IDFactory, a new WorldPosition of the spawn's world, the skill list part); postConstruct() runs the rest of the Java
 * constructor body in Java order: `getController().setOwner(*this)`, the NpcMoveController and the virtual setupStatContainers() (§10.1).
 * The skill list is a `const std::unique_ptr` part and therefore created by the constructor, after the base classes (Java creates it after the
 * move controller; its constructor only reads the npc template).
 * Java null for the overridden CreatureType and the SummonOwner is `std::optional` (overrideNpcType(null), DialogService.java:299); the master
 * name distinguishes null from a name (SummonedObject.java:48) and is returned as `std::optional<std::string>` (empty = null, Field<std::string>).
 *
 * @author Luno
 */
class Npc : public Creature {
	AION_MAKE_REF_FRIEND
private:
	const std::unique_ptr<skill::NpcSkillList> skillList;
	runtime::LinkedList<runtime::Ref<skill::NpcSkillEntry>> queuedSkills{};
	runtime::Field<runtime::Ref<spawnengine::WalkerGroup>> walkerGroup{};
	runtime::Field<std::string> masterName{};
	runtime::Field<int32_t> creatorId{0};
	runtime::Field<std::optional<CreatureType>> overriddenType{}; // fieldmap: Java null disables the override (overrideNpcType(null)), std::optional
	// fieldmap: NpcEquippedGear is static data owned by NpcTemplate (unique_ptr in the xmlgen shell); an override is a shared immutable value
	runtime::Field<std::shared_ptr<const items::NpcEquippedGear>> overriddenEquipment{};
	runtime::Field<std::optional<skillengine::effect::SummonOwner>> summonOwner{}; // fieldmap: Java null = no summon owner (DialogService.java:299)

protected:
	/** Java: Objects.requireNonNull(objectTemplate) (NullPointerException), then super(IDFactory.nextId(), ..., new WorldPosition(worldId), true) */
	Npc(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate,
		const templates::npc::NpcTemplate* objectTemplate);
	~Npc() override;

	/** Java constructor body after super(...): controller.setOwner(this), moveController = new NpcMoveController(this), setupStatContainers() */
	void postConstruct() override;

public:
	/** Narrows Creature::getMoveController (Java cast-only override) */
	runtime::Ptr<controllers::movement::NpcMoveController> getMoveController() const;

protected:
	virtual void setupStatContainers();

public:
	/** Narrows VisibleObject::getObjectTemplate (Java cast-only override) */
	const templates::npc::NpcTemplate* getObjectTemplate() const;

	int32_t getNpcId();

	int8_t getLevel() override;

	templates::npc::AbyssNpcType getAbyssNpcType();

	templates::npc::NpcRating getRating();

	templates::npc::NpcRank getRank();

	templates::npc::NpcTemplateType getNpcTemplateType();

	int32_t getHpGauge();

	/** Narrows Creature::getLifeStats (Java cast-only override) */
	runtime::Ptr<stats::container::NpcLifeStats> getLifeStats() const;

	/** Narrows Creature::getGameStats (Java cast-only override) */
	runtime::Ptr<stats::container::NpcGameStats> getGameStats() const;

	/** Narrows Creature::getController (Java cast-only override) */
	controllers::NpcController& getController() const;

	templates::item::ItemAttackType getAttackType() override;

	/** @return the skill list; Ptr because Java checks it for null (VisibleObjectSpawner.java:206), although the final field is never null */
	runtime::Ptr<skill::NpcSkillList> getSkillList() const;

	runtime::Ptr<skill::NpcSkillEntry> getNextQueuedSkill();

	bool hasQueuedSkill(const std::function<bool(skill::NpcSkillEntry&)>& filter);

	void removeNextQueuedSkill(skill::NpcSkillEntry& skill);

	void clearQueuedSkills();

	void queueSkill(skill::NpcSkillEntry& skill);

	void queueSkill(int32_t skillId, int32_t level);

	void queueSkill(int32_t skillId, int32_t level, int32_t nextSkillTime);

	void queueSkill(int32_t skillId, int32_t level, int32_t nextSkillTime, templates::npcskill::NpcSkillTargetAttribute npcSkillTargetAttribute);

	bool isWalker();

	bool isRandomWalker();

	bool isPathWalker();

	/** @return the tribe, std::nullopt for NPC templates without a tribe (Java null: 42 npc_templates, DropRegistrationService.java:291) */
	std::optional<TribeClass> getTribe() override;

	TribeClass getBaseTribe() override;

	int32_t getAggroRange();

	int32_t getShortAggroRange();

	int32_t getAggroAngle();

	/** @return True if the npc is within 1m of it's spawn location */
	bool isAtSpawnLocation();

	bool isEnemy(Creature& creature) override;

	bool isEnemyFrom(Creature& creature) override;

	bool isEnemyFrom(Npc& npc) override;

	bool isEnemyFrom(player::Player& player) override;

	virtual CreatureType getType(Creature& creature);

private:
	CreatureType getRelationBasedType(Creature& creature);

public:
	/** Sets a constant type and broadcasts it, if the npc is spawned. Set to null, to disable it. */
	void overrideNpcType(std::optional<CreatureType> newType);

	/** @return distance to spawn location */
	double getDistanceToSpawnLocation();

	int32_t getSeeState() override;

	/** @return Name of the Master (Java null: std::nullopt) */
	virtual std::optional<std::string> getMasterName();

	void setMasterName(std::string_view masterName);

	/** @return UniqueId of the VisibleObject which created this Npc (could be player or house) */
	virtual int32_t getCreatorId();

	void setCreatorId(int32_t value) { creatorId.set(value); }

	virtual runtime::Ptr<VisibleObject> getCreator();

	void setWalkerGroup(runtime::Ptr<spawnengine::WalkerGroup> wg);

	runtime::Ptr<spawnengine::WalkerGroup> getWalkerGroup() const { return walkerGroup.get(); }

	bool isFlag() override;

	bool isRaidMonster() override;

	int32_t getCancelLevel() override;

	bool isBoss();

	bool hasStatic();

	Race getRace() override;

	/** @return True if this npc sells items. */
	bool canSell();

	/** @return True if this npc buys items. */
	bool canBuy();

	/** @return True if this npc trades items for other items. */
	bool canTradeIn();

	/** @return True if this npc buys specific items. */
	bool canPurchase();

	templates::npc::GroupDropType getGroupDrop();

	void overrideEquipmentList(const dataholders::loadingutils::adapters::NpcEquipmentList* v);

	/** @return the overridden gear or the template's gear (a non-owning shared_ptr to the immortal template data), null without both */
	std::shared_ptr<const items::NpcEquippedGear> getOverrideEquipment() override;

	void setSummonOwner(std::optional<skillengine::effect::SummonOwner> value) { summonOwner.set(value); }

	std::optional<skillengine::effect::SummonOwner> getSummonOwner() const { return summonOwner.get(); }
};

} // namespace aion::gameserver::model::gameobjects
