# Explorer visual-alignment review — 2026-09-12

## Scope and provenance

- Explorer baseline: `08875ebe74ccaea3e8531c7a40553f8c437b0cbd` (`master`).
- Qwertycoin core used by the runtime preview: `e6e0b46b6603bc5c1402df63696b514ba735f8ee`.
- Design reference: `qwertycoin-org/qwertycoin-org.github.io` at `cad291f45602e44c94f756c5592167771dee7681`.
- Preview image: `sha256:9b236c6afc3dcaa42bbba6d0de0ceef48177684f83e667c520b8578e33cee5aa`.
- Preview chain identity: final genesis `906629482787e94cb00463696a0e95ec75a480da09257c6270c65ba1a74a76b0`, block count 3, tip height 2.
- Active deployment options reproduced in preview: JSON API enabled; transaction pusher, autorefresh pages and hex tools disabled.

The preview ran in a separate hardened container. It mounted the active chain read-only, used a separate derived-data volume, and was reachable only through an SSH tunnel. The public explorer was not changed.

## Design decisions

- Reused the approved Original-Q, local Archivo 900 and Inter 400/600 files, font licences and favicon family from the project website.
- Consolidated the interface around semantic cream, black, gold and restrained violet tokens instead of adding another override layer.
- Kept the explorer data-dense: search, four primary metrics and canonical block rows remain above the first desktop fold.
- Preserved the existing server-rendered Crow/mstch architecture. JavaScript remains progressive enhancement for theme, mobile navigation, copy feedback and existing refresh behavior.
- Kept public wording at **EPoSe** while retaining protocol/RPC terminology where technically required.
- Replaced the obsolete rehearsal/reset banner with a factual final-mainnet notice: final genesis is active and the page is a read-only observer.
- Kept lifecycle-active, qualified and reachable as separate states. Unsupported, unanchored, stale, unavailable and empty states remain explicit.

## Route and behavior verification

The browser regression covered:

- `/`, `/blocks`, `/blocks/0`, `/page/0`, `/txpool`, `/mempool`;
- block lookup by height and hash;
- `/tx/<hash>` and `/tx/<hash>/1`;
- `/service-nodes`, `/epochs`, `/network`;
- successful search by height, block hash and transaction hash;
- empty, malformed, not-found, 129-character and over-4-KiB search requests;
- exact hash copy, active navigation, theme persistence, mobile-menu keyboard control and Escape focus return;
- asset status, MIME type and cache headers;
- 360, 390, 768, 1024 and 1440 CSS-pixel widths in light and dark themes;
- equivalent 200% reflow checks for 1440- and 1024-pixel browser windows.

Machine-readable results: [browser E2E](e2e-report.json) and [200% reflow](zoom-200-report.json).

The current chain contains three blocks, so the requested “first four rows above the fold” cannot be demonstrated with live data. All three available canonical rows are visible at 1440 × 900 without scrolling.

## Automated tests

```text
cmake --build /workspace/qwc-builds/explorer-qwc-v2 --parallel 1
ctest --test-dir /workspace/qwc-builds/explorer-qwc-v2 --output-on-failure

3/3 tests passed:
- explorer_regression_tests
- transaction_semantics_tests
- privacy_routes
```

The same three tests passed inside the immutable Ubuntu 22.04 preview build linked against the exact final core revision above.

## Lighthouse

Environment: Lighthouse 13.4.1, Chrome for Testing 153.0.8010.12, Node.js 24.20.0. Runs used the isolated final-core preview and its fixed three-block snapshot.

| View | Profile | Performance | Accessibility | Best Practices | SEO | LCP | CLS | TBT |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Overview | Desktop | 100 | 100 | 100 | 100 | 563 ms | 0.010 | 0 ms |
| Overview | Mobile | 100 | 100 | 100 | 100 | 1,706 ms | 0.047 | 0 ms |
| Block 2 | Desktop | 100 | 100 | 100 | 100 | 530 ms | 0 | 0 ms |
| Block 2 | Mobile | 100 | 100 | 100 | 100 | 1,472 ms | 0 | 0 ms |
| Coinbase transaction | Desktop | 100 | 100 | 100 | 100 | 481 ms | 0 | 0 ms |
| Coinbase transaction | Mobile | 100 | 100 | 100 | 100 | 1,471 ms | 0 | 0 ms |

Lighthouse is a lab measurement. There is no field dataset for INP, so this review does not claim a field Core Web Vitals pass. Tested interactions had no long task or console error.

Machine-readable result: [Lighthouse summary](lighthouse-summary.json).

## Visual evidence

Every link below is a full-page PNG captured from the controlled baseline or the final-core preview.

| View | Before | After |
| --- | --- | --- |
| Overview · desktop · light | [PNG](before/overview-desktop-light.png) | [PNG](after/overview-desktop-light.png) |
| Overview · desktop · dark | [PNG](before/overview-desktop-dark.png) | [PNG](after/overview-desktop-dark.png) |
| Overview · mobile · light | [PNG](before/overview-mobile-light.png) | [PNG](after/overview-mobile-light.png) |
| Overview · mobile · dark | [PNG](before/overview-mobile-dark.png) | [PNG](after/overview-mobile-dark.png) |
| Block · desktop · light | [PNG](before/block-desktop-light.png) | [PNG](after/block-desktop-light.png) |
| Block · desktop · dark | [PNG](before/block-desktop-dark.png) | [PNG](after/block-desktop-dark.png) |
| Block · mobile · light | [PNG](before/block-mobile-light.png) | [PNG](after/block-mobile-light.png) |
| Block · mobile · dark | [PNG](before/block-mobile-dark.png) | [PNG](after/block-mobile-dark.png) |
| Transaction · desktop · light | [PNG](before/transaction-desktop-light.png) | [PNG](after/transaction-desktop-light.png) |
| Transaction · desktop · dark | [PNG](before/transaction-desktop-dark.png) | [PNG](after/transaction-desktop-dark.png) |
| Transaction · mobile · light | [PNG](before/transaction-mobile-light.png) | [PNG](after/transaction-mobile-light.png) |
| Transaction · mobile · dark | [PNG](before/transaction-mobile-dark.png) | [PNG](after/transaction-mobile-dark.png) |
| Service nodes · desktop · light | [PNG](before/service-nodes-desktop-light.png) | [PNG](after/service-nodes-desktop-light.png) |
| Service nodes · desktop · dark | [PNG](before/service-nodes-desktop-dark.png) | [PNG](after/service-nodes-desktop-dark.png) |
| Service nodes · mobile · light | [PNG](before/service-nodes-mobile-light.png) | [PNG](after/service-nodes-mobile-light.png) |
| Service nodes · mobile · dark | [PNG](before/service-nodes-mobile-dark.png) | [PNG](after/service-nodes-mobile-dark.png) |

## Known limitations

- Reachability remains unsupported by the current core API and is labelled accordingly.
- EPoSe calls are not exposed as one common block-anchored atomic snapshot; independently loaded sections remain labelled unanchored.
- Optional transaction pusher, autorefresh and hex tooling were not enabled in the reproduced production command and were not expanded by this visual-only change.
- INP requires field data; only deterministic interaction and main-thread checks are available here.
