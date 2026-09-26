"""Independent static data oracles (static-data.md section 4, V1-V4).

Nothing in this package imports the xmlgen generator: the import resolution, the holder count rules and the XSD reader are written
from the Java sources (XmlMerger, XmlUtil, StaticData and the 92 holder classes) and from the data itself.
"""


class OracleError(Exception):
	"""Anything unexpected: data that Java would reject, a construct the oracle does not model, or a Java startup crash."""
