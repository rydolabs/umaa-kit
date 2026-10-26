#!/usr/bin/env bash
# Linux-only network topology for GitHub hosted ubuntu-24.04 runners.
set -euo pipefail
BR=umaa_ci_br
NAMES=(umaa_ci_pub umaa_ci_sim1 umaa_ci_sim2)
IPS=(10.233.77.11 10.233.77.12 10.233.77.13)
teardown() {
  for ns in "${NAMES[@]}"; do
    if ip netns list | grep -Eq "^${ns}( |$)"; then
      ip netns delete "$ns"
    fi
  done
  ip link delete "$BR" 2>/dev/null || true
}
setup() {
  if [[ "$(id -u)" != 0 ]]; then echo 'netns requires sudo/root' >&2; exit 2; fi
  teardown
  trap 'teardown' ERR
  ip link add "$BR" type bridge
  ip link set "$BR" up
  for i in 0 1 2; do
    ns="${NAMES[$i]}"
    ip netns add "$ns"
    ip -n "$ns" link set lo up
    ip link add "umaa_ci_v${i}" type veth peer name "umaa_ci_p${i}"
    ip link set "umaa_ci_p${i}" netns "$ns"
    ip link set "umaa_ci_v${i}" master "$BR"
    ip link set "umaa_ci_v${i}" up
    ip -n "$ns" link set "umaa_ci_p${i}" name eth0
    ip -n "$ns" address add "${IPS[$i]}/24" dev eth0
    ip -n "$ns" link set eth0 up
  done
  trap - ERR
  for i in 0 1 2; do
    ip netns exec "${NAMES[$i]}" ip route get "${IPS[$(((i + 1) % 3))]}"
  done
  echo 'DDS_NETNS_TOPOLOGY_READY: 3 isolated nodes on veth/bridge'
}
case "${1:-}" in
  setup) setup ;;
  teardown) teardown ;;
  *) echo 'Usage: sudo bash tools/dds_netns.sh setup|teardown' >&2; exit 2 ;;
esac
