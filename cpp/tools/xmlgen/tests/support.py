"""Helpers of the xmlgen tests: in-memory Java fixture trees and generator runs."""
from __future__ import annotations

import os
import sys
import textwrap

HERE = os.path.dirname(os.path.abspath(__file__))
XMLGEN = os.path.dirname(HERE)
TOOLS = os.path.dirname(XMLGEN)
CPP = os.path.dirname(TOOLS)
REPO = os.path.dirname(CPP)
for p in (XMLGEN, TOOLS):
    if p not in sys.path:
        sys.path.insert(0, p)

from gen import javasrc  # noqa: E402

import cppmodel  # noqa: E402
import jaxb  # noqa: E402
import xmlgen  # noqa: E402

JAVA_SRC = os.path.join(REPO, 'game-server', 'src')
GENERATED = os.path.join(CPP, 'game-server', 'generated')
ORACLE = os.path.join(TOOLS, 'oracle')
PREFIX = 'com/aionemu/gameserver/'

IMPORTS = '''import java.util.*;
import java.time.LocalDateTime;
import javax.xml.bind.Unmarshaller;
import javax.xml.bind.annotation.*;
import javax.xml.bind.annotation.adapters.XmlJavaTypeAdapter;
import com.aionemu.gameserver.model.*;
'''


def index_of(sources):
    """ProjectIndex over {relative path under com/aionemu/gameserver: Java source}; package and imports are added when missing"""
    index = javasrc.ProjectIndex()
    for rel, text in sorted(sources.items()):
        text = textwrap.dedent(text)
        package = 'com.aionemu.gameserver.' + '.'.join(rel.split('/')[:-1]) if '/' in rel else 'com.aionemu.gameserver'
        if not text.lstrip().startswith('package'):
            text = f'package {package};\n{IMPORTS}\n{text}'
        cu = javasrc.parse_source(text, PREFIX + rel)
        cu.root = 'fixture'
        cu.relpath = PREFIX + rel
        index.add_unit(cu)
    return index


def policy_doc(roots, **tables):
    doc = {'roots': roots}
    doc.update(tables)
    return doc


def model_of(sources, roots, **tables):
    return jaxb.Model(index_of(sources), jaxb.Policy.from_toml(policy_doc(roots, **tables))).build()


def generate(sources, roots, **tables):
    """(CppModel, files) of a fixture tree"""
    return xmlgen.build_from(index_of(sources), policy_doc(roots, **tables))
