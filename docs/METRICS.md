# Explorer Metric Dictionary

All QWC values use eight decimal places. Public APIs encode atomic monetary
amounts as decimal strings and rendered pages format them with integer
arithmetic. Floating point is reserved for explicitly approximate display
quantities such as kB.

| Metric | Unit | Source and scope | Calculation/window | Freshness and unavailable behavior |
| --- | --- | --- | --- | --- |
| Tip height | zero-based block height | Local read-only LMDB observer | `block_count - 1` when the chain is non-empty | Bound to the rendered request; unavailable on an empty/inaccessible DB |
| Block count | blocks | Local read-only LMDB observer | Current blockchain height returned by core storage | Bound to the rendered request |
| Observed tip hash | 32-byte hash | Daemon network-info snapshot | Daemon-reported top block hash | Snapshot age is shown; stale is not relabeled live |
| Mempool transactions | transactions | Shared explorer mempool snapshot | Exact snapshot length | Empty is distinct from an unavailable section |
| Target block interval | seconds | Daemon network-info snapshot | Active daemon parameter | Snapshot age is shown |
| Block weight | kB (display) | Local LMDB consensus weight | Core DB block weight divided by 1024 for display | Called weight, never serialized size |
| Transaction count | transactions | Canonical block | Regular transaction hash count plus one coinbase transaction | Convention is stated in the table caption and row |
| Fees | QWC | Regular transactions in the selected block | Checked sum of exact atomic fees, formatted to 8 decimals | `unavailable` if any transaction is missing or checked addition fails |
| Coinbase total | QWC | Coinbase transaction outputs | Exact public output total, formatted to 8 decimals | Miner/service split remains unavailable without canonical v2 payment mapping |
| EPoSE source epoch | epoch | `get_epose_info` adjacent to `get_service_nodes` | Current observer epoch | Marked `unanchored` until core supplies one common height/hash anchor |
| Protocol-active | boolean | Current identity descriptor interval | `effective_epoch <= epoch < expiry_epoch` in core | Does not mean endpoint reachable |
| Qualified | boolean | Current core qualification view | The RPC result is preserved exactly, including `false` and a valid zero qualified count | Zero/false remain distinct from an unavailable RPC response |
| Reachability | availability state | No supported public observer source | Not calculated | Always `unsupported` in this release |
| Reward preview | availability state | `get_service_rewards` | No browser-side payee selection | `unsupported` unless core explicitly returns `preview_available=true` |

## Deliberately omitted metrics

- **Issued/circulating/spendable/unlocked supply:** the historical
  generated-coins accumulator does not establish these semantics.
- **Peer count:** restricted daemon responses can redact it; zero is not accepted
  as evidence of no peers.
- **Hashrate:** omitted until the displayed estimate can name its source window.
- **Miner/service coinbase split:** omitted until the validating core exposes its
  canonical, block-anchored EPoSE v2 payment-to-output mapping.
