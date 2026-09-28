"""scaffold: hand-written stubs for behaviour classes (docs/design/static-data.md §1.5 `xmlgen scaffold`).

For a top-level behaviour class X (Java FQN, or the short form relative to com.aionemu.gameserver) it creates, only if missing:
- `X.h`: includes `X.xml.h`, defines the class (Java superclass as public base) with `#include "X.xml.inc"` as the first line of the body,
  the Java no-argument constructor when it is protected or private, nested behaviour classes the same way, and (with comments) one comment
  per non-trivial Java method (the declarations to port);
- `X.cpp`: the afterUnmarshal and annotated setter definitions as AION_UNPORTED() stubs and (with comments) every non-trivial Java method
  body as a block comment; not written when it would contain nothing but its include.
The output compiles as it is; porting replaces the comments with declarations and definitions.

`scaffold_all` writes the shells of spine step S0a (handlers-and-porting-plan.md §2.5, decision 4): every top-level behaviour class without
comments, plus the hand-written target classes of class-level adapters (xmlgen.toml [adapters] with value_type: a constructor taking the
bound value and the methods the unmarshal statement calls with the LoadContext). Shells contain only what the generated member blocks and
binders need; the port checklist of xmlgen-report.md lists the methods to add.
"""
from __future__ import annotations

import re

import cppmodel
import jaxb
from jaxb import XmlGenError, short_fqn

UNPORTED_HEADER = 'aion/gameserver/runtime/base/Unported.h'
LOAD_CONTEXT = '::aion::gameserver::xml::LoadContext'
FWD_HEADER = 'aion/gameserver/dataholders/loadingutils/XmlBindingFwd.h'


def authors(td):
    doc = td.doc or '' if td is not None else ''
    found = re.findall(r'@author\s+([^\n*]+)', doc)
    return ', '.join(a.strip() for a in found)


def signature(m):
    mods = ' '.join(x for x in m.modifiers)
    params = ', '.join(f'{p.type}{"..." if p.varargs else ""} {p.name}' for p in m.params)
    if m.kind == 'constructor':
        return f'{mods} {m.name}({params})'.strip()
    tparams = f'<{", ".join(t.name for t in m.type_params)}> ' if m.type_params else ''
    return f'{mods} {tparams}{m.return_type} {m.name}({params})'.strip()


def java_comment(text, indent=''):
    text = text.replace('*/', '* /')
    return [f'{indent}/* Java:'] + [f'{indent}{line}' for line in text.split('\n')] + [f'{indent}*/']


def class_lines(cm, cc, indent='', comments=True):
    """the class definition, followed by the out-of-class definitions of its nested behaviour classes (the member block declares them;
    a nested class may derive from its enclosing class, which is complete only after its definition)"""
    # a hierarchy root derives the K1 marker StaticTemplate (included by the generated prelude), subclasses inherit it
    base = f' : public {cc.superclass.qualified if cc.superclass is not None else cppmodel.STATIC_TEMPLATE}'
    lines = []
    doc = f'{indent}/** Java {cc.fqn}.'
    a = authors(cc.model.td)
    if a:
        doc += f' @author {a}'
    lines.append(doc + ' */')
    lines.append(f'{indent}class {"::".join(_local_name(cc))}{base} {{')
    lines.append(f'#include "{cc.inc}"')
    ctor = cc.model.no_arg_constructor
    if ctor in ('protected', 'private'):
        # XmlBinding<T>::create() (a friend) constructs it; element lists of such classes store it by pointer
        lines.append(f'{indent}{ctor}:')
        lines.append(f'{indent}\t{cc.java_name}() = default; // Java: {ctor} {cc.java_name}()')
    else:
        lines.append(f'{indent}public:')
    if comments:
        if ctor in ('protected', 'private'):
            lines.append(f'{indent}public:')
        setters = {mem.prop.setter for mem in cc.members if mem.bind == 'methodSetter'}
        for m in cc.non_trivial_methods:
            if m.name in setters and len(m.params) == 1:
                continue  # declared by the member block
            lines.append(f'{indent}\t// TODO port: {signature(m)}')
    lines.append(f'{indent}}};')
    for n in nested_behaviour(cm, cc):
        lines.append('')
        lines += class_lines(cm, n, indent, comments)
    return lines


def nested_behaviour(cm, cc):
    """nested behaviour classes of cc, base classes before their subclasses, otherwise by name"""
    nested = sorted((n for n in cm.classes.values() if n.outer is cc and n.nested_in_cpp), key=lambda n: n.java_name)
    out, done = [], set()

    def visit(n):
        if n.fqn in done:
            return
        done.add(n.fqn)
        if n.superclass is not None and n.superclass in nested:
            visit(n.superclass)
        out.append(n)
    for n in nested:
        visit(n)
    return out


def hook_definitions(cm, cc, comments=True):
    out = []
    if cc.model.hook is not None:
        name = '::'.join(_local_name(cc))
        out += ['', f'void {name}::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {{', '\tAION_UNPORTED();']
        if comments:
            out += java_comment(cc.model.hook.body.text, '\t')
        out.append('}')
    setters = {mem.prop.setter: mem for mem in cc.members if mem.bind == 'methodSetter'}
    for m in cc.non_trivial_methods:
        if m.body is None:
            continue
        mem = setters.get(m.name) if len(m.params) == 1 else None
        if mem is not None:  # declared by the member block, called by the binder
            ptype = 'std::string_view' if mem.scalar_cpp == 'std::string' else mem.scalar_cpp
            out += ['', f'void {"::".join(_local_name(cc))}::{m.name}({ptype} /*{m.params[0].name}*/) {{', '\tAION_UNPORTED();']
            if comments:
                out += java_comment(m.body.text, '\t')
            out.append('}')
            continue
        if comments:
            out += [''] + java_comment(f'{signature(m)} {m.body.text}')
    for n in nested_behaviour(cm, cc):
        out += hook_definitions(cm, n, comments)
    return out


def _local_name(cc):
    chain = []
    c = cc
    while c is not None:
        chain.append(c.java_name)
        c = c.outer
    return list(reversed(chain))


def shell_files(cm, cc, comments=True):
    """[(path relative to src/, text)] of the header and source of a top-level behaviour class"""
    header = ['#pragma once', '', f'#include "{cc.prelude}"', '', f'namespace {cc.namespace} {{', '']
    header += class_lines(cm, cc, '', comments)
    header += ['', f'}} // namespace {cc.namespace}', '']
    out = [(cc.header, '\n'.join(header))]
    definitions = hook_definitions(cm, cc, comments)
    if definitions:  # a source without definitions is not needed (the porter adds it with the first method)
        source = [f'#include "{cc.header}"', '']
        if any('AION_UNPORTED' in line for line in definitions):
            source += [f'#include "{UNPORTED_HEADER}"', '']
        source += [f'namespace {cc.namespace} {{'] + definitions + ['', f'}} // namespace {cc.namespace}', '']
        out.append((cc.header[:-2] + '.cpp', '\n'.join(source)))
    return out


def scaffold(cm, names, comments=True):
    """[(path relative to src/, text)] for the requested classes"""
    out = []
    for name in names:
        fqn = jaxb.long_fqn(name)
        cc = cm.classes.get(fqn)
        if cc is None:
            raise XmlGenError(f'scaffold: {fqn} is not a reachable class')
        if cc.is_data or cc.container:
            raise XmlGenError(f'scaffold: {short_fqn(fqn)} is generated completely (data-only)')
        if cc.outer is not None:
            raise XmlGenError(f'scaffold: {short_fqn(fqn)} is nested; scaffold its outer class {short_fqn(cc.outer.fqn)}')
        out += shell_files(cm, cc, comments)
    return out


def scaffold_all(cm):
    """[(path relative to src/, text)] of the S0a shells: all top-level behaviour classes (without comments) and the class adapter targets"""
    tops = sorted((cc for cc in cm.classes.values() if not cc.is_data and not cc.container and cc.outer is None), key=lambda c: c.fqn)
    out = scaffold(cm, [cc.fqn for cc in tops], comments=False)
    for key in sorted(cm.policy.adapters):
        table = cm.policy.adapters[key]
        if table.get('value_type'):
            out += adapter_target_files(cm, key, table)
    return out


def adapter_target_files(cm, key, table):
    """The hand-written class a class-level adapter produces (NpcEquippedGear). `cpp = std::unique_ptr<X>`: a plain class with
    `explicit X(std::unique_ptr<Value> v)` storing the bound value. `cpp = ::aion::gameserver::runtime::Ref<X>`: a RefCounted class
    (hub-headers.md §10.1) with `static Ref<X> create(std::unique_ptr<Value> v)`, a protected constructor and destructor. Both get a
    `void m(LoadContext& ctx)` AION_UNPORTED stub per `o.{member}->m(c.load())` call of the unmarshal statement."""
    m = re.fullmatch(r'(std::unique_ptr|::aion::gameserver::runtime::Ref)<::((?:\w+::)*)(\w+)>', table['cpp'])
    if m is None:
        raise XmlGenError(f'xmlgen.toml [adapters] {short_fqn(key)}: cpp of a class adapter must be std::unique_ptr<::qualified::Name> or '
                          f'::aion::gameserver::runtime::Ref<::qualified::Name>')
    counted = m.group(1) != 'std::unique_ptr'
    namespace, name = m.group(2)[:-2], m.group(3)
    header = table['header']
    if any(c.header == header for c in cm.classes.values()):
        return []  # a generated or scaffolded static data class
    value = next((c for c in cm.classes.values() if c.fqn == jaxb.long_fqn(table['value_type'])), None)
    if value is None:
        raise XmlGenError(f'xmlgen.toml [adapters] {short_fqn(key)}: value_type {table["value_type"]} is not a reachable class')
    java_fqn = 'com.aionemu.gameserver.' + '.'.join(namespace.split('::')[2:] + [name])
    td = cm.model.index.lookup(java_fqn)
    calls = sorted(set(re.findall(r'o\.\{member\}->(\w+)\(c\.load\(\)\)', table.get('unmarshal', ''))))
    doc = f'/** Java {java_fqn} (shell of the class-level adapter {short_fqn(key)}: the contract of the generated binders).'
    a = authors(td)
    if a:
        doc += f' @author {a}'
    value_ptr = f'std::unique_ptr<{value.qualified}>'
    includes = [f'#include "{FWD_HEADER}"', f'#include "{value.header}"']
    if counted:
        includes += ['#include "aion/gameserver/runtime/lifetime/Ref.h"', '#include "aion/gameserver/runtime/lifetime/RefCounted.h"']
    header_lines = ['#pragma once', '', '#include <memory>', ''] + includes + ['', f'namespace {namespace} {{', '', doc + ' */',
                    '// fieldmap: shell; the other Java members arrive with the port of the class (fieldmap.py --class)']
    if counted:
        header_lines += [f'class {name} : public ::aion::gameserver::runtime::RefCounted {{', '\tAION_MAKE_REF_FRIEND', '', 'public:',
                         f'\tstatic ::aion::gameserver::runtime::Ref<{name}> create({value_ptr} v);']
    else:
        header_lines += [f'class {name} {{', 'public:', f'\texplicit {name}({value_ptr} v);']
    header_lines += [f'\tvoid {c}({LOAD_CONTEXT}& ctx);' for c in calls]
    if counted:
        header_lines += ['', 'protected:', f'\texplicit {name}({value_ptr} v);', f'\t~{name}() override;']
    header_lines += ['', 'private:', '\t// fieldmap: owns the bound adapter value (Java: a reference kept by the GC) until the port decides its lifetime',
                     f'\t{value_ptr} v;', '};', '', f'}} // namespace {namespace}', '']
    source = [f'#include "{header}"', '']
    if calls:
        source += [f'#include "{UNPORTED_HEADER}"', '']
    source += [f'namespace {namespace} {{', '']
    if counted:
        source += [f'::aion::gameserver::runtime::Ref<{name}> {name}::create({value_ptr} value) {{',
                   f'\treturn ::aion::gameserver::runtime::makeRef<{name}>(std::move(value));', '}', '']
    source += [f'{name}::{name}({value_ptr} value) : v(std::move(value)) {{', '}']
    if counted:
        source += ['', f'{name}::~{name}() = default;']
    for c in calls:
        java = [jm for jm in (td.methods if td is not None else []) if jm.name == c]
        source.append('')
        if any('synchronized' in jm.modifiers or (jm.body is not None and 'synchronized' in jm.body.texts()) for jm in java):
            source.append('// lint: L7 unported stub; Java synchronizes it, the port adds SYNCHRONIZED or documents the load-time confinement')
        source += [f'void {name}::{c}({LOAD_CONTEXT}& /*ctx*/) {{', '\tAION_UNPORTED();', '}']
    source += ['', f'}} // namespace {namespace}', '']
    return [(header, '\n'.join(header_lines)), (header[:-2] + '.cpp', '\n'.join(source))]
