"""irjson: the JSON outputs and the report of the static data generator.

- xmlmodel.json (format aion-xmlmodel, version 1): the complete model. Its class/property subset is the minimal IR the independent V1
  XSD cross-check reads (tools/oracle/README.md); everything else documents the C++ mapping for reviews and later tools.
- staticdata-classes.json (format aion-staticdata-classes, version 1): the reachable K1 classes and enums with their runtime-mutable
  fields, for tools/gen/fieldmap.py (runtime-architecture.md §3.1: K1 classes are exempt from K4 rules and escape analysis).
- xmlgen-report.md: classification, implicit bindings, policy use, initializer notes, unreachable JAXB types, enums with behaviour and
  the port checklist (non-trivial Java methods per behaviour class).
"""
from __future__ import annotations

import json

import jaxb
from jaxb import short_fqn


def rel_location(node):
    return jaxb.loc(node).replace('com/aionemu/gameserver/', '')


def property_json(mem, p):
    d = {
        'javaName': p.java_name,
        'node': 'element' if p.node == 'choice' else p.node,
        'xmlName': p.xml_name,
        'required': p.required,
    }
    if p.node == 'element' and p.value_type.kind == 'class' and not p.idref:
        d['typeFqn'] = p.value_type.fqn
    if p.class_adapter:
        d['typeFqn'] = jaxb.long_fqn(mem.adapter['value_type'])
    if p.wrapper:
        d['wrapperName'] = p.wrapper
    if p.node == 'choice':
        d['choices'] = [{'xmlName': n, 'typeFqn': t} for n, t in p.choices]
    d.update({
        'kind': p.kind,
        'source': p.source,
        'implicit': p.implicit,
        'javaType': str(p.java_type),
        'valueType': p.value_type.to_json(),
        'collection': p.collection,
        'xmlList': p.xml_list,
        'xmlId': p.xml_id,
        'idref': p.idref,
        'adapter': p.adapter or p.class_adapter,
        'initializer': p.initializer.to_json(),
        'access': p.access,
        'location': rel_location(p.decl),
        'runtimeMutable': p.runtime_mutable,
        'storageByPointer': p.storage_by_pointer,
        'optionalString': p.optional_string,
        'lenientEnum': p.lenient_enum,
    })
    if p.getter or p.setter:
        d['getter'] = p.getter
        d['setter'] = p.setter
    d['cpp'] = {'name': mem.name or None, 'type': mem.type or None, 'init': mem.init or None, 'access': mem.access, 'binding': mem.bind}
    return d


def xmlmodel(cm):
    classes = []
    for cc in sorted(cm.classes.values(), key=lambda c: c.fqn):
        c = cc.model
        if cc.container:
            continue
        d = {
            'fqn': c.fqn,
            'binaryName': c.td.binary_name,
            'superclass': c.superclass,
            'xmlTypeName': c.xml_type_name,
            'xmlRootElement': c.xml_root_element,
            'xmlTransient': c.xml_transient,
            'abstract': c.abstract,
            'accessorType': c.accessor_type,
            'location': rel_location(c.td),
            'kind': cc.kind,
            'behaviourReasons': cc.reasons,
            'cpp': {'qualifiedName': cc.qualified, 'header': cc.header, 'memberBlock': cc.inc or None, 'prelude': cc.prelude or None,
                    'nested': cc.nested_in_cpp, 'create': cc.create},
            'afterUnmarshal': c.hook is not None,
            'hookOwner': cc.hook_owner.fqn if cc.hook_owner is not None else None,
            'choiceBase': c.choice_base,
            'polymorphicRoot': cc.polymorphic_root,
            'subclasses': c.subclasses,
            'requiredAttributes': cc.required_attributes,
            'requiredElements': cc.required_elements,
            'denied': [{'field': n, 'reason': r} for n, r in c.denied],
            'properties': [property_json(mem, mem.prop) for mem in cc.members if mem.prop is not None],
            'accessors': [{'name': a.java.name, 'kind': a.kind, 'member': a.member.name, 'cpp': a.code} for a in cc.accessors],
            'nonTrivialMethods': [{'name': m.name, 'kind': m.kind, 'location': rel_location(m)} for m in cc.non_trivial_methods],
        }
        classes.append(d)
    enums = []
    for e in sorted(cm.enums.values(), key=lambda e: e.fqn):
        enums.append({
            'fqn': e.fqn,
            'xmlTypeName': e.model.xml_type_name,
            'external': e.model.external,
            'cpp': {'qualifiedName': e.qualified, 'header': e.header, 'underlying': e.underlying},
            'constants': [{'name': j, 'cpp': c, 'xml': x} for j, c, x in e.constants],
            'hasMethods': e.model.has_methods,
            'hasConstructorArgs': e.model.has_constructor_args,
            'hasConstantBodies': e.model.has_constant_bodies,
            'hasFields': e.model.has_fields,
            'location': e.model.location.replace('com/aionemu/gameserver/', ''),
        })
    return {
        'format': 'aion-xmlmodel',
        'version': 1,
        'generator': 'tools/xmlgen',
        'roots': cm.policy.roots,
        'classes': classes,
        'enums': enums,
        'unreachableJaxbTypes': cm.model.unreachable,
    }


def staticdata_classes(cm):
    out = []
    for cc in sorted(cm.classes.values(), key=lambda c: c.fqn):
        if cc.container:
            continue
        out.append({
            'fqn': cc.fqn,
            'binaryName': cc.model.td.binary_name,
            'kind': cc.kind,
            'cppQualifiedName': cc.qualified,
            'header': cc.header,
            'runtimeMutable': [{'field': m.java_field, 'cppType': m.type} for m in cc.members if m.return_kind == 'field'],
        })
    for e in sorted(cm.enums.values(), key=lambda e: e.fqn):
        out.append({'fqn': e.fqn, 'binaryName': e.model.td.binary_name if e.model.td is not None else e.fqn, 'kind': 'enum',
                    'cppQualifiedName': e.qualified, 'header': e.header, 'runtimeMutable': []})
    return {'format': 'aion-staticdata-classes', 'version': 1, 'classes': out}


def dumps(doc):
    return json.dumps(doc, indent='\t', ensure_ascii=False, sort_keys=False) + '\n'


def null_checked_strings(cm):
    """'Class.field (methods)' for non-optional std::string members whose Java field is compared with null in a method of its class"""
    out = []
    for cc in sorted(cm.classes.values(), key=lambda c: c.fqn):
        if cc.container:
            continue
        strings = {mem.java_field for mem in cc.members if mem.prop is not None and mem.type == 'std::string'}
        if not strings:
            continue
        found = {}
        for meth in cc.model.td.methods:
            if meth.body is None:
                continue
            t = meth.body.texts()
            for i, tok in enumerate(t):
                if tok not in ('==', '!='):
                    continue
                left = t[i - 1] if i > 0 else ''
                right = t[i + 1] if i + 1 < len(t) else ''
                name = left if right == 'null' else (right if left == 'null' else None)
                if name in strings and not (right == 'null' and i >= 3 and t[i - 2] == '.' and t[i - 3] != 'this'):
                    found.setdefault(name, set()).add(meth.name)
        for name in sorted(found):
            out.append(f'{short_fqn(cc.fqn)}.{name} ({", ".join(sorted(found[name]))})')
    return out


def report(cm, files):
    m = cm.model
    classes = [cc for cc in cm.classes.values() if not cc.container]
    data = sorted((cc for cc in classes if cc.is_data), key=lambda c: c.fqn)
    behaviour = sorted((cc for cc in classes if not cc.is_data), key=lambda c: c.fqn)
    props = [mem.prop for cc in classes for mem in cc.members if mem.prop is not None]
    lines = ['# xmlgen report', '', 'Generated by `python cpp/tools/xmlgen/xmlgen.py generate` from the Java sources (docs/design/static-data.md §1.5).', '',
             '## Summary', '', '| Item | Count |', '|---|---|',
             f'| Reachable classes | {len(classes)} |', f'| Data-only classes (generated headers) | {len(data)} |',
             f'| Behaviour classes (hand-written, generated member blocks) | {len(behaviour)} |', f'| Enums | {len(cm.enums)} |',
             f'| Bound properties | {len(props)} |', f'| afterUnmarshal hooks | {sum(1 for cc in classes if cc.model.hook is not None)} |',
             f'| @XmlElements factories | {sum(len(cc.factories) for cc in classes)} |',
             f'| Generated files | {len(files)} |', '']
    kinds = {}
    for p in props:
        kinds[p.kind] = kinds.get(p.kind, 0) + 1
    lines += ['## Property kinds', '', '| Kind | Count |', '|---|---|'] + [f'| {k} | {v} |' for k, v in sorted(kinds.items())] + ['']
    lines += ['## Implicit bindings (no JAXB annotation, bound by the accessor type)', '', '| Property | XML | Type |', '|---|---|---|']
    for p in sorted((p for p in props if p.implicit), key=lambda p: p.key):
        lines.append(f'| {short_fqn(p.key)} | <{p.xml_name}> | {p.java_type} |')
    lines += ['', '### Denied implicit fields (xmlgen.toml [deny_implicit])', '']
    for cc in classes:
        for name, reason in cc.model.denied:
            lines.append(f'- {short_fqn(cc.fqn)}.{name}: {reason}')
    lines += ['', '## Boxed fields with an initializer (plain C++ values, never null)', '']
    lines += [f'- {short_fqn(k)}' for k in sorted(cm.optional_boxed_with_initializer)] or ['- none']
    lines += ['', '## String fields with a literal default', '']
    lines += [f'- {short_fqn(k)}' for k in sorted(cm.string_literal_defaults)] or ['- none']
    lines += ['', '## String members compared with null in Java code', '',
              'Absent String attributes/elements are empty std::strings. Where Java distinguishes null from "", check the data (census EMPTY',
              'flag) and use xmlgen.toml [optional_strings] or keep the distinction in the hand-written code.', '']
    lines += [f'- {x}' for x in null_checked_strings(cm)] or ['- none']
    lines += ['', '## Mapped initializers (xmlgen.toml [initializers])', '']
    for p in sorted((p for p in props if p.initializer.kind == 'config'), key=lambda p: p.key):
        lines.append(f'- {short_fqn(p.key)}: Java `{p.initializer.text}` -> C++ `{p.initializer.cpp or "(default)"}` ({p.initializer.reason})')
    lines += ['', '## Runtime-mutable and pointer-stored properties', '']
    for p in sorted((p for p in props if p.runtime_mutable or p.storage_by_pointer), key=lambda p: p.key):
        lines.append(f'- {short_fqn(p.key)}: ' + ', '.join(x for x, f in (('runtime_mutable', p.runtime_mutable), ('storage_by_pointer', p.storage_by_pointer)) if f))
    lines += ['', '## Lenient enum properties (xmlgen.toml [lenient_enums]: unknown constants become null like JAXB)', '']
    lines += [f'- {short_fqn(p.key)}: {cm.policy.lenient_enums[p.key]["reason"]}' for p in sorted((p for p in props if p.lenient_enum), key=lambda p: p.key)] or ['- none']
    lines += ['', '## Members renamed (a Java method or nested type has the field name)', '']
    lines += [f'- {x}' for x in sorted(cm.renamed_members)] or ['- none']
    lines += ['', '## @XmlElements entries of abstract classes (JAXB cannot instantiate them; not in the factories)', '']
    lines += [f'- {x}' for x in sorted(cm.abstract_choices)] or ['- none']
    lines += ['', '## Unreachable JAXB-annotated types (review: a missing root?)', '']
    lines += [f'- {short_fqn(x)}' for x in m.unreachable] or ['- none']
    lines += ['', '## Enums with constructor data or methods (hand-written companion headers)', '']
    for e in sorted(cm.enums.values(), key=lambda e: e.fqn):
        if e.model.has_methods or e.model.has_constructor_args or e.model.has_fields:
            flags = [x for x, f in (('methods', e.model.has_methods), ('constructor arguments', e.model.has_constructor_args),
                                    ('fields', e.model.has_fields), ('constant bodies', e.model.has_constant_bodies)) if f]
            lines.append(f'- {short_fqn(e.fqn)} ({", ".join(flags)})')
    escaped = [(e, j, c) for e in cm.enums.values() for j, c, _ in e.constants if j != c]
    lines += ['', '## Escaped enum constants (C++ keywords or platform macros)', '']
    lines += [f'- {short_fqn(e.fqn)}.{j} -> {c}' for e, j, c in sorted(escaped, key=lambda x: (x[0].fqn, x[1]))] or ['- none']
    lines += ['', '## Data-only classes', '']
    lines += [f'- {short_fqn(cc.fqn)} -> `{cc.header}`' for cc in data]
    lines += ['', '## Behaviour classes and port checklist', '',
              'Each hand-written class includes its member block; methods listed here have no generated C++ yet.', '']
    for cc in behaviour:
        lines.append(f'### {short_fqn(cc.fqn)}')
        lines.append('')
        lines.append(f'- header `{cc.header}`, member block `{cc.inc}`')
        lines.append(f'- reasons: {"; ".join(cc.reasons)}')
        if cc.non_trivial_methods:
            lines.append('- methods: ' + ', '.join(f'{m.name}' + ('()' if m.kind == 'method' else ' (constructor)') for m in cc.non_trivial_methods))
        lines.append('')
    unused = cm.policy.unused_entries()
    lines += ['## Unused xmlgen.toml entries', '']
    lines += [f'- {u}' for u in unused] or ['- none']
    return '\n'.join(lines) + '\n'
