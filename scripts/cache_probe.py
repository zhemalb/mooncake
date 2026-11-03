#!/usr/bin/env python3
"""Кладет блок в узел A, читает его из памяти и с диска, потом забирает тот же блок через узел B."""

import argparse
import hashlib
import json
import sys
import time
import urllib.request

BLOCK_PATH = "/blocks/checkpoint1-block"
PAYLOAD = b"mooncake-checkpoint-1|" * (16 * 1024)


def call(base_url: str, method: str, expected_status: int, body: bytes | None = None, headers=None):
    req = urllib.request.Request(base_url + BLOCK_PATH, data=body, method=method, headers=headers or {})
    with urllib.request.urlopen(req, timeout=10) as resp:
        if resp.status != expected_status:
            raise RuntimeError(f"{method} вернул {resp.status}")
        return resp.read(), resp.headers


def since_us(start_ns: int) -> int:
    return (time.perf_counter_ns() - start_ns) // 1000


def emit(operation: str, source: str | None, size: int, latency_us: int, checksum_ok: bool) -> None:
    line = {"operation": operation}
    if source:
        line["source"] = source
    line.update(payload_bytes=size, network_bytes=size, latency_us=latency_us, checksum_ok=checksum_ok)
    print(json.dumps(line, separators=(",", ":")))


def put(base_url: str, checksum: str) -> None:
    start = time.perf_counter_ns()
    headers = {"X-Checksum-SHA256": checksum, "Content-Type": "application/octet-stream"}
    call(base_url, "PUT", 201, PAYLOAD, headers)
    emit("put", None, len(PAYLOAD), since_us(start), True)


def get(base_url: str, checksum: str, expected_source: str) -> None:
    start = time.perf_counter_ns()
    data, headers = call(base_url, "GET", 200)
    latency = since_us(start)

    source = headers["X-Block-Source"]
    ok = (
        data == PAYLOAD
        and hashlib.sha256(data).hexdigest() == checksum
        and headers["X-Checksum-SHA256"] == checksum
    )
    emit("get", source, len(data), latency, ok)
    if not ok:
        raise RuntimeError("блок вернулся испорченным")
    if source != expected_source:
        raise RuntimeError(f"блок пришел из {source}, а ждали {expected_source}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-url", default="http://127.0.0.1:18080")
    parser.add_argument("--peer-url", default="http://127.0.0.1:18081")
    args = parser.parse_args()

    checksum = hashlib.sha256(PAYLOAD).hexdigest()
    put(args.base_url, checksum)
    get(args.base_url, checksum, "ram")
    call(args.base_url, "DELETE", 204)
    get(args.base_url, checksum, "ssd")

    # У B блока нет, он забирает его у A и оставляет копию у себя.
    get(args.peer_url, checksum, "remote")
    get(args.peer_url, checksum, "ram")


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        sys.exit(f"cache_probe: {e}")
