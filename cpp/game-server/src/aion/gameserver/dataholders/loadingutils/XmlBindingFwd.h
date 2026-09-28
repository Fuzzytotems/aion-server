#pragma once

/**
 * Forward declarations of the static data XML binder runtime (docs/design/static-data.md §2).
 *
 * This is the only binder header that hand-written template headers include (before their class body), so the generated member block
 * (`X.xml.inc`) can declare `friend struct ::aion::gameserver::xml::XmlBinding<X>;` and hooks can be declared as
 * `void afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& parent);`. It pulls in no pugixml and no runtime headers.
 * The full contract for generated code is documented in XmlBinding.h.
 */
namespace aion::gameserver::xml {

/** Binder of one class, specialized by generated code (never defined for the primary template). See XmlBinding.h. */
template <class T>
struct XmlBinding;

/** Name table of a generated enum (EnumTraits.h). */
template <class E>
struct EnumTraits;

class BindContext;
class LoadContext;
struct XmlParent;
class ChildCounts;

} // namespace aion::gameserver::xml
