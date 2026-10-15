#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/model/Expirable.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/pet/fwd.h"
#include "aion/gameserver/services/toypet/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `PetList.pets`, `Pet.commonData`), created with create().
 * Expirable is held by Ref, so retain()/release() forward to RefCounted (§9.2). The constructor reads the pet templates (DataManager), so it
 * stays `AION_UNPORTED` after the member initializers. The birthday and despawn time are nullable (getBirthday() checks birthday for null).
 *
 * @author ATracer
 */
class PetCommonData : public runtime::RefCounted, public Expirable {
	AION_MAKE_REF_FRIEND
private:
	const int32_t objectId;
	const int32_t templateId;
	const int32_t masterObjectId;
	runtime::Field<int32_t> decoration{};
	runtime::Field<std::string> name{};
	// fieldmap.toml: Java compares it with null (PetCommonData.java getBirthday), hub-headers.md §6 nullable Timestamp field
	runtime::Field<std::optional<commons::database::Timestamp>> birthday{};

public:
	runtime::Field<runtime::Ref<services::toypet::PetFeedProgress>> feedProgress{};
	runtime::Field<runtime::Ref<templates::pet::PetDopingBag>> dopingBag{};

private:
	runtime::Field<bool> cancelFeed{false};
	runtime::Field<int64_t> refeedTime{};
	runtime::Field<int64_t> startMoodTime{};
	runtime::Field<int32_t> shuggleCounter{};
	runtime::Field<int32_t> lastSentPoints{};
	runtime::Field<int64_t> moodCdStarted{};
	runtime::Field<int64_t> giftCdStarted{};
	const int32_t expireTime;
	// fieldmap.toml: null until the DAO sets it (Timestamp column), hub-headers.md §6 nullable Timestamp field
	runtime::Field<std::optional<commons::database::Timestamp>> despawnTime{};
	runtime::Field<bool> isLooting_{false};
	runtime::Field<bool> isSelling_{false};
	runtime::Field<runtime::FutureRef> refeedTask{};

protected:
	PetCommonData(int32_t objectId, int32_t templateId, int32_t masterObjectId, int32_t expireTime);
	~PetCommonData() override;

public:
	/** Java: new PetCommonData(objectId, templateId, masterObjectId, expireTime) */
	static runtime::Ref<PetCommonData> create(int32_t objectId, int32_t templateId, int32_t masterObjectId, int32_t expireTime);

	/** C++ only: Ref<Expirable> retains this object (hub-headers.md §9.2). */
	void retain() const noexcept override { runtime::RefCounted::retain(); }

	void release() const noexcept override { runtime::RefCounted::release(); }

	int32_t getObjectId() const { return objectId; }

	int32_t getMasterObjectId() const { return masterObjectId; }

	int32_t getDecoration() const { return decoration.get(); }

	void setDecoration(int32_t value) { decoration.set(value); }

	std::string getName() const { return name.get(); }

	void setName(std::string_view value) { name.set(std::string(value)); }

	int32_t getTemplateId() const { return templateId; }

	int32_t getBirthday();

	std::optional<commons::database::Timestamp> getBirthdayTimestamp() const { return birthday.get(); }

	void setBirthday(std::optional<commons::database::Timestamp> value) { birthday.set(value); }

	int64_t getRefeedTime() const { return refeedTime.get(); }

	void setRefeedTime(int64_t curentTime) { refeedTime.set(curentTime); }

	bool getCancelFeed() const { return cancelFeed.get(); }

	void setCancelFeed(bool value) { cancelFeed.set(value); }

	void scheduleRefeed(int64_t reFoodTime);

	void cancelRefeedTask();

	int64_t getRefeedDelay();

	/** Java final */
	int64_t getMoodStartTime() const { return startMoodTime.get(); }

	/** Java final */
	int32_t getShuggleCounter() const { return shuggleCounter.get(); }

	/** Java final */
	void setShuggleCounter(int32_t value) { shuggleCounter.set(value); }

	/** Java final */
	int32_t getMoodPoints(bool forPacket);

	/** Java final */
	int32_t getLastSentPoints() const { return lastSentPoints.get(); }

	/** Java final */
	void setLastSentPoints(int32_t points) { lastSentPoints.set(points); }

	/** Java final */
	bool increaseShuggleCounter();

	/** Java final */
	void clearMoodStatistics();

	/** Java final */
	void setStartMoodTime(int64_t value) { startMoodTime.set(value); }

	/** @return moodCdStarted */
	int64_t getMoodCdStarted() const { return moodCdStarted.get(); }

	/** @param value the moodCdStarted to set */
	void setMoodCdStarted(int64_t value) { moodCdStarted.set(value); }

	int32_t getMoodRemainingTime();

	/** @return the giftCdStarted */
	int64_t getGiftCdStarted() const { return giftCdStarted.get(); }

	/** @param value the giftCdStarted to set */
	void setGiftCdStarted(int64_t value) { giftCdStarted.set(value); }

	int32_t getGiftRemainingTime();

	/** @return the despawnTime */
	std::optional<commons::database::Timestamp> getDespawnTime() const { return despawnTime.get(); }

	/** @param value the despawnTime to set */
	void setDespawnTime(std::optional<commons::database::Timestamp> value) { despawnTime.set(value); }

	/** @return feedProgress, null if pet has no feed function */
	runtime::Ptr<services::toypet::PetFeedProgress> getFeedProgress() const { return feedProgress.get(); }

	void setIsLooting(bool value) { isLooting_.set(value); }

	bool isLooting() const { return isLooting_.get(); }

	bool isSelling() const { return isSelling_.get(); }

	void setIsSelling(bool selling) { isSelling_.set(selling); }

	/** @return the doping bag, null if the pet has no doping function */
	runtime::Ptr<templates::pet::PetDopingBag> getDopingBag() const { return dopingBag.get(); }

	int32_t getExpireTime() override { return expireTime; }

	void onExpire(Player& player) override;
};

} // namespace aion::gameserver::model::gameobjects::player
