#!/usr/bin/env bash
set -euo pipefail

trace_dir="data/traces"
trace_path="${trace_dir}/conversation_trace.jsonl"
trace_url="https://raw.githubusercontent.com/kvcache-ai/Mooncake/408b831bfeff/FAST25-release/traces/conversation_trace.jsonl"
expected_sha256="b8cbb061a85206d729d91cdc2981f43c9e0d99209dce588d3af5f7934408b9df"

mkdir -p "${trace_dir}"
curl -L --fail --silent --show-error -o "${trace_path}" "${trace_url}"

actual_sha256="$(shasum -a 256 "${trace_path}" | awk '{print $1}')"
if [[ "${actual_sha256}" != "${expected_sha256}" ]]; then
  echo "контрольная сумма трассы не совпала, ждали ${expected_sha256}, получили ${actual_sha256}" >&2
  exit 1
fi

echo "трасса скачана в ${trace_path}, sha256 ${actual_sha256}"
