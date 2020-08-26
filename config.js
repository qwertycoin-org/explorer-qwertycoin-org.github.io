var api = "https://api.qwertycoin.org/sslnode";
var apiList = [];
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
		["88.99.85.223:8197"],
		["94.130.187.117:8197"],
		["node-01.qwertycoin.org:8197"],
		["135.181.36.222:8197"],
		["195.201.25.118:8197"],
		["node-02.qwertycoin.org:8197"],
		["195.201.29.64:8197"],
		["node-03.qwertycoin.org:8197"]
		["78.47.85.215:8197"],
		["node-04.qwertycoin.org:8197"],
		["116.203.121.22:8197"],
		["node-05.qwertycoin.org:8197"],
		["148.251.115.233:8197"],
		["213.136.89.252:8197"],
		["50.78.90.86:8197"],
		["173.212.197.11:8197"],
		["80.211.204.60:8197"],
		["135.181.36.222:8197"]
    ];
