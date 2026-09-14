"""scaffold: hand-written stubs for behaviour classes (docs/design/static-data.md §1.5 `xmlgen scaffold`).

For a top-level behaviour class X (Java FQN, or the short form relative to com.aionemu.gameserver) it creates, only if missing:
- `X.h`: includes `X.xml.h`, defines the class (Java superclass as public base) with `#include "X.xml.inc"` as the first line of the body,
  nested behaviour classes the same way, and one comment per non-trivial Java method (the declarations to port);
- `X.cpp`: the afterUnmarshal definitions as AION_UNPORTED() stubs and every non-trivial Java method body as a block comment.
The output compiles as it is; porting replaces the comments with declarations and definitions.
"""
from __future__ import annotations

import re

import jaxb
from jaxb import XmlGenError, short_fqn

UNPORTED_HEADER = 'aion/gameserver/handlers/Unported.h'


def authors(td):
    doc = td.doc or ''
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


def class_lines(cm, cc, indent=''):
    """the class definition, followed by the out-of-class definitions of its nested behaviour classes (the member block declares them;
    a nested class may derive from its enclosing class, which is complete only after its definition)"""
    base = f' : public {cc.superclass.qualified}' if cc.superclass is not None else ''
    lines = []
    doc = f'{indent}/** Java {cc.fqn}.'
    a = authors(cc.model.td)
    if a:
        doc += f' @author {a}'
    lines.append(doc + ' */')
    lines.append(f'{indent}class {"::".join(_local_name(cc))}{base} {{')
    lines.append(f'#include "{cc.inc}"')
    lines.append(f'{indent}public:')
    setters = {mem.prop.setter for mem in cc.members if mem.bind == 'methodSetter'}
    for m in cc.non_trivial_methods:
        if m.name in setters and len(m.params) == 1:
            continue  # declared by the member block
        lines.append(f'{indent}\t// TODO port: {signature(m)}')
    lines.append(f'{indent}}};')
    for n in nested_behaviour(cm, cc):
        lines.append('')
        lines += class_lines(cm, n, indent)
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


def hook_definitions(cm, cc):
    out = []
    if cc.model.hook is not None:
        name = '::'.join(_local_name(cc))
        out += ['', f'void {name}::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {{', '\tAION_UNPORTED();']
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
            out += java_comment(m.body.text, '\t')
            out.append('}')
            continue
        out += [''] + java_comment(f'{signature(m)} {m.body.text}')
    for n in nested_behaviour(cm, cc):
        out += hook_definitions(cm, n)
    return out


def _local_name(cc):
    chain = []
    c = cc
    while c is not None:
        chain.append(c.java_name)
        c = c.outer
    return list(reversed(chain))


def scaffold(cm, names):
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
        header = ['#pragma once', '', f'#include "{cc.prelude}"', '', f'namespace {cc.namespace} {{', '']
        header += class_lines(cm, cc, '')
        header += ['', f'}} // namespace {cc.namespace}', '']
        source = [f'#include "{cc.header}"', '', f'#include "{UNPORTED_HEADER}"', '', f'namespace {cc.namespace} {{']
        source += hook_definitions(cm, cc)
        source += ['', f'}} // namespace {cc.namespace}', '']
        out.append((cc.header, '\n'.join(header)))
        out.append((cc.header[:-2] + '.cpp', '\n'.join(source)))
    return out
