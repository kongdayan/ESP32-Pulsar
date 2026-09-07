#!/usr/bin/env python3
"""把 llvm-cov 的 summary JSON 转成一行结论，并按阈值决定退出码。

用法:
    coverage_summary.py <coverage.json> [min_line_percent] [--markdown out.md]

兼容 llvm-cov（count/covered/percent）与 gcov-json（count/missing/lines）两种字段。
"""
import json
import sys


def stat(total: dict, kind: str) -> tuple:
    entry = total.get(kind, {})
    count = entry.get("count", 0)
    if "covered" in entry:
        covered = entry["covered"]
    elif "missing" in entry:
        covered = count - entry["missing"]
    else:
        covered = 0
    percent = entry.get("percent")
    if percent is None:
        percent = (100.0 * covered / count) if count else 100.0
    return count, covered, percent


def main() -> int:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if not args:
        print("usage: coverage_summary.py <coverage.json> [min_line_percent] [--markdown out.md]",
              file=sys.stderr)
        return 2

    path = args[0]
    threshold = float(args[1]) if len(args) > 1 else 90.0
    markdown = None
    if "--markdown" in sys.argv:
        markdown = sys.argv[sys.argv.index("--markdown") + 1]

    with open(path, encoding="utf-8") as fh:
        payload = json.load(fh)

    data = payload["data"]
    totals = data[0]["totals"] if isinstance(data, list) else data["totals"]

    lines = stat(totals, "lines")
    funcs = stat(totals, "functions")
    branches = stat(totals, "branches")

    print("line     %.2f%%  (%d/%d)" % (lines[2], lines[1], lines[0]))
    print("function %.2f%%  (%d/%d)" % (funcs[2], funcs[1], funcs[0]))
    print("branch   %.2f%%  (%d/%d)" % (branches[2], branches[1], branches[0]))
    print("threshold: line >= %.2f%%" % threshold)

    if markdown:
        with open(markdown, "w", encoding="utf-8") as fh:
            fh.write("### 主机侧单元测试覆盖率\n\n")
            fh.write("| 指标 | 覆盖率 | 覆盖/总数 |\n|---|---|---|\n")
            for name, s in (("行", lines), ("函数", funcs), ("分支", branches)):
                fh.write("| %s | %.2f%% | %d/%d |\n" % (name, s[2], s[1], s[0]))
            fh.write("\n阈值：行覆盖率 >= %.2f%%，%s\n" % (threshold,
                       "达标 ✅" if lines[2] + 1e-9 >= threshold else "未达标 ❌"))

    return 0 if lines[2] + 1e-9 >= threshold else 1


if __name__ == "__main__":
    sys.exit(main())
