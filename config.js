var api = "https://node-02.qwertycoin.org/sslnode";
var apiList = [
	"https://node-00.qwertycoin.org/sslnode",
	"https://node-01.qwertycoin.org/sslnode",
	"https://node-02.qwertycoin.org/sslnode",
	"https://node-03.qwertycoin.org/sslnode",
	"https://node-04.qwertycoin.org/sslnode",
	"https://node-06.qwertycoin.org/sslnode"
];
var blockTargetInterval = 120;
var coinUnits = 100000000;
var symbol = 'QWC';
var refreshDelay = 30000;
var addressPattern = new RegExp("^QWC[123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz]{95}$");
// pools stats by MainCoins
var poolsStat = 
	[
		["pool.qwertycoin.org","https://mining.qwertycoin.org:8119/stats"],
		["superblockchain.con-ip.com/qwc","https://superblockchain.con-ip.com:8333/stats"],
		["easyhash.pro/qwc","https://easyhash.pro/qwc/api/stats"],
		["qwertycoin.fairhash.org","https://qwertycoin.fairhash.org/api/stats"],
		["newpool.pw/qwc_b2b","https://minenice.newpool.pw:8205/stats"],
		["digging.qwertycoin.org","https://mining.qwertycoin.org:8119/stats"]
    ];
var nodesStat = 
	[
		["node-00.qwertycoin.org:8197"],
		["node-01.qwertycoin.org:8197"],
		["node-02.qwertycoin.org:8197"],
		["node-03.qwertycoin.org:8197"],
		["node-04.qwertycoin.org:8197"],
		["node-05.qwertycoin.org:8197"],
		["node-06.qwertycoin.org:8197"],
		["node-07.qwertycoin.org:8197"],
		["node-08.qwertycoin.org:8197"],
		["node-09.qwertycoin.org:8197"]
    ];
