#!/usr/bin/env bash
set -euo pipefail

# Узел A хранит блок, узел B знает про A и забирает блок у него.
port_a=18080
port_b=18081
task_tmp_dir="$(mktemp -d -t mooncake-kt1.XXXXXX)"
pids=()

cleanup() {
  for pid in ${pids[@]+"${pids[@]}"}; do
    kill "${pid}" 2>/dev/null || true
    wait "${pid}" 2>/dev/null || true
  done
  rm -rf "${task_tmp_dir}"
}
trap cleanup EXIT

start_node() {
  local name="$1" port="$2" peers="$3"
  sed -e "s|^data-dir:.*|data-dir: ${task_tmp_dir}/${name}|" \
    -e "s|^server-port:.*|server-port: ${port}|" \
    -e "s|^peers:.*|peers: ${peers}|" \
    configs/config_vars.yaml >"${task_tmp_dir}/${name}.yaml"
  build/cache_node \
    --config configs/static_config.yaml \
    --config_vars "${task_tmp_dir}/${name}.yaml" \
    >"${task_tmp_dir}/${name}.log" 2>&1 &
  pids+=("$!")
}

wait_node() {
  local name="$1" port="$2" pid="$3"
  for _ in {1..50}; do
    if ! kill -0 "${pid}" 2>/dev/null; then
      cat "${task_tmp_dir}/${name}.log" >&2
      echo "cache_node ${name} не запустился" >&2
      exit 1
    fi
    if curl --fail --silent "http://127.0.0.1:${port}/ping" >/dev/null; then
      return
    fi
    sleep 0.1
  done
  echo "cache_node ${name} не ответил на /ping" >&2
  exit 1
}

# Сначала один раз запустите cmake из README.
cmake --build build --target cache_node block_store_test >/dev/null
build/block_store_test

for port in "${port_a}" "${port_b}"; do
  if curl --silent "http://127.0.0.1:${port}/ping" >/dev/null; then
    echo "порт ${port} уже занят, остановите старый cache_node" >&2
    exit 1
  fi
done

start_node a "${port_a}" "[]"
start_node b "${port_b}" "[\"http://127.0.0.1:${port_a}\"]"
wait_node a "${port_a}" "${pids[0]}"
wait_node b "${port_b}" "${pids[1]}"

scripts/cache_probe.py \
  --base-url "http://127.0.0.1:${port_a}" \
  --peer-url "http://127.0.0.1:${port_b}"
