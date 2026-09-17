"use strict";

const assert = require("node:assert/strict");
const status = require("../src/templates/assets/epose-status.js");

function info(overrides = {}) {
    return Object.assign({
        protocol_version: 2,
        current_epoch: 3,
        epoch_end_height: 2879,
        observer_tip_height: 2719,
        observed_at_unix: 100,
        qualification_availability: "current"
    }, overrides);
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
        observed_at_unix: 101
    }, overrides);
}

function rewards(overrides = {}) {
    return Object.assign({
        height: 2720,
        epoch: 2,
        observed_at_unix: 101
    }, overrides);
}

const coherent = status.snapshotContext(
    info({observed_at_unix: 100}), nodes(), rewards());
assert.deepEqual(coherent, {
    current: true, currentFinal: true, rewards: true,
    currentEpoch: 3, sourceEpoch: 2
});
assert.deepEqual(status.snapshotContext(info(), nodes(), null), {
    current: true, currentFinal: true, rewards: false,
    currentEpoch: 3, sourceEpoch: null
});
assert.equal(status.snapshotContext(
    info({observed_at_unix: 101}), nodes({observed_at_unix: 102}),
    rewards({observed_at_unix: 100})).rewards, true);

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
assert.equal(status.currentQualification(node({qualified_for_current_epoch: true}), info(), coherent).label, "Qualified");
const closedInfo = info({observer_tip_height: 2819});
const coherentAtClose = status.snapshotContext(
    closedInfo, nodes(), rewards({height: 2820}));
assert.equal(status.currentQualification(node(), closedInfo, coherentAtClose).label, "Not qualified");
assert.equal(status.currentQualification(node({protocol_active: false}), info(), coherent).label, "Not participating");
assert.equal(status.currentQualification(node({qualified_for_current_epoch: null}), info(), coherent).label, "Unavailable");
assert.equal(status.currentQualification(node({qualification_epoch: 2}), info(), coherent).label, "Unavailable");

const epochFour = info({current_epoch: 4, epoch_end_height: 3599, observer_tip_height: 2880});
const coherentEpochFour = status.snapshotContext(
    epochFour, nodes({current_epoch: 4, source_epoch: 3}),
    rewards({height: 2881, epoch: 3}));
assert.equal(status.currentQualification(node({qualification_epoch: 4}), epochFour, coherentEpochFour).label, "Pending");
assert.equal(status.currentQualification(node(), epochFour, coherentEpochFour).label, "Unavailable");

assert.equal(status.rewardQualification(node({qualified_for_source_epoch: true}), coherent).label, "Qualified");
assert.equal(status.rewardQualification(node(), coherent).label, "Not qualified");
assert.equal(status.rewardQualification(node({effective_epoch: 3}), coherent).label, "Unavailable");
assert.equal(status.rewardQualification(node({source_qualification_availability: "unavailable"}), coherent).label, "Unavailable");
assert.equal(status.rewardQualification(node({qualified_for_source_epoch: null}), coherent).label, "Unavailable");
assert.equal(status.rewardQualification(node({source_qualification_epoch: 1}), coherent).label, "Unavailable");

assert.equal(status.registration(node()).label, "Registration active");
assert.equal(status.registration(node({protocol_active: false})).label, "Registration inactive");
assert.equal(status.registration(node({protocol_active: null})).label, "Registration unavailable");

assert.equal(status.servicePeriod(node()), "Sequence 0 · Epochs 2–4 · expires at start of Epoch 5");
assert.equal(status.servicePeriod(node({effective_epoch: 3, expiry_epoch: 4})), "Sequence 0 · Epoch 3 · expires at start of Epoch 4");
assert.equal(status.servicePeriod(node({effective_epoch: 3, expiry_epoch: 3})), "Unavailable");

const staleNodesAtClose = status.snapshotContext(
    info({observer_tip_height: 2819, observed_at_unix: 102}),
    nodes({observed_at_unix: 101}),
    rewards({height: 2820, observed_at_unix: 102}));
assert.deepEqual(staleNodesAtClose, {
    current: true, currentFinal: false, rewards: true,
    currentEpoch: 3, sourceEpoch: 2
});
assert.equal(status.currentQualification(
    node(), info({observer_tip_height: 2819}), staleNodesAtClose).label, "Unavailable");
const olderNodesWhileOpen = status.snapshotContext(
    info({observer_tip_height: 2818, observed_at_unix: 102}),
    nodes({observed_at_unix: 101}),
    rewards({height: 2819, observed_at_unix: 102}));
assert.equal(status.currentQualification(
    node(), info({observer_tip_height: 2818}), olderNodesWhileOpen).label, "Pending");

assert.deepEqual(status.snapshotContext(
    info({observed_at_unix: 100}), nodes(), rewards({height: 2721})),
    {current: true, currentFinal: true, rewards: false,
        currentEpoch: 3, sourceEpoch: null});
assert.deepEqual(status.snapshotContext(
    info({observed_at_unix: 100}), nodes({current_epoch: 2}), rewards()),
    {current: false, currentFinal: false, rewards: false,
        currentEpoch: null, sourceEpoch: null});
assert.deepEqual(status.snapshotContext(
    info({observed_at_unix: 100}), nodes(), rewards({epoch: 1})),
    {current: true, currentFinal: true, rewards: false,
        currentEpoch: 3, sourceEpoch: null});
assert.deepEqual(status.snapshotContext(
    info({current_epoch: 4, observer_tip_height: 2880, observed_at_unix: 100}),
    nodes({current_epoch: 4, source_epoch: 2}),
    rewards({height: 2881, epoch: 2})),
    {current: true, currentFinal: true, rewards: false,
        currentEpoch: 4, sourceEpoch: null});

const sameSecondAtClose = status.snapshotContext(
    closedInfo, nodes({observed_at_unix: 100}),
    rewards({height: 2820, observed_at_unix: 100}));
assert.equal(sameSecondAtClose.current, true);
assert.equal(sameSecondAtClose.currentFinal, false);
assert.equal(status.currentQualification(
    node(), closedInfo, sameSecondAtClose).label, "Unavailable");
assert.equal(status.currentQualification(node(), info(), {
    current: true, currentFinal: false, currentEpoch: 3
}).detail, "Qualification has not been finalized");

console.log("EPoSe presentation-state tests passed");
