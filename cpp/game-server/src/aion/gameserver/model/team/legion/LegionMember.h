#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/legion/LegionRank.h"
#include "aion/gameserver/model/team/legion/fwd.h"

namespace aion::gameserver::model::team::legion {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `Player.legionMember`, `LegionService.legionMemberById`),
 * created with create().
 *
 * @author Simple
 */
class LegionMember : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const int32_t objectId;
	const runtime::Ref<Legion> legion;
	runtime::Field<LegionRank> rank{LegionRank::VOLUNTEER};
	runtime::Field<std::string> nickname{""};
	runtime::Field<std::string> selfIntro{""};
	runtime::Field<int32_t> challengeScore{};
	// --- below are cached player fields (not in legion_members table) ---
	runtime::Field<std::string> name{};
	runtime::Field<PlayerClass> playerClass{};
	runtime::Field<int32_t> level{};
	runtime::Field<int32_t> worldId{};
	runtime::Field<int32_t> lastOnlineEpochSeconds{};
	runtime::Field<bool> online{false};

protected:
	/** This constructor will set the legion and object id */
	LegionMember(int32_t objectId, Legion& legion);
	~LegionMember() override;

public:
	/** Java: new LegionMember(objectId, legion) */
	static runtime::Ref<LegionMember> create(int32_t objectId, Legion& legion);

	int32_t getObjectId() const { return objectId; }

	/** @return the legion */
	runtime::Ptr<Legion> getLegion() const { return legion; }

	/** @param value the rank to set */
	void setRank(LegionRank value) { rank.set(value); }

	/** @return the rank */
	LegionRank getRank() const { return rank.get(); }

	bool isBrigadeGeneral();

	/** @param value the nickname to set */
	void setNickname(std::string_view value) { nickname.set(std::string(value)); }

	/** @return the nickname */
	std::string getNickname() const { return nickname.get(); }

	/** @param value the selfIntro to set */
	void setSelfIntro(std::string_view value) { selfIntro.set(std::string(value)); }

	/** @return the selfIntro */
	std::string getSelfIntro() const { return selfIntro.get(); }

	int32_t getChallengeScore() const { return challengeScore.get(); }

	void setChallengeScore(int32_t value) { challengeScore.set(value); }

	void increaseChallengeScore(int32_t amount);

	void setPlayerData(gameobjects::player::Player& player);

	void setPlayerData(gameobjects::player::PlayerCommonData& playerCommonData);

	std::string getName() const { return name.get(); }

	PlayerClass getPlayerClass() const { return playerClass.get(); }

	int32_t getLevel() const { return level.get(); }

	int32_t getWorldId() const { return worldId.get(); }

	int32_t getLastOnlineEpochSeconds() const { return lastOnlineEpochSeconds.get(); }

	bool isOnline() const { return online.get(); }

	bool hasRights(LegionPermissionsMask permissions);
};

} // namespace aion::gameserver::model::team::legion
