"use strict";

const assert = require("node:assert/strict");
const status = require("../src/templates/assets/epose-status.js");

function anchor(blockCount, hashCharacter = "a") {
    return {
        snapshot_block_count: blockCount,
        snapshot_tip_height: blockCount - 1,
        snapshot_tip_hash: hashCharacter.repeat(64),
        snapshot_consistency: "anchored"
    };
}

function info(overrides = {}) {
    return Object.assign({
        protocol_version: 2,
        current_epoch: 3,
        epoch_end_height: 2879,
        observer_tip_height: 2719,
        observed_at_unix: 100,
        qualification_availability: "current"
    }, anchor(2720), overrides);
}

function node(overrides = {}) {
    return Object.assign({
        descriptor_sequence: 0,
        effective_epoch: 2,
        expiry_epoch: 5,
        protocol_active: true,
        qualification_epoch: 3,
        qualification_availability: "current",
        qualified_for_current_epoch: false,
        source_qualification_epoch: 2,
        source_qualification_availability: "finalized",
        qualified_for_source_epoch: false
    }, overrides);
}

function nodes(overrides = {}) {
    return Object.assign({
        current_epoch: 3,
        source_epoch: 2,
        observed_at_unix: 100
    }, anchor(2720), overrides);
}

function rewards(overrides = {}) {
    return Object.assign({
        height: 2720,
        epoch: 2,
        observed_at_unix: 100
    }, anchor(2720), overrides);
}

const coherent = status.snapshotContext(info(), nodes(), rewards());
assert.deepEqual(coherent, {
    current: true, currentFinal: true, rewards: true,
    currentEpoch: 3, sourceEpoch: 2
});
assert.deepEqual(status.snapshotContext(info(), nodes(), null), {
    current: true, currentFinal: true, rewards: false,
    currentEpoch: 3, sourceEpoch: null
});
assert.equal(status.snapshotAnchor(info()).blockCount, 2720);
assert.equal(status.snapshotAnchor(info({snapshot_consistency: "unanchored"})), null);
assert.equal(status.snapshotAnchor(info({snapshot_tip_hash: "bad"})), null);

assert.deepEqual(status.qualificationContext(info()), {
    phase: "pending", closeHeight: 2819, tipHeight: 2719
});
assert.equal(status.qualificationContext(info({observer_tip_height: 2818})).phase, "pending");
assert.equal(status.qualificationContext(info({observer_tip_height: 2819})).phase, "closed");
assert.equal(status.qualificationContext(info({protocol_version: 99})).phase, "unavailable");
assert.equal(status.qualificationContext(info({observer_tip_height: null})).phase, "unavailable");
assert.equal(status.nonNegativeInteger(null), null);
assert.equal(status.nonNegativeInteger(undefined), null);
assert.equal(status.nonNegativeInteger(""), null);
assert.equal(status.nonNegativeInteger(0), 0);

assert.equal(status.currentQualification(node(), info(), coherent).label, "Pending");
assert.equal(status.currentQualification(
    node({qualified_for_current_epoch: true}), info(), coherent).label, "Qualified");

const closeAnchor = anchor(2820);
const closedInfo = info(Object.assign({observer_tip_height: 2819}, closeAnchor));
const coherentAtClose = status.snapshotContext(
    closedInfo, nodes(closeAnchor), rewards(Object.assign({height: 2820}, closeAnchor)));
assert.equal(status.currentQualification(
    node(), closedInfo, coherentAtClose).label, "Not qualified");
assert.equal(status.currentQualification(
    node({protocol_active: false}), info(), coherent).label, "Not participating");
assert.equal(status.currentQualification(
    node({qualified_for_current_epoch: null}), info(), coherent).label, "Unavailable");
assert.equal(status.currentQualification(
    node({qualification_epoch: 2}), info(), coherent).label, "Unavailable");

const epochFourAnchor = anchor(2881);
const epochFour = info(Object.assign({
    current_epoch: 4,
    epoch_end_height: 3599,
    observer_tip_height: 2880
}, epochFourAnchor));
const coherentEpochFour = status.snapshotContext(
    epochFour,
    nodes(Object.assign({current_epoch: 4, source_epoch: 3}, epochFourAnchor)),
    rewards(Object.assign({height: 2881, epoch: 3}, epochFourAnchor)));
assert.equal(status.currentQualification(
    node({qualification_epoch: 4}), epochFour, coherentEpochFour).label, "Pending");
assert.equal(status.currentQualification(node(), epochFour, coherentEpochFour).label, "Unavailable");

assert.equal(status.rewardQualification(
    node({qualified_for_source_epoch: true}), coherent).label, "Qualified");
assert.equal(status.rewardQualification(node(), coherent).label, "Not qualified");
assert.equal(status.rewardQualification(node({effective_epoch: 3}), coherent).label, "Unavailable");
assert.equal(status.rewardQualification(
    node({source_qualification_availability: "unavailable"}), coherent).label, "Unavailable");
assert.equal(status.rewardQualification(
    node({qualified_for_source_epoch: null}), coherent).label, "Unavailable");
assert.equal(status.rewardQualification(
    node({source_qualification_epoch: 1}), coherent).label, "Unavailable");

assert.equal(status.registration(node()).label, "Registration active");
assert.equal(status.registration(node({protocol_active: false})).label, "Registration inactive");
assert.equal(status.registration(node({protocol_active: null})).label, "Registration unavailable");

assert.equal(status.servicePeriod(node()),
    "Sequence 0 · Epochs 2–4 · expires at start of Epoch 5");
assert.equal(status.servicePeriod(node({effective_epoch: 3, expiry_epoch: 4})),
    "Sequence 0 · Epoch 3 · expires at start of Epoch 4");
assert.equal(status.servicePeriod(
    node({effective_epoch: 3, expiry_epoch: 3})), "Unavailable");

const staleNodesAtClose = status.snapshotContext(
    closedInfo,
    nodes(anchor(2819)),
    rewards(Object.assign({height: 2820}, closeAnchor)));
assert.deepEqual(staleNodesAtClose, {
    current: false, currentFinal: false, rewards: false,
    currentEpoch: null, sourceEpoch: null
});
assert.equal(status.currentQualification(
    node(), closedInfo, staleNodesAtClose).label, "Unavailable");

const sameHeightReorg = status.snapshotContext(
    closedInfo,
    nodes(anchor(2820, "b")),
    rewards(Object.assign({height: 2820}, closeAnchor)));
assert.equal(sameHeightReorg.current, false);
assert.equal(status.currentQualification(
    node(), closedInfo, sameHeightReorg).label, "Unavailable");

const validSameSecondAtClose = status.snapshotContext(
    closedInfo,
    nodes(Object.assign({observed_at_unix: 100}, closeAnchor)),
    rewards(Object.assign({height: 2820, observed_at_unix: 100}, closeAnchor)));
assert.equal(validSameSecondAtClose.current, true);
assert.equal(validSameSecondAtClose.currentFinal, true);
assert.equal(status.currentQualification(
    node(), closedInfo, validSameSecondAtClose).label, "Not qualified");

assert.deepEqual(status.snapshotContext(
    info(), nodes(), rewards({height: 2721})),
    {current: true, currentFinal: true, rewards: false,
        currentEpoch: 3, sourceEpoch: null});
assert.deepEqual(status.snapshotContext(
    info(), nodes({current_epoch: 2}), rewards()),
    {current: false, currentFinal: false, rewards: false,
        currentEpoch: null, sourceEpoch: null});
assert.deepEqual(status.snapshotContext(
    info(), nodes(), rewards(Object.assign({epoch: 2}, anchor(2720, "b")))),
    {current: true, currentFinal: true, rewards: false,
        currentEpoch: 3, sourceEpoch: null});
assert.deepEqual(status.snapshotContext(
    info(), nodes(), rewards({epoch: 1})),
    {current: true, currentFinal: true, rewards: false,
        currentEpoch: 3, sourceEpoch: null});
assert.deepEqual(status.snapshotContext(
    epochFour,
    nodes(Object.assign({current_epoch: 4, source_epoch: 2}, epochFourAnchor)),
    rewards(Object.assign({height: 2881, epoch: 2}, epochFourAnchor))),
    {current: true, currentFinal: true, rewards: false,
        currentEpoch: 4, sourceEpoch: null});

assert.equal(status.currentQualification(node(), info(), {
    current: true, currentFinal: true, currentEpoch: 3
}).detail, "Qualification has not been finalized");

console.log("EPoSe presentation-state tests passed");
