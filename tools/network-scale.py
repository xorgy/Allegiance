#!/usr/bin/env python3
"""Run encrypted human-roster evaluations sequentially, without rendering.

Build allegiance-network-scale and allegiance-network-load first. The normal
case progression is 2x128, 4x64, 4x128, 4x256, on original multi-sector maps.
Logs, per-tick CSV and a machine-readable result remain in --output.
"""
import argparse
import csv as csv_module
import datetime
import json
import platform
import re
import signal
import subprocess
import time
from pathlib import Path


AUTHORITY = re.compile(
    r"UDP authority: (\d+) pilots, (\d+) sectors, (\d+) seconds; "
    r"work p99 ([\d.]+) ms, max ([\d.]+) ms, sim p99 ([\d.]+) ms, "
    r"capture p99 ([\d.]+) ms; (\d+) deadline misses")
TRAFFIC = re.compile(
    r"UDP traffic ([\d.]+) MiB/s down, ([\d.]+) MiB/s up; "
    r"(\d+) reports, (\d+) arena encodes, ([\d.]+) MiB shared history; "
    r"rejected (\d+), input drops (\d+)")
CLIENT = re.compile(
    r"Encrypted UDP agents: (\d+) peers, (\d+) reports \((\d+) minimum per pilot\), "
    r"(\d+) missing bases, p99 report age ([\d.]+) ms; "
    r"([\d.]+) MiB received, ([\d.]+) MiB sent")
AUTHORITY_AGE = re.compile(r"Clock-probed authority age: p99 ([\d.]+) ms")
ARENA_TRAFFIC = re.compile(r"Arena traffic MiB/s: map ([\d.]+), physical ([\d.]+), trajectories ([\d.]+), private ([\d.]+); bootstrap ([\d.]+) MiB")


def stop(process):
    if process.poll() is None:
        process.send_signal(signal.SIGINT)
    try:
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


def evaluate(args, teams, pilots):
    prefix = args.output / f"{teams}x{pilots}"
    server_log = prefix.with_name(prefix.name + "-server.log")
    csv = prefix.with_name(prefix.name + "-server.csv")
    common = ["--coast"] if args.coast else []
    map_options = ["--map", str(args.map)] + (["--large-map"] if args.large_map else [])
    processes, files, client_logs = [], [], []
    result = {"teams": teams, "pilots_per_team": pilots,
              "seconds": args.seconds, "server_log": str(server_log), "csv": str(csv)}
    try:
        file = server_log.open("w")
        files.append(file)
        server = subprocess.Popen(
            [str(args.bin / "allegiance-network-scale"), "--server", "--teams", str(teams),
             "--pilots", str(pilots), "--seconds", str(args.seconds), "--port", str(args.port),
             "--csv", str(csv)] + common + map_options, stdout=file, stderr=subprocess.STDOUT)
        processes.append(server)
        deadline = time.monotonic() + 30
        while "Waiting for" not in server_log.read_text():
            if server.poll() is not None or time.monotonic() > deadline:
                raise RuntimeError("Authority failed to reach connection setup")
            time.sleep(.05)
        for side in range(teams):
            log = prefix.with_name(prefix.name + f"-client-{side}.log")
            client_logs.append(log)
            file = log.open("w")
            files.append(file)
            processes.append(subprocess.Popen(
                [str(args.bin / "allegiance-network-load"), "--count", str(pilots),
                 "--pilots", str(pilots), "--start", str(side * pilots),
                 "--seconds", str(args.seconds + 100), "--address", f"127.0.0.1:{args.port}",
                 "--csv", str(prefix.with_name(prefix.name + f"-client-{side}.csv"))]
                + common + (["--replica"] if args.replica and side==teams-1 else []), stdout=file, stderr=subprocess.STDOUT))
        deadline = time.monotonic() + args.seconds + 120
        while server.poll() is None:
            if "UDP authority:" in server_log.read_text():
                break
            if any(p.poll() is not None for p in processes[1:]):
                raise RuntimeError("Load agent exited before evaluation completed")
            if time.monotonic() > deadline:
                raise RuntimeError("Evaluation timeout")
            time.sleep(.1)
        # Finish immediately when the measured interval ends. Teardown and
        # idle hangar time must not contaminate the client freshness samples.
        # Signal every agent before waiting for any agent's socket teardown.
        # Waiting sequentially lets later agents measure a stopped authority.
        for process in processes[1:]:
            if process.poll() is None:
                process.send_signal(signal.SIGINT)
        for process in processes[1:]:
            stop(process)
        server.wait(timeout=30)
        result["server_exit"] = server.returncode
        match = AUTHORITY.search(server_log.read_text())
        if not match:
            raise RuntimeError("Missing authority summary")
        fields = ("pilots", "sectors", "measured_seconds", "work_p99_ms", "work_max_ms",
                  "sim_p99_ms", "publication_p99_ms", "deadline_misses")
        result.update(zip(fields, (float(v) if i in range(3, 7) else int(v)
                                   for i, v in enumerate(match.groups()))))
        match = TRAFFIC.search(server_log.read_text())
        if not match:
            raise RuntimeError("Missing traffic summary")
        fields = ("down_mib_s", "up_mib_s", "reports", "arena_encodes", "history_mib",
                  "rejected", "input_drops")
        result.update(zip(fields, (float(v) if i in (0, 1, 4) else int(v)
                                   for i, v in enumerate(match.groups()))))
        match = ARENA_TRAFFIC.search(server_log.read_text())
        if match:
            result["traffic_mib_s"] = dict(zip(("map", "physical", "trajectories", "private", "bootstrap_mib"), map(float, match.groups())))
        with csv.open() as file:
            rows = list(csv_module.DictReader(file))
        if rows:
            result["population"] = {"maximum_pods": max(int(r["pods"]) for r in rows),
                                    "final_pods": int(rows[-1]["pods"]),
                                    "first_pod_tick": next((int(r["tick"]) for r in rows if int(r["pods"])), None),
                                    "peak_models": max(int(r["models"]) for r in rows)}
        result["clients"] = []
        for process, log in zip(processes[1:], client_logs):
            match = CLIENT.search(log.read_text())
            if not match:
                raise RuntimeError(f"Missing load-agent summary: {log}")
            fields = ("peers", "reports", "minimum_reports", "missing_bases",
                      "report_gap_age_p99_ms", "received_mib", "sent_mib")
            client = dict(zip(fields, (float(v) if i >= 4 else int(v)
                                       for i, v in enumerate(match.groups()))))
            client.update(exit=process.returncode, log=str(log))
            match = AUTHORITY_AGE.search(log.read_text())
            if match:
                client["authority_age_p99_ms"] = float(match.group(1))
            replica = re.search(r"IGC replica: (\d+) reports, (\d+) missing bases, (\d+) command-view stages, step p99 ([\d.]+) ms, ([\d.]+) MiB received", log.read_text())
            if replica:
                client["replica"] = dict(zip(("reports", "missing_bases", "command_view_stages", "step_p99_ms", "received_mib"),
                                              (float(v) if i>=3 else int(v) for i,v in enumerate(replica.groups()))))
            result["clients"].append(client)
        result["passed"] = (result["server_exit"] == 0 and result["deadline_misses"] == 0
                            and result["rejected"] == 0 and result["input_drops"] == 0
                            and all(c["exit"] == 0 and c["missing_bases"] == 0
                                    and c["minimum_reports"] >= args.seconds * 10 * .95
                                    and c["report_gap_age_p99_ms"] <= 180
                                    for c in result["clients"]) and
                            (not args.replica or any(c.get("replica", {}).get("reports", 0) >= args.seconds * 10 * .95
                                                      and c["replica"]["missing_bases"] == 0
                                                      and c["replica"]["command_view_stages"] == 2
                                                      for c in result["clients"])))
    except (RuntimeError, OSError, subprocess.TimeoutExpired) as error:
        result.update(passed=False, error=str(error))
    finally:
        for process in processes:
            stop(process)
        for file in files:
            file.close()
    print(json.dumps(result), flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bin", type=Path, default=Path(__file__).resolve().parents[1]
                        / "build/linux-release/src/native")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--case", action="append", choices=["2x128", "4x64", "4x128", "4x256"])
    parser.add_argument("--seconds", type=int, default=30)
    parser.add_argument("--port", type=int, default=27660)
    parser.add_argument("--coast", action="store_true")
    parser.add_argument("--map", type=int, choices=range(15), default=0,
                        help="Original map selection, e.g. 9 = HiHigher")
    parser.add_argument("--large-map", action="store_true", help="Original map's large size")
    parser.add_argument("--replica", action="store_true", help="Replace one codec agent with the real headless IGC/prediction client and exercise F2 sector switching")
    args = parser.parse_args()
    if args.seconds < 10 or not 1 <= args.port <= 65535:
        parser.error("Use at least 10 seconds and a valid UDP port")
    args.bin = args.bin.resolve()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    report = {"date": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "kernel": platform.release(), "machine": platform.machine(), "sim_hz": 60,
              "snapshot_hz": 10, "transport": "encrypted GNS UDP on fixed-capacity loopback",
              "crowding": "original behavior preserved", "firing": not args.coast,
              "map": args.map, "large_map": args.large_map,
              "real_igc_replica": args.replica,
              "client_measurement": "codec report-gap age and clock-probed authority age; optional real headless IGC client measured separately; no rendering",
              "runs": []}
    for case in args.case or ["2x128", "4x64", "4x128", "4x256"]:
        report["runs"].append(evaluate(args, *map(int, case.split("x"))))
        (args.output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    return 0 if all(r["passed"] for r in report["runs"]) else 2


if __name__ == "__main__":
    raise SystemExit(main())
