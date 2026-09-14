#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/CreatureTemplate.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * This class is holding base information about player, that may be used even when player itself is not online.
 * <p>
 * Hub header (docs/design/hub-headers.md). A RefCounted K4 class (a packet member of SM_GM_SHOW_PLAYER_STATUS) that Java also passes as the
 * Player's CreatureTemplate: Player's constructor hands `const CreatureTemplate*` to Creature, kept alive by the PlayerAccountData holding it.
 * getName(), getTemplateId(), getL10nId() and getBoundRadius() override VisibleObjectTemplate/L10n in Java; the xmlgen shells of those
 * bases do not declare them yet, so they are plain declarations here, already const like the template bases, and only gain `override` when
 * P4-07 adds the virtual base declarations.
 * Not immortal: although it derives StaticTemplate through CreatureTemplate, IsStaticTemplate<PlayerCommonData> is false (below), so a
 * `const PlayerCommonData*` is no TaskArg; tasks capture `Ref<PlayerCommonData>` or pin the Player. `player.getObjectTemplate()` returns this
 * object as `const VisibleObjectTemplate*`, which the type system cannot tell apart from static data: never capture it for a Player
 * (VisibleObject::getObjectTemplate).
 * The last online timestamp is null for a player that never logged in: `std::optional` (Mailbox.java:52). The name of addExp may be null.
 *
 * @author Luno, cura
 */
class PlayerCommonData : public runtime::RefCounted, public CreatureTemplate {
	AION_MAKE_REF_FRIEND
private:
	const int32_t playerObjId;
	runtime::Field<Race> race{};
	runtime::Field<std::string> name{};
	runtime::Field<PlayerClass> playerClass{};
	/** Should be changed right after character creation **/
	runtime::Field<int32_t> level{0};
	runtime::Field<int64_t> exp{0};
	runtime::Field<int64_t> expRecoverable{0};
	runtime::Field<Gender> gender{};
	runtime::Field<std::optional<commons::database::Timestamp>> lastOnline{}; // fieldmap: Java null = never online (Mailbox.java:52)
	runtime::Field<bool> online{};
	runtime::Field<std::string> note{};
	runtime::Field<int32_t> mapId{};
	runtime::Field<float> x{};
	runtime::Field<float> y{};
	runtime::Field<float> z{};
	runtime::Field<int8_t> heading{};
	runtime::Field<int32_t> questExpands{0};
	runtime::Field<int32_t> npcExpands{0};
	runtime::Field<int32_t> itemExpands{0};
	runtime::Field<int32_t> warehouseNpcExpands{0};
	runtime::Field<int32_t> warehouseBonusExpands{0};
	runtime::Field<int32_t> titleId{-1};
	runtime::Field<int32_t> bonusTitleId{-1};
	runtime::Field<int32_t> dp{0};
	runtime::Field<int32_t> mailboxLetters{};
	runtime::Field<int32_t> soulSickness{0};
	runtime::Field<bool> noExp{false};
	runtime::Field<int64_t> reposeCurrent{};
	runtime::Field<int64_t> reposeMax{};
	runtime::Field<int64_t> salvationPoint{};
	runtime::Field<int32_t> mentorFlagTime{};
	runtime::Field<int32_t> worldOwnerId{};
	runtime::Field<bool> isDaeva_{};
	runtime::Field<bool> isInEditMode_{};
	runtime::Field<const templates::BoundRadius*> boundRadius{};
	runtime::Field<int64_t> lastTransferTime{};

protected:
	explicit PlayerCommonData(int32_t objId);
	~PlayerCommonData() override;

public:
	static runtime::Ref<PlayerCommonData> create(int32_t objId);

	int32_t getPlayerObjId() const { return playerObjId; }

	int64_t getExp() const { return exp.get(); }

	int32_t getQuestExpands() const { return questExpands.get(); }

	void setQuestExpands(int32_t value) { questExpands.set(value); }

	void setNpcExpands(int32_t value) { npcExpands.set(value); }

	int32_t getNpcExpands() const { return npcExpands.get(); }

	int32_t getItemExpands() const { return itemExpands.get(); }

	void setItemExpands(int32_t value) { itemExpands.set(value); }

	int64_t getExpShown();

	int64_t getExpNeed();

	/**
	 * calculate the lost experience must be called before setexp
	 *
	 * @author Jangan
	 */
	void calculateExpLoss();

	void setRecoverableExp(int64_t value) { expRecoverable.set(value); }

	void resetRecoverableExp();

	int64_t getExpRecoverable() const { return expRecoverable.get(); }

	void addExp(int64_t value, Rates rates);

	void addExp(int64_t value, Rates rates, std::optional<std::string_view> name);

	bool isInEditMode() const { return isInEditMode_.get(); }

	void setInEditMode(bool value) { isInEditMode_.set(value); }

	bool isReadyForSalvationPoints();

	bool isReadyForReposeEnergy();

	void addReposeEnergy(int64_t add);

	void updateMaxRepose();

	void setCurrentReposeEnergy(int64_t value) { reposeCurrent.set(value); }

	int64_t getCurrentReposeEnergy() const { return reposeCurrent.get(); }

	int64_t getMaxReposeEnergy() const { return reposeMax.get(); }

	/** sets the exp and level value */
	void setExp(int64_t exp);

	void setNoExp(bool value) { noExp.set(value); }

	bool getNoExp() const { return noExp.get(); }

	/** Java final */
	Race getRace() const { return race.get(); }

	int32_t getMentorFlagTime() const { return mentorFlagTime.get(); }

	bool isHaveMentorFlag();

	void setMentorFlagTime(int32_t value) { mentorFlagTime.set(value); }

	void setRace(Race value) { race.set(value); }

	/** Java @Override of VisibleObjectTemplate.getName() (the shell does not declare it yet) */
	std::string getName() const { return name.get(); }

	void setName(std::string_view value) { name.set(std::string(value)); }

	PlayerClass getPlayerClass() const { return playerClass.get(); }

	void setPlayerClass(PlayerClass value) { playerClass.set(value); }

	bool isOnline() const { return online.get(); }

	void setOnline(bool value) { online.set(value); }

	Gender getGender() const { return gender.get(); }

	void setGender(Gender value) { gender.set(value); }

	int32_t getMapId() const { return mapId.get(); }

	void setMapId(int32_t value) { mapId.set(value); }

	float getX() const { return x.get(); }

	void setX(float value) { x.set(value); }

	float getY() const { return y.get(); }

	void setY(float value) { y.set(value); }

	float getZ() const { return z.get(); }

	void setZ(float value) { z.set(value); }

	int8_t getHeading() const { return heading.get(); }

	void setHeading(int8_t value) { heading.set(value); }

	/** @return Timestamp the player was last online. May be null */
	std::optional<commons::database::Timestamp> getLastOnline() const { return lastOnline.get(); }

	/**
	 * @return Unix timestamp the player was last online (measured in seconds since 1970-01-01T00:00:00Z). 0 if he was never online before.
	 */
	int32_t getLastOnlineEpochSeconds();

	void setLastOnline(std::optional<commons::database::Timestamp> timestamp) { lastOnline.set(timestamp); }

	int32_t getLevel() const { return level.get(); }

	/** This will only set the specified level >= 10 if the player is a daeva. */
	void setLevel(int32_t level);

	std::string getNote() const { return note.get(); }

	void setNote(std::string_view value) { note.set(std::string(value)); }

	int32_t getTitleId() const { return titleId.get(); }

	void setTitleId(int32_t value) { titleId.set(value); }

	int32_t getBonusTitleId() const { return bonusTitleId.get(); }

	void setBonusTitleId(int32_t value) { bonusTitleId.set(value); }

	/**
	 * Gets the corresponding Player for this common data. Returns null if the player is not online
	 *
	 * @return Player or null
	 */
	runtime::Ptr<Player> getPlayer();

	void addDp(int32_t dp);

	/**
	 * //TODO move to lifestats -> db save?<br>
	 * => {@link PlayerGameStats#onStatsChange()}
	 */
	void setDp(int32_t dp);

	int32_t getDp() const { return dp.get(); }

	/** Java @Override of VisibleObjectTemplate.getTemplateId() (const like the template getters; the shell does not declare it yet) */
	int32_t getTemplateId() const;

	/** Java @Override of L10n.getL10nId() (const like templates::L10n; the shell does not declare it yet) */
	int32_t getL10nId() const { return 0; }

	void setWhNpcExpands(int32_t value) { warehouseNpcExpands.set(value); }

	int32_t getWhNpcExpands() const { return warehouseNpcExpands.get(); }

	int32_t getWhBonusExpands() const { return warehouseBonusExpands.get(); }

	void setWhBonusExpands(int32_t value) { warehouseBonusExpands.set(value); }

	void setMailboxLetters(int32_t value) { mailboxLetters.set(value); }

	int32_t getMailboxLetters() const { return mailboxLetters.get(); }

	void setBoundingRadius(const templates::BoundRadius* value) { boundRadius.set(value); }

	/** Java @Override of VisibleObjectTemplate.getBoundRadius() (the shell does not declare it yet) */
	const templates::BoundRadius* getBoundRadius() const { return boundRadius.get(); }

	void setDeathCount(int32_t value) { soulSickness.set(value); }

	int32_t getDeathCount() const { return soulSickness.get(); }

	/** Value returned here means % of exp bonus. */
	int8_t getCurrentSalvationPercent();

	void addSalvationPoints(int64_t points);

	void setCurrentSalvationPoints(int64_t points) { salvationPoint.set(points); }

	void resetSalvationPoints();

	void setLastTransferTime(int64_t value) { lastTransferTime.set(value); }

	int64_t getLastTransferTime() const { return lastTransferTime.get(); }

	int32_t getWorldOwnerId() const { return worldOwnerId.get(); }

	void setWorldOwnerId(int32_t value) { worldOwnerId.set(value); }

	/** @return True, if the player has a main class and completed the ascension quest (gets updated on login and quest completion). */
	bool isDaeva() const { return isDaeva_.get(); }

	void setDaeva(bool value) { isDaeva_.set(value); }

	/** @return True, if player was promoted to daeva. False if he already has daeva status or wasn't promoted. */
	bool updateDaeva();
};

} // namespace aion::gameserver::model::gameobjects::player

/** A RefCounted, mutable object, not an immortal static data template (runtime/lifetime/RefCounted.h IsStaticTemplate, sched/TaskConcepts.h) */
template <>
struct aion::gameserver::runtime::IsStaticTemplate<aion::gameserver::model::gameobjects::player::PlayerCommonData> : std::false_type {};
