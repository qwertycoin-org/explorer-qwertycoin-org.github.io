# Bounded Core Interface Dependencies

The explorer fails closed where the current HF17/EPoSE v2 core does not expose
enough validated public evidence. These are read-only observer dependencies, not
requests to change consensus.

## Implemented dependency

1. **Validated service payment mapping:** `get_epose_block_reward` returns a block-height/hash-anchored
   serialization of the canonical accepted `validated_service_payment_v2`
   context, including bound output indices and exact atomic amounts. The explorer
   must never duplicate payee selection or infer the split from output position.
   This interface is presentation-only. Aggregate issuance remains independently
   derived from accepted coinbase outputs minus regular transaction fees.

## Remaining dependencies

1. **Common response identity:** network, genesis hash, consensus-parameter
   fingerprint, tip height/hash, core build, and an operator deployment/reset
   generation on every derived dataset.
2. **Typed qualification availability:** distinguish valid-empty membership from
   missing, unsupported, stale, or unavailable state; remove the ambiguous
   `epoch=0 means latest` selector.
3. **Identity history:** bounded lookup by persistent `identity_id`, descriptor/key
   rotations and recovery, admission evidence, freeze membership, receipts, and
   actual payment history at a requested epoch/height.
4. **Epoch evidence:** parameter-derived start/end/cutoff/freeze boundaries,
   qualification source epoch, payout epoch, state commitment, and exact chain
   anchor.
5. **Reachability observations:** only if a separately defined, timestamped and
   privacy-reviewed observer interface is adopted. Registration or protocol
   activity must not be repurposed as online status.

The signed advertised endpoint is already available through
`get_epose_service_endpoint_v2` and is displayed separately from reachability.
The explorer also consumes `get_epose_block_reward` for an exact canonical
block-hash-bound miner/service allocation. The response is accepted only when
its hash, height, coinbase total and complete denomination proof match the local
read-only chain view; output position is never used as a recipient heuristic.

Until the remaining interfaces exist, machine-readable API fields remain
explicitly `unsupported`, `unavailable`, `unknown`, or `unanchored`; the UI uses
plain-language explanations for those states.
