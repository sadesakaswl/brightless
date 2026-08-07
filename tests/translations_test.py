#!/usr/bin/env python3

from collections import Counter
from pathlib import Path
import json
import re
import sys
import xml.etree.ElementTree as ET

if len(sys.argv) != 3:
    raise SystemExit(f"usage: {sys.argv[0]} QML_DIR TRANSLATIONS_DIR")

qml_dir, translations_dir = map(Path, sys.argv[1:])
string_pattern = re.compile(r'\bqsTr\("((?:\\.|[^"\\])*)"\)')
placeholder_pattern = re.compile(r"%(?:L?\d+|n)")
required = {
    path.stem: {
        json.loads(f'"{match.group(1)}"')
        for match in string_pattern.finditer(path.read_text())
    }
    for path in qml_dir.glob("*.qml")
}

errors = []
catalogs = sorted(translations_dir.glob("*.ts"))
if not catalogs:
    errors.append("no translation catalogs found")

for catalog in catalogs:
    contexts = {}
    for context in ET.parse(catalog).getroot().findall("context"):
        messages = {}
        for message in context.findall("message"):
            source = message.findtext("source", "")
            translation = message.find("translation")
            kind = "" if translation is None else translation.get("type", "")
            text = "" if translation is None else "".join(translation.itertext()).strip()
            if kind in {"vanished", "obsolete"}:
                continue
            messages[source] = text
            if kind == "unfinished" or not text:
                errors.append(f"{catalog.name}: unfinished translation: {source}")
            if Counter(placeholder_pattern.findall(source)) != Counter(
                placeholder_pattern.findall(text)
            ):
                errors.append(f"{catalog.name}: placeholder mismatch: {source}")
        contexts[context.findtext("name", "")] = messages

    for context, sources in required.items():
        for source in sources:
            if source not in contexts.get(context, {}):
                errors.append(f"{catalog.name}: missing [{context}]: {source}")

if errors:
    raise SystemExit("\n".join(errors))

print(
    f"Verified {sum(map(len, required.values()))} QML strings "
    f"across {len(catalogs)} translation catalogs."
)
