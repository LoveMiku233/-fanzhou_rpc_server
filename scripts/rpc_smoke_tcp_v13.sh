#!/usr/bin/env bash
set -euo pipefail

HOST="${RPC_HOST:-127.0.0.1}"
PORT="${RPC_PORT:-12345}"
TARGET_DEV="${TARGET_DEV:-1}"
DEV_LIST="${DEV_LIST:-1,2,3,4}"
AUTH_TOKEN="${AUTH_TOKEN:-}"
STRICT_CONNECTION=0
TIMEOUT_SEC="${RPC_TIMEOUT_SEC:-3}"

if [[ "${1:-}" == "--strict-connection" ]]; then
  STRICT_CONNECTION=1
fi

PASS=0
FAIL=0
SKIP=0
RID=1000

log() { printf '%s\n' "$*"; }
ok() { PASS=$((PASS + 1)); log "[PASS] $*"; }
fail() { FAIL=$((FAIL + 1)); log "[FAIL] $*"; }
skip() { SKIP=$((SKIP + 1)); log "[SKIP] $*"; }

json_escape() {
  local s="$1"
  s="${s//\\/\\\\}"
  s="${s//\"/\\\"}"
  printf '%s' "$s"
}

inject_auth() {
  local params="$1"
  if [[ -z "$AUTH_TOKEN" ]]; then
    printf '%s' "$params"
    return 0
  fi
  local token
  token="$(json_escape "$AUTH_TOKEN")"
  if [[ "$params" == "{}" ]]; then
    printf '{"auth_token":"%s"}' "$token"
  else
    printf '%s' "${params%}}"
    printf ',\"auth_token\":\"%s\"}' "$token"
  fi
}

rpc_call() {
  local method="$1"
  local params="$2"
  local params_with_auth
  params_with_auth="$(inject_auth "$params")"
  RID=$((RID + 1))
  local req
  req="$(printf '{"jsonrpc":"2.0","id":%d,"method":"%s","params":%s}\n' "$RID" "$method" "$params_with_auth")"
  timeout "${TIMEOUT_SEC}s" nc "$HOST" "$PORT" <<< "$req" | tr -d '\r'
}

assert_contains() {
  local text="$1"
  local needle="$2"
  [[ "$text" == *"$needle"* ]]
}

response_has_dev() {
  local text="$1"
  local dev="$2"
  [[ "$text" =~ \"dev\"[[:space:]]*:[[:space:]]*${dev}([^0-9]|$) ]]
}

run_expect_contains_raw() {
  local name="$1"
  local resp="$2"
  local expect1="$3"
  local expect2="${4:-}"
  if ! assert_contains "$resp" "$expect1"; then
    fail "$name (missing: $expect1) resp=$resp"
    return
  fi
  if [[ -n "$expect2" ]] && ! assert_contains "$resp" "$expect2"; then
    fail "$name (missing: $expect2) resp=$resp"
    return
  fi
  ok "$name"
}

run_expect_contains() {
  local name="$1"
  local method="$2"
  local params="$3"
  local expect1="$4"
  local expect2="${5:-}"

  local resp
  if ! resp="$(rpc_call "$method" "$params")"; then
    fail "$name (rpc call failed)"
    return
  fi
  if ! assert_contains "$resp" "$expect1"; then
    fail "$name (missing: $expect1) resp=$resp"
    return
  fi
  if [[ -n "$expect2" ]] && ! assert_contains "$resp" "$expect2"; then
    fail "$name (missing: $expect2) resp=$resp"
    return
  fi
  ok "$name"
}

log "RPC smoke target: $HOST:$PORT (dev=$TARGET_DEV)"

if ! nc -z -w 1 "$HOST" "$PORT" >/dev/null 2>&1; then
  log "[FATAL] RPC server is not reachable at ${HOST}:${PORT}"
  log "Start server first, then rerun this script."
  exit 2
fi

# 1) Base RPC health
run_expect_contains "rpc.ping basic" "rpc.ping" "{}" "\"ok\":true"
run_expect_contains "rpc.list includes tcp methods" "rpc.list" "{}" "device.tcp.ping"

# 2) Validation-path tests (independent from board connection)
run_expect_contains "device.tcp.relay.set invalid ch" \
  "device.tcp.relay.set" \
  "{\"dev\":${TARGET_DEV},\"ch\":0,\"state\":1,\"waitMs\":200}" \
  "\"ok\":false" "invalid ch"

run_expect_contains "device.tcp.cfg.set invalid comm_mode" \
  "device.tcp.cfg.set" \
  "{\"dev\":${TARGET_DEV},\"args\":{\"comm_mode\":9},\"waitMs\":200}" \
  "\"ok\":false" "comm_mode out of range"

run_expect_contains "relay.controlBatch strict invalid item" \
  "relay.controlBatch" \
  "{\"strict\":true,\"commands\":[{\"node\":1,\"ch\":9,\"action\":\"fwd\"}]}" \
  "\"ok\":false" "invalid command at index"

# 3) Connection-dependent tests
STATUS_RESP="$(rpc_call "device.tcp.status" "{}" || true)"
if assert_contains "$STATUS_RESP" "\"clientCount\":"; then
  if [[ "$STATUS_RESP" =~ \"clientCount\"[[:space:]]*:[[:space:]]*[1-9][0-9]* ]]; then
    ok "device.tcp.status reports connected clients"
    IFS=',' read -ra DEV_ARR <<< "$DEV_LIST"
    for dev in "${DEV_ARR[@]}"; do
      dev="${dev//[[:space:]]/}"
      [[ -z "$dev" ]] && continue
      if ! [[ "$dev" =~ ^[0-9]+$ ]] || [[ "$dev" -lt 1 ]] || [[ "$dev" -gt 255 ]]; then
        fail "invalid DEV_LIST item: $dev"
        continue
      fi

      if ! response_has_dev "$STATUS_RESP" "$dev"; then
        if [[ "$STRICT_CONNECTION" -eq 1 ]]; then
          fail "dev=${dev} not found in device.tcp.status clients"
        else
          skip "dev=${dev} not connected, skip live tests for this device"
        fi
        continue
      fi

      ok "device.tcp.status contains dev=${dev}"

      run_expect_contains "device.tcp.ping dev=${dev}" \
        "device.tcp.ping" \
        "{\"dev\":${dev},\"waitMs\":1200}" \
        "\"ok\":true" "\"targetDev\":${dev}"

      run_expect_contains "device.tcp.status.get dev=${dev}" \
        "device.tcp.status.get" \
        "{\"dev\":${dev},\"waitMs\":1200}" \
        "\"ok\":true" "\"targetDev\":${dev}"

      CFG_RESP="$(rpc_call "device.tcp.cfg.get" "{\"dev\":${dev},\"waitMs\":1200}" || true)"
      run_expect_contains_raw "device.tcp.cfg.get dev=${dev}" "$CFG_RESP" "\"ok\":true" "\"targetDev\":${dev}"
    done
  else
    if [[ "$STRICT_CONNECTION" -eq 1 ]]; then
      fail "device.tcp.status: no connected board client"
    else
      skip "No TCP board connected, skip live device.tcp.* command tests"
    fi
  fi
else
  fail "device.tcp.status malformed response: $STATUS_RESP"
fi

log "Summary: PASS=$PASS FAIL=$FAIL SKIP=$SKIP"
if [[ "$FAIL" -gt 0 ]]; then
  exit 1
fi
exit 0
