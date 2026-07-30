#!/usr/bin/env bash
#
# Crow logger A/B: skip-rate sweep, one side per invocation.
#
#   BENCH_LABEL=branch ./run_gate_bench.sh          # measure the checked-out side
#   ./run_gate_bench.sh --report                    # compare accumulated sides
#
# The benchmark source is carried across checkouts so both sides run identical
# benchmark code and only the crow library differs:
#
#   git checkout <branch>       && rm -rf build/release && cmake --preset release && build && BENCH_LABEL=branch  ./run_gate_bench.sh
#   git checkout main           && git checkout <branch> -- benchmarks/logger/
#                               && rm -rf build/release && cmake --preset release && build && BENCH_LABEL=main    ./run_gate_bench.sh
#   ./run_gate_bench.sh --report
#
# Every line carries a bench_tree hash of benchmarks/logger; --report refuses to
# print a delta between two sides whose hashes, governor or SMT state differ.
#
# Core allocation on this 6-physical-core box (CPUs 0-5 are distinct cores, 6-11
# are their SMT siblings):
#
#   PRIMARY  -c 0-11   8 producer threads oversubscribed across SMT siblings.
#                      The realistic case for a logger sharing a box with an app.
#   CONTROL  -c 0-5    physical cores only, no sibling contention.
#
# Both are reported. Neither is discarded.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${BUILD:-$ROOT/build/release}"
BIN="$BUILD/benchmarks/logger"
OUT="${OUT:-$ROOT/benchmark_results/logger}"
REPS="${REPS:-3}"
LABEL="${BENCH_LABEL:-unlabelled}"
SCENARIO="${SCENARIO:-}"   # substring filter, for debugging a single row

GATE_BIN="$BIN/Menagerie.Benchmarks.Crow.GateBenchmark"
TERM_BIN="$BIN/Menagerie.Benchmarks.Crow.TerminationBenchmark"
ABSEIL_BIN="$BIN/Menagerie.Benchmarks.Abseil.FileBenchmark"

CPUS_PRIMARY="0-11"
CPUS_CONTROL="0-5"

# ---------------------------------------------------------------- report mode
if [[ "${1:-}" == "--report" ]]; then
    python3 - "$OUT/raw.jsonl" <<'PY'
import json, statistics, sys, collections

rows = []
for line in open(sys.argv[1]):
    line = line.strip()
    if line:
        rows.append(json.loads(line))
if not rows:
    sys.exit("no rows in raw.jsonl")

labels = sorted({r.get("label", "?") for r in rows})
print(f"labels present: {', '.join(labels)}")

# Environment guard. Comparing two sides measured under different governors or
# SMT settings is not a comparison, so refuse rather than quietly mislead.
def env_of(label):
    s = {(r.get("bench_tree", "?"), r.get("governor", "?"), r.get("smt", "?"))
         for r in rows if r.get("label") == label}
    return s

blocked = False
for label in labels:
    envs = env_of(label)
    if len(envs) > 1:
        print(f"!! {label} spans multiple environments: {envs}")
        blocked = True
if len(labels) >= 2:
    a, b = labels[0], labels[-1]
    ea, eb = env_of(a).pop(), env_of(b).pop()
    for i, what in enumerate(("bench_tree", "governor", "smt")):
        if ea[i] != eb[i]:
            print(f"!! {what} differs between {a} ({ea[i]}) and {b} ({eb[i]}) -- deltas suppressed")
            blocked = True

def key(r):
    p = r.get("params", {})
    return (r["scenario"], r["variant"], json.dumps(p, sort_keys=True), r["threads"], r.get("layout", "?"))

grouped = collections.defaultdict(lambda: collections.defaultdict(list))
for r in rows:
    if r.get("ok", True):
        grouped[key(r)][r.get("label", "?")].append(r["attempted_per_sec"])

# Noise floor: the control scenario carries no logger, so any delta it shows is
# machine drift plus code layout. Nothing smaller than this is claimed as real.
noise = 0.0
for k, per_label in grouped.items():
    if k[0] != "control":
        continue
    if len(per_label) >= 2:
        meds = [statistics.median(v) for v in per_label.values()]
        noise = max(noise, abs(max(meds) - min(meds)) / max(meds))
print(f"noise floor from control rows: {noise*100:.1f}%")
if blocked:
    print("\nDELTAS SUPPRESSED -- see the environment warnings above.\n")

# "main" is the baseline when present; the other label is the subject under test.
base_label = "main" if "main" in labels else labels[0]
subj_label = next((l for l in labels if l != base_label), base_label)
print(f"baseline={base_label} subject={subj_label} (positive delta = {subj_label} faster)")

hdr = f"{'scenario':38s} {'thr':>3s} {'layout':>7s}"
for label in labels:
    hdr += f" {label:>14s}"
hdr += f" {'delta':>9s}"
print("\n" + hdr)
print("-" * len(hdr))

headroom = {}
for k in sorted(grouped):
    scenario, variant, params, threads, layout = k
    per_label = grouped[k]
    name = f"{scenario}/{variant}" if variant else scenario
    p = json.loads(params)
    for tag in ("skip_rate", "drop_rate", "wait", "sinks", "pool_size", "health_check_interval_ms", "churn_hz"):
        if tag in p:
            name += f" {tag}={p[tag]}"
    line = f"{name:38s} {threads:3d} {layout:>7s}"
    meds = {}
    for label in labels:
        if label in per_label:
            m = statistics.median(per_label[label])
            meds[label] = m
            line += f" {m:14,.0f}"
        else:
            line += f" {'-':>14s}"
    if len(meds) >= 2 and not blocked:
        # Direction is explicit, never an artefact of alphabetical label order:
        # positive means the subject is faster than the baseline.
        if base_label in meds and subj_label in meds:
            d = (meds[subj_label] - meds[base_label]) / meds[base_label]
            line += "  within noise" if abs(d) < noise else f" {d*100:+8.1f}%"
            headroom[(scenario, p.get("skip_rate") or p.get("drop_rate"))] = d
    print(line)

# The gate is level-only, so prefix-rejected events still pay full format+publish.
# This number is what a prefix-aware gate would have to be worth building.
lvl = headroom.get(("level_sweep", "1.00"))
pfx = headroom.get(("prefix_sweep", "1.00"))
if lvl is not None and pfx is not None:
    print(f"\nprefix-gate headroom (level_sweep@1.00 - prefix_sweep@1.00): {(lvl - pfx)*100:+.1f} pp")
PY
    exit 0
fi

# ---------------------------------------------------------------- measure mode
command -v taskset >/dev/null || { echo "taskset not found"; exit 1; }
[[ -x "$GATE_BIN" ]] || { echo "missing $GATE_BIN -- build the release preset first"; exit 1; }

mkdir -p "$OUT/logs"

# Output must not land on tmpfs. /tmp on this box is tmpfs backed by zram; filling
# it does not OOM-kill, it livelocks kswapd and needs a hard reboot. The FileSink
# scenarios write gigabytes, so this refuses rather than warns.
FSTYPE="$(findmnt -no FSTYPE --target "$OUT" 2>/dev/null || echo unknown)"
case "$FSTYPE" in
    tmpfs|ramfs)
        echo "REFUSING: $OUT is on $FSTYPE. The file scenarios write gigabytes."
        echo "Set OUT= to a path on real disk."
        exit 1
        ;;
esac
AVAIL_GB=$(df -BG --output=avail "$OUT" | tail -1 | tr -dc '0-9')
if [[ -n "$AVAIL_GB" && "$AVAIL_GB" -lt 20 ]]; then
    echo "REFUSING: only ${AVAIL_GB}G free at $OUT; need >= 20G for the file scenarios."
    exit 1
fi

COMMIT="$(git -C "$ROOT" rev-parse --short HEAD)"
BRANCH="$(git -C "$ROOT" rev-parse --abbrev-ref HEAD)"
DIRTY="$(git -C "$ROOT" status --porcelain | wc -l | tr -d ' ')"
# Hash of the benchmark source as it exists ON DISK -- not `git ls-files -s`, which reads
# the index and would therefore ignore untracked files and report stale hashes for
# modified ones. Since the whole point of carrying this directory across checkouts is that
# it is uncommitted on at least one side, an index-based hash would match vacuously and
# prove nothing.
# Scoped to exactly what the GateBenchmark binary compiles. crow_file_benchmark.cpp and
# abseil_file_benchmark.cpp are deliberately excluded: they legitimately differ between the
# sides (the newer Sink contract requires noexcept overrides the older one cannot have), and
# they are not the A/B subject.
BENCH_TREE="$(cd "$ROOT/benchmarks/logger" && sha256sum benchmark_harness.hpp crow_gate_benchmark.cpp CMakeLists.txt \
    | sha256sum | cut -c1-12)"
RUN_ID="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

echo "label=$LABEL branch=$BRANCH commit=$COMMIT dirty=$DIRTY bench_tree=$BENCH_TREE"
echo "reps=$REPS out=$OUT/raw.jsonl"
[[ "$DIRTY" != "0" ]] && echo "[warn] working tree is dirty; the measured library may not match $COMMIT"

filter_args() {
    [[ -n "$SCENARIO" ]] && echo --scenario "$SCENARIO"
}

meta_args() {
    echo --meta "run_id=$RUN_ID" --meta "label=$LABEL" --meta "branch=$BRANCH" \
         --meta "commit=$COMMIT" --meta "dirty=$DIRTY" --meta "bench_tree=$BENCH_TREE" \
         --meta "layout=$1"
}

# Rep-major: a thermal or frequency ramp then hits every scenario equally, instead
# of penalising whichever scenario happened to run last.
for rep in $(seq 1 "$REPS"); do
    echo "--- rep $rep/$REPS: primary layout ($CPUS_PRIMARY) ---"
    taskset -c "$CPUS_PRIMARY" "$GATE_BIN" --reps 1 --json "$OUT/raw.jsonl" \
        --out-dir "$OUT/logs" $(meta_args primary) $(filter_args) >/dev/null

    if [[ -x "$TERM_BIN" ]]; then
        taskset -c "$CPUS_PRIMARY" "$TERM_BIN" --reps 1 --json "$OUT/raw.jsonl" \
            --out-dir "$OUT/logs" $(meta_args primary) $(filter_args) >/dev/null
    fi
done

echo "--- control layout ($CPUS_CONTROL), one rep ---"
taskset -c "$CPUS_CONTROL" "$GATE_BIN" --reps 1 --json "$OUT/raw.jsonl" \
    --out-dir "$OUT/logs" $(meta_args control) $(filter_args) >/dev/null

# Drift control: untouched by the change under test, so a difference between the
# two sides here is the machine moving, not the library.
if [[ -x "$ABSEIL_BIN" ]]; then
    echo "--- abseil drift control ---"
    taskset -c "$CPUS_PRIMARY" "$ABSEIL_BIN" >"$OUT/abseil_$LABEL.txt" 2>&1
    echo "wrote $OUT/abseil_$LABEL.txt"
fi

rm -f "$OUT"/logs/*.log
echo "done: $LABEL"
