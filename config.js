var api = "https://api.qwertycoin.org/sslnode";
var apiList = ["https://api.qwertycoin.org/sslnode","https://pr01.myqwertycoin.com/sync","https://pr02.myqwertycoin.com/sync"];
var blockTargetInterval = 120;
var coinUnits = 100000000;
var symbol = 'QWC';
var refreshDelay = 30000;
var addressPattern = new RegExp("^QWC[123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz]{95}$");
// pools stats by MainCoins
var poolsStat = 
	[
		["pool.qwertycoin.org","https://pool.qwertycoin.org:8119/stats"],
		["superblockchain.con-ip.com/qwc","https://superblockchain.con-ip.com:8333/stats"],
		["qwc.superpools.online","https://qwc.cryptonote.club:8199/stats"],
		["easyhash.pro/qwc","https://easyhash.pro/qwc/api/stats"],
		["qwc.cryptonote.club","https://qwc.cryptonote.club:8199/stats"],
		["qwertycoin.fairhash.org","https://qwertycoin.fairhash.org/api/stats"],
		["newpool.pw/qwc_b2b","https://minenice.newpool.pw:8205/stats"],
		["dxpool.com","https://api.qwertycoin.org/pools/dxpool-com"],
		["m2pool.eu","http://103.253.41.40:9981/stats"],
		//["qwertycoin.herominers.com","https://qwertycoin.herominers.com/api/stats"]
		//["qwertypool.com","https://qwertypool.com:7776/stats"]
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
		["node-07.qwertycoin.org:8197"]
    ];
