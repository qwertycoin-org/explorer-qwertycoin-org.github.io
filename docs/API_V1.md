# Public Read API v1

The browser uses only allowlisted `/api/v1` explorer endpoints. A reverse proxy
must not expose a generic daemon RPC route.

## Response and error rules

- Success: HTTP 200 and `{"status":"success","data":...}`.
- Invalid or out-of-range input: HTTP 400 and JSend `fail`.
- Required upstream/DB failure: HTTP 503 and JSend `error`.
- An HTTP 200 JSON-RPC error or a missing result is never converted to an empty
  successful response.
- Pagination is strict unsigned decimal input, 1–100 rows per page, with checked
  multiplication. Out-of-range pages return a bounded successful empty page.
- Monetary atomic values are decimal strings; QWC display values are exact
  eight-decimal strings. Counts and heights remain JSON integers.

## Endpoints

- `GET /api/v1/version`
- `GET /api/v1/overview`
- `GET /api/v1/network`
- `GET /api/v1/supply`
- `GET /api/v1/identity`
- `GET /api/v1/transactions?page=0&limit=25`
- `GET /api/v1/mempool?page=0&limit=25`
- `GET /api/v1/epose`
- `GET /api/v1/epose/service-nodes`
- `GET /api/v1/epose/rewards`

`/api/v1/overview` is a bounded, five-second server snapshot used by the
dashboard refresh controller. It contains independently timestamped network,
supply and mempool data plus a canonical-tip-validated recent-block window.
The browser polls it every 15 seconds with an eight-second deadline, one
in-flight request, failure backoff and hidden-tab suspension. A successful
network refresh does not change the mempool or supply observation time.

`/api/v1/supply` exposes the explorer-owned issuance index. The exact
`minted_supply_atomic` is cumulative canonical coinbase outputs minus regular
transaction fees, including immature rewards. It is not circulating or
spendable supply. Consumers must inspect `availability`, `complete`,
`indexed_through_height`, `tip_height`, `tip_hash`, `genesis_hash`,
`calculation_version` and `chain_reset_id` before presenting it as current.

The compatibility namespace `/qwc-rpc/` is separate from `/api/v1`. The
application parses JSON-RPC envelopes and accepts only the wallet path and
method allowlists in `wallet_rpc_policy.h`; batches, notifications, malformed
envelopes, unknown methods and unknown paths are rejected before the restricted
daemon is contacted. Request bodies are not access-logged.

HTML search is `POST /search`; values are length-bounded and are not placed in a
URL. The old secret-processing routes are absent from the application and must
return 404/410 at the edge.

## Current anchoring limit

The selected core does not yet attach a common tip height/hash, genesis hash,
parameter fingerprint, or deployment-reset generation to all EPoSE responses.
The service-node adapter therefore reports `snapshot_consistency: "unanchored"`
instead of implying one consistent multi-source snapshot.
