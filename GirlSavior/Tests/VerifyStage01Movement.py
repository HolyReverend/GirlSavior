"""Validate captured PIE behavior against Stage 1 design, not the velocity algorithm."""
import csv
import json
import math
from pathlib import Path

root = Path(__file__).resolve().parents[1]
saved = root / "Saved" / "Stage1"
results = {}
failures = []

def check(condition, label):
    if not condition:
        failures.append(label)

def vector(row, prefix):
    return row[prefix + "_x"], row[prefix + "_y"]

def distance(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])

basis = math.sqrt(.5)
forward = (basis, basis)
right = (-basis, basis)
for fps in (60, 30):
    with (saved / f"Movement{fps}.csv").open(encoding="utf-8-sig") as stream:
        rows = [{k: (v if k == "phase" else float(v)) for k, v in r.items()}
                for r in csv.DictReader(stream)]
    phases = {name: [r for r in rows if r["phase"] == name] for name in dict.fromkeys(r["phase"] for r in rows)}
    intervals = [r["dt"] for r in rows]
    measured = {}
    for name, target, deadline in (
        ("start", (forward[0]*500, forward[1]*500), .10),
        ("reverse", (-forward[0]*500, -forward[1]*500), .10),
        ("stop", (0, 0), .08),
    ):
        samples = phases[name]
        reached = next((r for r in samples if distance(vector(r, "velocity"), target) <= 1), None)
        check(reached is not None, f"{fps} {name}: target vector never reached")
        elapsed = reached["elapsed"] if reached else None
        measured[name] = elapsed
        tolerance = max(r["dt"] for r in samples)
        check(elapsed is not None and elapsed <= deadline + tolerance + 1e-5,
              f"{fps} {name}: {elapsed} exceeded {deadline} + {tolerance}")
        if name in ("start", "stop"):
            check(elapsed is not None and elapsed >= deadline - tolerance - 1e-5,
                  f"{fps} {name}: instantaneous response lost required inertia")
            first_speed = math.hypot(*vector(samples[0], "velocity"))
            check(1 < first_speed < 499, f"{fps} {name}: first update must retain a partial velocity")
        # First eligible movement update must already respond.
        previous = (0, 0) if name == "start" else (
            (forward[0]*500, forward[1]*500) if name == "reverse"
            else (-forward[0]*500, -forward[1]*500))
        check(distance(vector(samples[0], "velocity"), previous) > 1,
              f"{fps} {name}: no first-update velocity response")
    if fps == 60:
        expected = {
            "start": ((0, 1), (forward[0]*500, forward[1]*500)),
            "reverse": ((0, -1), (-forward[0]*500, -forward[1]*500)),
            "right": ((1, 0), (right[0]*500, right[1]*500)),
            "left": ((-1, 0), (-right[0]*500, -right[1]*500)),
            "diagonal": ((basis, basis), (0, 500)),
            "cancel_forward": ((1, 0), (right[0]*500, right[1]*500)),
            "cancel_both": ((0, 0), (0, 0)),
        }
        for name, (intent, velocity) in expected.items():
            check(all(distance(vector(r, "input"), intent) < .001 for r in phases[name]),
                  f"60 {name}: evaluated signed input")
            check(distance(vector(phases[name][-1], "velocity"), velocity) <= 1,
                  f"60 {name}: final full velocity vector")
        slide = phases["slide"]
        contact = [r for r in slide if r["elapsed"] > 1.2]
        fpos = lambda r: r["position_x"]*basis + r["position_y"]*basis
        rpos = lambda r: -r["position_x"]*basis + r["position_y"]*basis
        # Authored wall front is f=380; capsule radius 34 leaves center <=346.
        check(all(fpos(r) <= 347 for r in slide + phases["head_on"]), "60 collision: wall penetration")
        check(rpos(contact[-1]) - rpos(contact[0]) > 100, "60 slide: no tangential displacement at wall")
        check(math.hypot(*vector(phases["head_on"][-1], "velocity")) <= 1,
              "60 head-on: did not stop at contact")
        check(340 <= fpos(phases["head_on"][-1]) <= 347, "60 head-on: did not reach wall")
        check(sum(a*b for a,b in zip(vector(phases["escape"][0], "velocity"), forward)) < -1,
              "60 escape: no first-update response away from wall")
        check(all(distance(vector(r, "input"), (0, 0)) < .001 for r in phases["flush"]),
              "60 flush: stale intent")
    results[str(fps)] = {"timings_seconds": measured, "dt_min": min(intervals), "dt_max": max(intervals),
                         "samples": len(rows)}
results["failures"] = failures
results["passed"] = not failures
(saved / "Acceptance.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
print(json.dumps(results, indent=2))
raise SystemExit(1 if failures else 0)