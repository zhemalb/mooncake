#!/usr/bin/env python3
"""Считает повторно используемые префиксы в первых N запросах трассы Mooncake."""

import argparse
import collections
import json


def reused_prefix(keys: list[int], seen: set[int]) -> int:
    # Одинаковый ключ блока в трассе значит одинаковый префикс, поэтому хватает множества ключей.
    length = 0
    while length < len(keys) and keys[length] in seen:
        length += 1
    return length


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace")
    parser.add_argument("--limit", type=int, default=200)
    args = parser.parse_args()

    seen: set[int] = set()
    hit_lengths: collections.Counter[int] = collections.Counter()
    samples = []
    requests = 0

    with open(args.trace, encoding="utf-8") as stream:
        for line in stream:
            if requests >= args.limit:
                break
            requests += 1

            request = json.loads(line)
            keys = request["hash_ids"]
            hit = reused_prefix(keys, seen)
            seen.update(keys)

            if hit:
                hit_lengths[hit] += 1
            if hit >= 2 and len(samples) < 5:
                samples.append(
                    {
                        "request": requests,
                        "reusable_prefix_blocks": hit,
                        "input_tokens": request["input_length"],
                    }
                )

    result = {
        "checked_requests": requests,
        "requests_with_reusable_prefix": sum(hit_lengths.values()),
        "requests_with_prefix_ge_2": sum(n for length, n in hit_lengths.items() if length >= 2),
        "reusable_prefix_blocks": sum(length * n for length, n in hit_lengths.items()),
        "max_reusable_prefix_blocks": max(hit_lengths, default=0),
        "hit_length_histogram": dict(sorted(hit_lengths.items())),
        "sample_hits": samples,
    }
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
