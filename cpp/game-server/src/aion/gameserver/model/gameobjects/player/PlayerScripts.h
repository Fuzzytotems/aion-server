#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `House.playerScripts`), created with create(). The
 * constructor fills the script array with empty scripts (ported). The compressed script bytes are stored as they are (runtime Array, null
 * allowed); decompressAndValidate returns null on an invalid script (`std::optional`). sendToPlayer checks the player for null.
 *
 * @author Rolandas, Neon, Sykra
 */
class PlayerScripts : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	// max number of scripts a player can have
	static constexpr int8_t SCRIPT_LIMIT = 8;

private:
	// Java: private static final Logger log = LoggerFactory.getLogger(PlayerScripts.class) - namespace-scope logger in PlayerScripts.cpp
	const int32_t houseObjId;
	const runtime::Ref<runtime::Array<runtime::Ref<house::PlayerScript>>> scripts; // Java: = new PlayerScript[SCRIPT_LIMIT] (constructor)

protected:
	explicit PlayerScripts(int32_t houseId);
	~PlayerScripts() override;

public:
	/** Java: new PlayerScripts(houseId) */
	static runtime::Ref<PlayerScripts> create(int32_t houseId);

private:
	void fillEmptyScriptsArray();

public:
	bool set(int32_t id, runtime::Ptr<runtime::Array<int8_t>> compressedXML, int32_t uncompressedSize);

	bool set(int32_t id, runtime::Ptr<runtime::Array<int8_t>> compressedXML, int32_t uncompressedSize, bool storeInDb);

	void remove(int32_t scriptId);

	void removeAll();

	/** @return the script, null for an invalid script id */
	runtime::Ptr<house::PlayerScript> get(int32_t scriptId);

private:
	bool isInvalidScriptId(int32_t scriptId);

	std::optional<std::string> decompressAndValidate(runtime::Ptr<runtime::Array<int8_t>> compressedXML, int32_t uncompressedSize);

public:
	void sendToPlayer(runtime::Ptr<Player> player, int32_t houseAddress);
};

} // namespace aion::gameserver::model::gameobjects::player
