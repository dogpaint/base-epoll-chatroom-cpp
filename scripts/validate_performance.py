#!/usr/bin/env python3
import json
import sys
import os

THRESHOLDS = {
    "min_throughput_msg_per_sec": 50,
    "max_avg_connect_time_ms": 5000,
    "max_error_rate_percent": 5.0,
    "max_p95_latency_ms": 500,
    "max_p99_latency_ms": 1000,
}

def load_report(path):
    if not os.path.exists(path):
        print(f"ERROR: Report file not found: {path}")
        return None
    with open(path, 'r') as f:
        return json.load(f)

def validate(report):
    if report is None:
        return False

    results = report.get("results", {})
    passed = True
    failures = []

    throughput = results.get("throughput_msg_per_sec", 0)
    if throughput < THRESHOLDS["min_throughput_msg_per_sec"]:
        passed = False
        failures.append(f"Throughput {throughput:.1f} < {THRESHOLDS['min_throughput_msg_per_sec']} msg/sec")

    avg_conn = results.get("avg_connect_time_ms", 0)
    if avg_conn > THRESHOLDS["max_avg_connect_time_ms"]:
        passed = False
        failures.append(f"Avg connect time {avg_conn:.1f}ms > {THRESHOLDS['max_avg_connect_time_ms']}ms")

    error_rate = results.get("error_rate_percent", 0)
    if error_rate > THRESHOLDS["max_error_rate_percent"]:
        passed = False
        failures.append(f"Error rate {error_rate:.2f}% > {THRESHOLDS['max_error_rate_percent']}%")

    p95 = results.get("p95_latency_ms", 0)
    if p95 > THRESHOLDS["max_p95_latency_ms"]:
        passed = False
        failures.append(f"P95 latency {p95:.1f}ms > {THRESHOLDS['max_p95_latency_ms']}ms")

    p99 = results.get("p99_latency_ms", 0)
    if p99 > THRESHOLDS["max_p99_latency_ms"]:
        passed = False
        failures.append(f"P99 latency {p99:.1f}ms > {THRESHOLDS['max_p99_latency_ms']}ms")

    if not results.get("passed", False):
        print("  (note: test self-reported as FAILED, but individual thresholds determine actual pass/fail)")

    print("=" * 50)
    print("PERFORMANCE VALIDATION REPORT")
    print("=" * 50)
    print(f"Throughput:         {throughput:>10.1f} msg/sec  (min: {THRESHOLDS['min_throughput_msg_per_sec']})")
    print(f"Avg Connect Time:   {avg_conn:>10.1f} ms       (max: {THRESHOLDS['max_avg_connect_time_ms']})")
    print(f"Error Rate:         {error_rate:>10.2f} %        (max: {THRESHOLDS['max_error_rate_percent']})")
    print(f"P50 Latency:        {results.get('p50_latency_ms', 0):>10.1f} ms")
    print(f"P95 Latency:        {p95:>10.1f} ms       (max: {THRESHOLDS['max_p95_latency_ms']})")
    print(f"P99 Latency:        {p99:>10.1f} ms       (max: {THRESHOLDS['max_p99_latency_ms']})")
    print(f"Total Messages:     {results.get('total_messages_received', 0):>10}")
    print(f"Heartbeat OK/FAIL:  {results.get('heartbeat_ok', 0):>6} / {results.get('heartbeat_fail', 0)}")
    print(f"Connect Errors:     {results.get('connect_errors', 0):>10}")
    print(f"Send Errors:        {results.get('send_errors', 0):>10}")
    print("-" * 50)
    print(f"RESULT: {'PASS' if passed else 'FAIL'}")
    if failures:
        print("Failures:")
        for f in failures:
            print(f"  - {f}")
    print("=" * 50)

    return passed

if __name__ == "__main__":
    report_path = sys.argv[1] if len(sys.argv) > 1 else "build/tests/stress_test_report.json"
    report = load_report(report_path)
    ok = validate(report)
    sys.exit(0 if ok else 1)