"""python -m tools.gen.questgen (or python -m questgen from cpp/tools/gen): see cli.py"""
import sys

from .cli import main

sys.exit(main())
