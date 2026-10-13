#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/instance/InstanceScoreType.h"
#include "aion/gameserver/model/instance/instancescore/fwd.h"
#include "aion/gameserver/model/instance/playerreward/fwd.h"
#include "aion/gameserver/network/aion/instanceinfo/InstanceScoreWriter.h"
#include "aion/gameserver/network/aion/instanceinfo/fwd.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"

namespace aion::gameserver::network::aion::instanceinfo {

/**
 * @author Estrayl
 * <p>
 * C++: RefCounted like its base (create()). The erased base holds the score as InstanceScore; score() is the typed field of Java's generic
 * InstanceScoreWriter (the constructor takes the score class).
 */
class PvpInstanceScoreWriter : public InstanceScoreWriter {
	AION_MAKE_REF_FRIEND
private:
	static constexpr int32_t MAXIMUM_PLAYER_COUNT_PER_FACTION = 24;
	const model::instance::InstanceScoreType instanceScoreType;
	runtime::ArrayList<runtime::Ref<model::gameobjects::player::Player>> participants{AION_LOCK_CLASS(PvpInstanceScoreWriter::participants)};
	/** Java null unless created with a race (only UPDATE_FACTION_SCORE reads it, a NullPointerException without one) */
	const std::optional<model::Race> race; // fieldmap: Java null unless created with a race; a nullable enum (hub-headers.md §6)
	const int32_t objectId;
	const int32_t status;

protected:
	PvpInstanceScoreWriter(model::instance::instancescore::PvpInstanceScore& reward, model::instance::InstanceScoreType instanceScoreType);
	PvpInstanceScoreWriter(model::instance::instancescore::PvpInstanceScore& reward, model::instance::InstanceScoreType instanceScoreType,
	                       model::Race race);
	PvpInstanceScoreWriter(model::instance::instancescore::PvpInstanceScore& reward, model::instance::InstanceScoreType instanceScoreType,
	                       int32_t objectId, int32_t status);
	PvpInstanceScoreWriter(model::instance::instancescore::PvpInstanceScore& reward, model::instance::InstanceScoreType instanceScoreType,
	                       std::vector<runtime::Ref<model::gameobjects::player::Player>> participants);
	~PvpInstanceScoreWriter() override;

public:
	/** Java: new PvpInstanceScoreWriter(...) */
	static runtime::Ref<PvpInstanceScoreWriter> create(model::instance::instancescore::PvpInstanceScore& reward,
	                                                   model::instance::InstanceScoreType instanceScoreType);

	/** Java: new PvpInstanceScoreWriter(...) */
	static runtime::Ref<PvpInstanceScoreWriter> create(model::instance::instancescore::PvpInstanceScore& reward,
	                                                   model::instance::InstanceScoreType instanceScoreType, model::Race race);

	/** Java: new PvpInstanceScoreWriter(...) */
	static runtime::Ref<PvpInstanceScoreWriter> create(model::instance::instancescore::PvpInstanceScore& reward,
	                                                   model::instance::InstanceScoreType instanceScoreType, int32_t objectId, int32_t status);

	/** Java: new PvpInstanceScoreWriter(...) */
	static runtime::Ref<PvpInstanceScoreWriter> create(model::instance::instancescore::PvpInstanceScore& reward,
	                                                   model::instance::InstanceScoreType instanceScoreType,
	                                                   std::vector<runtime::Ref<model::gameobjects::player::Player>> participants);

	void writeMe(commons::utils::ByteBuffer& buf) override;

private:
	void writeShowReward(commons::utils::ByteBuffer& buf);

	void writeUpdateScoreToBuffer(commons::utils::ByteBuffer& buf);

	void writePlayerFullData(commons::utils::ByteBuffer& buf);

	void writePlayerFullData(commons::utils::ByteBuffer& buf, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players);

	void writePlayerBuffInfo(commons::utils::ByteBuffer& buf, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players);

	void writeEmptyDataToBuffer(commons::utils::ByteBuffer& buf, int32_t dataSize, int32_t missingPlayerCount);

	/** Java: the typed field instanceScore (InstanceScoreWriter<PvpInstanceScore<PvpInstancePlayerReward>>) */
	model::instance::instancescore::PvpInstanceScore& score() const;

	/** Java: instanceScore.getPlayerReward(objectId) of type PvpInstancePlayerReward (null if none) */
	runtime::Ptr<model::instance::playerreward::PvpInstancePlayerReward> rewardOf(int32_t playerObjectId) const;

	/** Java: participants.stream().filter(p -> p.getRace() == race).toList() */
	std::vector<runtime::Ptr<model::gameobjects::player::Player>> participantsOf(model::Race participantRace) const;
};

} // namespace aion::gameserver::network::aion::instanceinfo
