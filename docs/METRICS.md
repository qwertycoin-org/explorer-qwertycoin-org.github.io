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
| Estimated network hashrate | H/s with SI display units | One successful daemon network snapshot | Next-block PoW difficulty divided by the active target interval, preserving wide-integer precision | Unavailable for missing/invalid difficulty or a zero target; it is not measured miner telemetry |
| Total mined supply | atomic QWC and exact 8-decimal QWC | Explorer-owned derived index over canonical read-only LMDB blocks | Checked cumulative coinbase public outputs minus regular transaction fees | Includes mature and immature rewards; unavailable/indexing until a fully persisted canonical anchor is published |
| Target block interval | seconds | Daemon network-info snapshot | Active daemon parameter | Snapshot age is shown |
| Block weight | kB (display) | Local LMDB consensus weight | Core DB block weight divided by 1024 for display | Called weight, never serialized size |
| Transaction count | transactions | Canonical block | Regular transaction hash count plus one coinbase transaction | Convention is stated in the table caption and row |
| Fees | QWC | Regular transactions in the selected block | Checked sum of exact atomic fees, formatted to 8 decimals | `unavailable` if any transaction is missing or checked addition fails |
| Coinbase total | QWC | Coinbase transaction outputs | Exact public output total, formatted to 8 decimals | Bound to the selected canonical block |
| Miner / pool payout | QWC | `get_epose_block_reward` | Canonical Core v2 allocation, including the miner fee share | Shown only when block hash, height, coinbase total and the complete payment proof match the local canonical block |
| EPoSE service payout | QWC | `get_epose_block_reward` | Sum of the exact denominated outputs accepted by Core's production payment verifier | Shown only with a valid block-bound proof; the raw output rows remain available in the block details |
| Current epoch qualification | nodes | `get_epose_info` | Current, still-evolving qualification view | Explicitly labelled current; it does not describe the source set paid by an already-mined block |
| Reward source qualification | nodes | `get_service_rewards` | Finalized prior/source epoch used for the next payout selection | The source epoch and count are displayed together |
| Service-node observation epoch | epoch | `get_epose_info` adjacent to `get_service_nodes` | Current observer epoch | Presented as an independent RPC snapshot until core supplies one common height/hash anchor |
| Advertised service endpoint | host and port | `get_epose_service_endpoint_v2` keyed by the node's on-chain `endpoint_commitment` | Core validates the signed descriptor; the explorer additionally requires an exact descriptor-hash and service-key match | A missing or mismatched lookup is `unavailable`; it is not presented as an online check |
| Protocol-active | boolean | Current identity descriptor interval | `effective_epoch <= epoch < expiry_epoch` in core | Does not mean endpoint reachable |
| Qualified | boolean | Current core qualification view | The RPC result is preserved exactly, including `false` and a valid zero qualified count | Zero/false remain distinct from an unavailable RPC response |
| Independent online check | availability state | No supported public observer source | Not calculated | Displayed as `Not exposed by Core` in this release |
| Next service reward | availability state | `get_service_rewards` | No browser-side payee selection | Displayed as `Preview not exposed by Core` unless core explicitly returns `preview_available=true` |

## Deliberately omitted metrics

- **Circulating/spendable/unlocked supply:** total mined supply is issuance,
  not evidence that private wallet outputs are mature, unspent or accessible.
- **Peer count:** restricted daemon responses can redact it; zero is not accepted
  as evidence of no peers.
