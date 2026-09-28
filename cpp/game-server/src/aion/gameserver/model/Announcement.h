#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/fwd.h"

namespace aion::gameserver::model {

/**
 * This class represents an announcement
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Divinity
 */
class Announcement : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const int32_t id;
	// fieldmap.toml: null for an unrestricted announcement (getFactionEnum returns null for "ALL", AnnouncementService.java:59 tests it)
	const std::optional<Race> faction;
	const std::string announce;
	const std::string chatType;
	const int32_t delay;

protected:
	/** Constructor with the ID of announcement */
	Announcement(int32_t id, std::string_view announce, std::string_view faction, std::string_view chatType, int32_t delay);

public:
	static runtime::Ref<Announcement> create(int32_t value, std::string_view announceValue, std::string_view factionValue,
		std::string_view chatTypeValue, int32_t delayValue);

private:
	/** @return null for "ALL" */
	std::optional<Race> getFactionEnum(std::string_view faction);

public:
	/** Return the id of the announcement */
	int32_t getId() const { return this->id; }

	/** Return the announcement's text */
	std::string getAnnounce() const { return this->announce; }

	/** Return the announcement's faction (ELYOS or ASMODIANS, null if unrestricted) */
	std::optional<Race> getFaction() const { return this->faction; }

	/** Return the chatType in String mode (for the insert in database) */
	std::string getType() const { return this->chatType; }

	/** Return the chatType with the ChatType Enum */
	ChatType getChatType();

	/** Return the announcement's delay */
	int32_t getDelay() const { return this->delay; }

protected:
	~Announcement() override;
};

} // namespace aion::gameserver::model
