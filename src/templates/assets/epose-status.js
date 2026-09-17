(function (root, factory) {
    "use strict";
    var api = factory();
    if (typeof module === "object" && module.exports) module.exports = api;
    if (root) root.QwcEposeStatus = api;
}(typeof window !== "undefined" ? window : this, function () {
    "use strict";

    // Consensus parameters mirrored by protocol version. Unknown versions fail
    // closed instead of guessing when qualification becomes final.
    var PROTOCOL_PARAMETERS = Object.freeze({
        2: Object.freeze({qualificationAnchorDepth: 60})
    });

    function integer(value) {
        if (value === null || value === undefined || value === "" || typeof value === "boolean") {
            return null;
        }
        var parsed = Number(value);
        return Number.isSafeInteger(parsed) && parsed >= 0 ? parsed : null;
    }

    function result(key, detail) {
        var labels = {
            pending: "Pending",
            qualified: "Qualified",
            not_qualified: "Not qualified",
            not_participating: "Not participating",
            unavailable: "Unavailable"
        };
        return {key: key, label: labels[key], detail: detail || ""};
    }

    function snapshotAnchor(payload) {
        if (!payload || payload.snapshot_consistency !== "anchored") return null;
        var blockCount = integer(payload.snapshot_block_count);
        var tipHeight = integer(payload.snapshot_tip_height);
        var tipHash = payload.snapshot_tip_hash;
        if (blockCount === null || blockCount === 0
                || tipHeight === null || blockCount - 1 !== tipHeight
                || typeof tipHash !== "string"
                || !/^[0-9a-f]{64}$/.test(tipHash)) {
            return null;
        }
        return {blockCount: blockCount, tipHeight: tipHeight, tipHash: tipHash};
    }

    function sameAnchor(lhs, rhs) {
        return lhs !== null && rhs !== null
                && lhs.blockCount === rhs.blockCount
                && lhs.tipHeight === rhs.tipHeight
                && lhs.tipHash === rhs.tipHash;
    }

    function snapshotContext(info, nodes, rewards) {
        // observed_at_unix cannot prove chain ordering. Only backend-verified
        // canonical height/hash anchors may combine independently delivered JSON.
        var unavailable = {
            current: false, currentFinal: false, rewards: false,
            currentEpoch: null, sourceEpoch: null
        };
        if (!info || !nodes) return unavailable;

        var infoAnchor = snapshotAnchor(info);
        var nodesAnchor = snapshotAnchor(nodes);
        var rewardsAnchor = snapshotAnchor(rewards);
        var currentEpoch = integer(info.current_epoch);
        var nodesEpoch = integer(nodes.current_epoch);
        var sourceEpoch = integer(nodes.source_epoch);
        var tipHeight = integer(info.observer_tip_height);
        if (currentEpoch === null || nodesEpoch === null || tipHeight === null
                || !sameAnchor(infoAnchor, nodesAnchor)
                || infoAnchor.tipHeight !== tipHeight
                || nodesEpoch !== currentEpoch) {
            return unavailable;
        }

        var rewardsEpoch = integer(rewards && rewards.epoch);
        var rewardHeight = integer(rewards && rewards.height);
        var rewardSnapshotsMatch = sourceEpoch !== null && rewardsEpoch !== null
                && rewardHeight !== null
                && sameAnchor(infoAnchor, rewardsAnchor)
                && rewardHeight === infoAnchor.blockCount
                && rewardsEpoch < Number.MAX_SAFE_INTEGER
                && sourceEpoch === rewardsEpoch
                && rewardsEpoch + 1 === currentEpoch;
        return {
            current: true,
            currentFinal: true,
            rewards: rewardSnapshotsMatch,
            currentEpoch: currentEpoch,
            sourceEpoch: rewardSnapshotsMatch ? sourceEpoch : null
        };
    }

    function qualificationContext(info) {
        if (!info || info.qualification_availability !== "current") {
            return {phase: "unavailable", closeHeight: null, tipHeight: null};
        }
        var protocolVersion = integer(info.protocol_version);
        var epochEnd = integer(info.epoch_end_height);
        var tipHeight = integer(info.observer_tip_height);
        var parameters = PROTOCOL_PARAMETERS[protocolVersion];
        if (!parameters || epochEnd === null || tipHeight === null
                || epochEnd < parameters.qualificationAnchorDepth) {
            return {phase: "unavailable", closeHeight: null, tipHeight: tipHeight};
        }
        var closeHeight = epochEnd - parameters.qualificationAnchorDepth;
        return {
            phase: tipHeight >= closeHeight ? "closed" : "pending",
            closeHeight: closeHeight,
            tipHeight: tipHeight
        };
    }

    function currentQualification(node, info, snapshots) {
        if (!node) return result("unavailable");
        if (!snapshots || snapshots.current !== true) {
            return result("unavailable", "Qualification snapshots are inconsistent");
        }
        var currentEpoch = integer(info && info.current_epoch);
        var qualificationEpoch = integer(node.qualification_epoch);
        if (currentEpoch === null || qualificationEpoch === null
                || snapshots.currentEpoch !== currentEpoch
                || currentEpoch !== qualificationEpoch
                || node.qualification_availability !== "current") {
            return result("unavailable");
        }
        if (node.qualified_for_current_epoch === true) {
            return result("qualified", "Core-confirmed qualification");
        }
        if (node.protocol_active === false) {
            return result("not_participating", "Registration is not active in this epoch");
        }
        if (node.protocol_active !== true
                || node.qualified_for_current_epoch !== false) {
            return result("unavailable");
        }
        var context = qualificationContext(info);
        if (context.phase === "pending") {
            return result("pending", "Qualification has not been finalized");
        }
        if (context.phase === "closed") {
            if (snapshots.currentFinal !== true) {
                return result("unavailable", "Final node status cannot be matched to the closing snapshot");
            }
            return result("not_qualified", "Final qualification did not include this node");
        }
        return result("unavailable");
    }

    function rewardQualification(node, snapshots) {
        if (!snapshots || snapshots.rewards !== true) {
            return result("unavailable", "Reward snapshots do not share a coherent epoch");
        }
        if (!node || node.source_qualification_availability !== "finalized") {
            return result("unavailable");
        }
        var sourceEpoch = integer(node.source_qualification_epoch);
        if (sourceEpoch === null || snapshots.sourceEpoch !== sourceEpoch) {
            return result("unavailable");
        }
        if (node.qualified_for_source_epoch === true) {
            return result("qualified", "Included in the finalized source-epoch set");
        }
        if (node.qualified_for_source_epoch !== false) return result("unavailable");

        var effectiveEpoch = integer(node.effective_epoch);
        var expiryEpoch = integer(node.expiry_epoch);
        if (effectiveEpoch === null || expiryEpoch === null || expiryEpoch < effectiveEpoch) {
            return result("unavailable");
        }
        if (sourceEpoch < effectiveEpoch || sourceEpoch >= expiryEpoch) {
            return result("unavailable", "Source-epoch participation cannot be verified");
        }
        return result("not_qualified", "Final source-epoch qualification did not include this node");
    }

    function registration(node) {
        if (!node) return {label: "Registration unavailable", active: null};
        if (node.protocol_active === true) return {label: "Registration active", active: true};
        if (node.protocol_active === false) return {label: "Registration inactive", active: false};
        return {label: "Registration unavailable", active: null};
    }

    function servicePeriod(node) {
        if (!node) return "Unavailable";
        var sequence = integer(node.descriptor_sequence);
        var effectiveEpoch = integer(node.effective_epoch);
        var expiryEpoch = integer(node.expiry_epoch);
        if (sequence === null || effectiveEpoch === null || expiryEpoch === null
                || expiryEpoch <= effectiveEpoch) {
            return "Unavailable";
        }
        var lastActiveEpoch = expiryEpoch - 1;
        var activeRange = effectiveEpoch === lastActiveEpoch
                ? "Epoch " + effectiveEpoch
                : "Epochs " + effectiveEpoch + "–" + lastActiveEpoch;
        return "Sequence " + sequence + " · " + activeRange
                + " · expires at start of Epoch " + expiryEpoch;
    }

    return Object.freeze({
        protocolParameters: PROTOCOL_PARAMETERS,
        nonNegativeInteger: integer,
        snapshotAnchor: snapshotAnchor,
        snapshotContext: snapshotContext,
        qualificationContext: qualificationContext,
        currentQualification: currentQualification,
        rewardQualification: rewardQualification,
        registration: registration,
        servicePeriod: servicePeriod
    });
}));
