"""Check six prepared Dechorder Mozart cases with diagnostics off and on.

python tests/check_mozart_parity.py --workspace C:/path/to/Dechorder
    --plugin-dir C:/path/to/seventh-diagnostics-v9 --output-dir C:/path/to/parity
Requires the frozen analysis/mozart-reference/runs/production-v1 baseline and
its three prepared WAVs. Never renders new audio or edits the application.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--plugin-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    workspace, plugin, output = args.workspace.resolve(), args.plugin_dir.resolve(), args.output_dir.resolve()
    baseline = workspace / "analysis/mozart-reference/runs/production-v1"
    report = json.loads((baseline / "report.json").read_text())
    host = workspace / ".tools/sonic-annotator/sonic-annotator-win64/sonic-annotator.exe"
    assert digest(host) == report["engine"]["host_sha256"], "baseline host changed"
    assert len(report["results"]) == 6, "expected three fixtures times two profiles"
    output.mkdir(parents=True, exist_ok=True)
    summary = {"baseline_report_sha256": digest(baseline / "report.json"),
        "baseline_dll_sha256": report["engine"]["dll_sha256"],
        "diagnostic_dll_sha256": digest(plugin / "nnls-chroma.dll"),
        "host_sha256": digest(host), "script_sha256": digest(Path(__file__)), "cases": []}
    for case in report["results"]:
        fixture, profile = case["fixture"], case["profile"]
        audio = workspace / "analysis/mozart-reference/prepared" / (fixture + ".wav")
        original = baseline / fixture / profile / "raw.csv"
        base_transform = baseline / (profile + ".n3")
        assert digest(audio) == case["audio_sha256"], "frozen audio changed"
        assert digest(original) == case["raw_csv_sha256"], "baseline raw CSV changed"
        assert digest(base_transform) == report["transforms"][profile], "baseline transform changed"
        for diagnostics in (0, 1):
            target = output / fixture / profile / ("diagnostics-" + str(diagnostics))
            target.mkdir(parents=True, exist_ok=True)
            text = re.sub(r'\s*vamp:plugin_version\s+[^;]+;', '', base_transform.read_text()).rstrip()
            assert text.endswith('.')
            text = text[:-1] + (';\n vamp:parameter_binding [ vamp:parameter [ vamp:identifier "diagnostics" ] ; '
                f'vamp:value "{diagnostics}"^^xsd:float ] .\n')
            transform = target / "transform.n3"
            transform.write_text(text)
            result = subprocess.run([str(host), "-t", str(transform), str(audio), "-w", "csv", "--csv-stdout",
                "--csv-omit-filename", "--csv-fill-ends", "--csv-end-times"], cwd=workspace,
                env=dict(os.environ, VAMP_PATH=str(plugin)), capture_output=True, timeout=120,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
            (target / "engine.log").write_bytes(result.stderr)
            result.check_returncode()
            csv_path = target / "raw.csv"
            csv_path.write_bytes(result.stdout)
            equal = result.stdout == original.read_bytes()
            summary["cases"].append({"fixture": fixture, "profile": profile, "diagnostics": diagnostics,
                "audio_sha256": digest(audio), "transform_sha256": digest(transform),
                "raw_csv_sha256": digest(csv_path), "raw_csv_path": str(csv_path), "byte_identical": equal})
    summary["all_byte_identical"] = all(case["byte_identical"] for case in summary["cases"])
    (output / "report.json").write_text(json.dumps(summary, indent=2) + "\n")
    assert summary["all_byte_identical"], "diagnostic build changed chord output"
    print("All 12 outputs are byte-identical: 3 Mozart WAVs x 2 profiles x diagnostics off/on.")
    print(output / "report.json")


if __name__ == "__main__":
    main()
