#!/usr/bin/env python3
"""Generate assets/lang_config.h from the X-macro list in lang_home.cc."""

import argparse
import re
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, help="Path to lang_home.cc")
    parser.add_argument("--output", required=True, help="Path to lang_config.h")
    parser.add_argument("--default-code", default="zh-CN")
    args = parser.parse_args()

    source = Path(args.source).read_text(encoding="utf-8")
    keys = []
    seen = set()
    for name in re.findall(r"^\s*X\(([A-Za-z0-9_]+)\s*,", source, re.MULTILINE):
        if name not in seen:
            seen.add(name)
            keys.append(name)
    if not keys:
        raise SystemExit(f"No language keys found in {args.source}")

    lines = [
        "// Auto-generated from lang_home.cc by scripts/gen_lang_config_from_home.py.",
        "// Keep this file in sync when adding or removing an X(...) entry.",
        "#pragma once",
        "",
        f'#ifndef LANG_DEFAULT_CODE',
        f'#define LANG_DEFAULT_CODE "{args.default_code}"',
        "#endif",
        "",
        "namespace Lang {",
        "extern const char* CODE;",
        "bool SetLanguage(const char* code, bool persist = true);",
        "const char* Current();",
        "void InitFromNvs();",
        "",
        "namespace Strings {",
    ]
    lines.extend(f"extern const char* {key};" for key in keys)
    lines.extend(["}", "}", ""])

    output = Path(args.output)
    output.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {output} ({len(keys)} keys)")


if __name__ == "__main__":
    main()
