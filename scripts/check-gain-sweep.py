"""Compare captured click energy across deliberate app-gain settings, without normalization."""
import csv
import json
import math
from pathlib import Path
import sys
from statistics import median

folder = Path(sys.argv[1])
data = json.loads((folder / "analysis.json").read_text())
if data["failures"]:
    raise SystemExit("Resolve capture failures before comparing gain")
cases = {case["case"]: case for case in data["cases"]}
levels = list(csv.DictReader((folder / "levels.csv").open()))
summary = {}
endpoints = set()
for name, gain in [("gain100", 100), ("gain200", 200), ("gain400", 400), ("return200", 200)]:
    case = cases[name]
    logged = [row for row in levels if row["case"] == name]
    if not logged or any(int(r["control_pct"]) != gain or int(r["requested_pct"]) != gain
                         or abs(float(r["session_scalar"])-.15) > .001 or int(r["session_muted"]) for r in logged):
        raise SystemExit(f"{name}: missing/unexpected gain telemetry")
    endpoints.update((r["endpoint_scalar"], r["endpoint_muted"]) for r in logged)
    summary[name] = dict(clicks=case["clicks"], energy=median(c["energy"] for c in case["per_click"]),
                         spread_db=case["energy_spread_db"], spectrum_error=case["max_relative_spectral_error"])
if len(endpoints) != 1:
    raise SystemExit("Endpoint volume or mute changed during the comparison")
for case in summary.values():
    case["gain_db_vs_100"] = 10*math.log10(case["energy"]/summary["gain100"]["energy"])
if not (summary["gain400"]["energy"] > summary["gain200"]["energy"] > summary["gain100"]["energy"]):
    raise SystemExit("Increasing Pulse gain did not increase captured energy")
if abs(summary["gain200"]["gain_db_vs_100"]-summary["return200"]["gain_db_vs_100"]) > .05:
    raise SystemExit("Returning to 200% failed to reproduce its level")
print(json.dumps(summary, indent=2))
