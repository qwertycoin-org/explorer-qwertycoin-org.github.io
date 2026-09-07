# Bounded Core Interface Dependencies

The explorer fails closed where the current HF17/EPoSE v2 core does not expose
enough validated public evidence. These are read-only observer dependencies, not
requests to change consensus.

1. **Validated service payment mapping:** return a block-height/hash-anchored
   serialization of the canonical accepted `validated_service_payment_v2`
   context, including bound output indices and exact atomic amounts. The explorer
   must never duplicate payee selection or infer the split from output position.
2. **Common response identity:** network, genesis hash, consensus-parameter
   fingerprint, tip height/hash, core build, and an operator deployment/reset
   generation on every derived dataset.
3. **Typed qualification availability:** distinguish valid-empty membership from
   missing, unsupported, stale, or unavailable state; remove the ambiguous
   `epoch=0 means latest` selector.
4. **Identity history:** bounded lookup by stable `identity_id`, descriptor/key
   rotations and recovery, admission evidence, freeze membership, receipts, and
   actual payment history at a requested epoch/height.
5. **Epoch evidence:** parameter-derived start/end/cutoff/freeze boundaries,
   qualification source epoch, payout epoch, state commitment, and exact chain
   anchor.
6. **Reachability observations:** only if a separately defined, timestamped and
   privacy-reviewed observer interface is adopted. Registration or protocol
   activity must not be repurposed as online status.

Until these interfaces exist, the corresponding UI fields remain explicitly
`unsupported`, `unavailable`, `unknown`, or `unanchored`.
