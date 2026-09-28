#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `Player.macros`), created with create(). The record Macro
 * is a nested RefCounted value class (fieldmap K3).
 *
 * @author Aquanox, nrg
 */
class Macros : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	/** Java record Macro(int id, String xml) */
	class Macro : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	private:
		const int32_t id_;
		const std::string xml_;

	protected:
		Macro(int32_t id, std::string_view xml);
		~Macro() override;

	public:
		/** Java: new Macro(id, xml) (canonical record constructor) */
		static runtime::Ref<Macro> create(int32_t id, std::string_view xml);

		int32_t id() const { return id_; }

		std::string xml() const { return xml_; }

		/** Java record equals (all components) */
		bool equals(const Macro& obj) const;

		/** Java record hashCode */
		int32_t hashCode() const;
	};

private:
	runtime::HashMap<int32_t, runtime::Ref<Macros::Macro>> macrosById{AION_LOCK_CLASS(Macros::macrosById)};

protected:
	Macros();
	~Macros() override;

public:
	/** Java: new Macros() */
	static runtime::Ref<Macros> create();

	/** synchronized */
	std::vector<runtime::Ptr<Macros::Macro>> getAll();

	/** synchronized. @throws IllegalArgumentException for an invalid macro ID */
	bool add(int32_t macroId, std::string_view macroXML);

	/** synchronized */
	bool remove(int32_t macroId);
};

} // namespace aion::gameserver::model::gameobjects::player
