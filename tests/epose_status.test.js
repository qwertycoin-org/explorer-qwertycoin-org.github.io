"use strict";

const assert = require("node:assert/strict");
const status = require("../src/templates/assets/epose-status.js");

function info(overrides = {}) {
    return Object.assign({
        protocol_version: 2,
        current_epoch: 3,
        epoch_end_height: 2879,
        observer_tip_height: 2719,
        qualification_availability: "current"
    }, overrides);
}

function node(overrides = {}) {
    return Object.assign({
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

assert.equal(status.currentQualification(node(), info()).label, "Pending");
assert.equal(status.currentQualification(node({qualified_for_current_epoch: true}), info()).label, "Qualified");
assert.equal(status.currentQualification(node(), info({observer_tip_height: 2819})).label, "Not qualified");
assert.equal(status.currentQualification(node({protocol_active: false}), info()).label, "Not participating");
assert.equal(status.currentQualification(node({qualified_for_current_epoch: null}), info()).label, "Unavailable");
assert.equal(status.currentQualification(node({qualification_epoch: 2}), info()).label, "Unavailable");

const epochFour = info({current_epoch: 4, epoch_end_height: 3599, observer_tip_height: 2880});
assert.equal(status.currentQualification(node({qualification_epoch: 4}), epochFour).label, "Pending");
assert.equal(status.currentQualification(node(), epochFour).label, "Unavailable");

assert.equal(status.rewardQualification(node({qualified_for_source_epoch: true})).label, "Qualified");
assert.equal(status.rewardQualification(node()).label, "Not qualified");
assert.equal(status.rewardQualification(node({effective_epoch: 3})).label, "Unavailable");
assert.equal(status.rewardQualification(node({source_qualification_availability: "unavailable"})).label, "Unavailable");
assert.equal(status.rewardQualification(node({qualified_for_source_epoch: null})).label, "Unavailable");

assert.equal(status.registration(node()).label, "Registration active");
assert.equal(status.registration(node({protocol_active: false})).label, "Registration inactive");
assert.equal(status.registration(node({protocol_active: null})).label, "Registration unavailable");

console.log("EPoSe presentation-state tests passed");
