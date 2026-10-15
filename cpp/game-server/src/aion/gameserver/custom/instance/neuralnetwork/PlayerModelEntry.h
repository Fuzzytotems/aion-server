#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/custom/instance/neuralnetwork/fwd.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::custom::instance::neuralnetwork {

/**
 * One recorded action of a player (or of the boss) in the custom instance: the skill used and the state of both sides (D12, owner
 * 2026-10-13).
 * <p>
 * C++: the recorded values are const (Java never writes them after a constructor); both constructors delegate to one that takes them as a
 * Values record.
 *
 * @author Jo
 */
class PlayerModelEntry : public runtime::RefCounted, public model::gameobjects::Persistable {
	AION_MAKE_REF_FRIEND
private:
	/** C++: the recorded values, in Java's field order */
	struct Values {
		commons::database::Timestamp timestamp{};
		int32_t skillID = 0;
		int32_t playerID = 0;
		int32_t playerClassID = 0;
		float playerHPpercentage = 0, playerMPpercentage = 0;
		bool playerIsRooted = false, playerIsSilenced = false, playerIsBound = false, playerIsStunned = false, playerIsAetherhold = false;
		int32_t playerBuffCount = 0, playerDebuffCount = 0;
		bool playerIsShielded = false;
		float targetHPpercentage = 0, targetMPpercentage = 0;
		bool targetFocusesPlayer = false;
		float distance = 0;
		bool targetIsRooted = false, targetIsSilenced = false, targetIsBound = false, targetIsStunned = false, targetIsAetherhold = false;
		int32_t targetBuffCount = 0, targetDebuffCount = 0;
		bool targetIsShielded = false;
	};

	runtime::Field<Persistable::PersistentState> persistentState{};
	const commons::database::Timestamp timestamp{};
	const float timeCDdone{};
	const int32_t skillID{};
	const int32_t playerID{};
	const int32_t playerClassID{};
	const float playerHPpercentage{};
	const float playerMPpercentage{};
	const bool playerIsRooted{};
	const bool playerIsSilenced{};
	const bool playerIsBound{};
	const bool playerIsStunned{};
	const bool playerIsAetherhold{};
	const int32_t playerBuffCount{};
	const int32_t playerDebuffCount{};
	const bool playerIsShielded{};
	const float targetHPpercentage{};
	const float targetMPpercentage{};
	const bool targetFocusesPlayer{};
	const float distance{};
	const bool targetIsRooted{};
	const bool targetIsSilenced{};
	const bool targetIsBound{};
	const bool targetIsStunned{};
	const bool targetIsAetherhold{};
	const int32_t targetBuffCount{};
	const int32_t targetDebuffCount{};
	const bool targetIsShielded{};

	PlayerModelEntry(const Values& values, model::gameobjects::Persistable_PersistentState state);
	static Values liveValues(model::gameobjects::Creature& playerOrBoss, int32_t skillID, runtime::Ptr<model::gameobjects::Creature> target);
protected:
	PlayerModelEntry(model::gameobjects::Creature& playerOrBoss, int32_t skillID, runtime::Ptr<model::gameobjects::Creature> target);
	PlayerModelEntry(int32_t playerID, commons::database::Timestamp timestamp, int32_t skillID, int32_t playerClassID,
		float playerHPpercentage, float playerMPpercentage, bool playerIsRooted, bool playerIsSilenced, bool playerIsBound, bool playerIsStunned,
		bool playerIsAetherhold, int32_t playerBuffCount, int32_t playerDebuffCount, bool playerIsShielded, float targetHPpercentage,
		float targetMPpercentage, bool targetFocusesPlayer, float distance, bool targetIsRooted, bool targetIsSilenced, bool targetIsBound,
		bool targetIsStunned, bool targetIsAetherhold, int32_t targetBuffCount, int32_t targetDebuffCount, bool targetIsShielded);
public:
	/** Java: the live constructor */
	static runtime::Ref<PlayerModelEntry> create(model::gameobjects::Creature& playerOrBoss, int32_t skillID,
		runtime::Ptr<model::gameobjects::Creature> target);
	/** Java: the constructor from persistence */
	static runtime::Ref<PlayerModelEntry> create(int32_t playerID, commons::database::Timestamp timestamp, int32_t skillID,
		int32_t playerClassID, float playerHPpercentage, float playerMPpercentage, bool playerIsRooted, bool playerIsSilenced, bool playerIsBound,
		bool playerIsStunned, bool playerIsAetherhold, int32_t playerBuffCount, int32_t playerDebuffCount, bool playerIsShielded,
		float targetHPpercentage, float targetMPpercentage, bool targetFocusesPlayer, float distance, bool targetIsRooted, bool targetIsSilenced,
		bool targetIsBound, bool targetIsStunned, bool targetIsAetherhold, int32_t targetBuffCount, int32_t targetDebuffCount,
		bool targetIsShielded);
	std::vector<double> toStateInputArray(const std::vector<int32_t>& skillSet, int32_t previousSkillID) const;
	std::vector<double> toActionOutputArray(const std::vector<int32_t>& skillSet) const;
	commons::database::Timestamp getTimestamp() const { return this->timestamp; }
	float getTimeCDdone() const { return this->timeCDdone; }
	int32_t getSkillID() const { return this->skillID; }
	int32_t getPlayerID() const { return this->playerID; }
	int32_t getPlayerClassID() const { return this->playerClassID; }
	float getPlayerHPpercentage() const { return this->playerHPpercentage; }
	float getPlayerMPpercentage() const { return this->playerMPpercentage; }
	bool isPlayerRooted() const { return this->playerIsRooted; }
	bool isPlayerSilenced() const { return this->playerIsSilenced; }
	bool isPlayerBound() const { return this->playerIsBound; }
	bool isPlayerStunned() const { return this->playerIsStunned; }
	bool isPlayerAetherhold() const { return this->playerIsAetherhold; }
	int32_t getPlayerBuffCount() const { return this->playerBuffCount; }
	int32_t getPlayerDebuffCount() const { return this->playerDebuffCount; }
	bool isPlayerIsShielded() const { return this->playerIsShielded; }
	float getTargetHPpercentage() const { return this->targetHPpercentage; }
	float getTargetMPpercentage() const { return this->targetMPpercentage; }
	bool isTargetFocusesPlayer() const { return this->targetFocusesPlayer; }
	float getDistance() const { return this->distance; }
	bool isTargetRooted() const { return this->targetIsRooted; }
	bool isTargetSilenced() const { return this->targetIsSilenced; }
	bool isTargetBound() const { return this->targetIsBound; }
	bool isTargetStunned() const { return this->targetIsStunned; }
	bool isTargetAetherhold() const { return this->targetIsAetherhold; }
	int32_t getTargetBuffCount() const { return this->targetBuffCount; }
	int32_t getTargetDebuffCount() const { return this->targetDebuffCount; }
	bool isTargetIsShielded() const { return this->targetIsShielded; }
	model::gameobjects::Persistable_PersistentState getPersistentState() override { return this->persistentState.get(); }
	void setPersistentState(model::gameobjects::Persistable_PersistentState state) override { this->persistentState.set(state); }
protected:
	~PlayerModelEntry() override;
};

} // namespace aion::gameserver::custom::instance::neuralnetwork
