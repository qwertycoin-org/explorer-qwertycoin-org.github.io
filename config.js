var api = 'https://api.qwertycoin.org/sslnode';
var apiList = ["https://api.qwertycoin.org/sslnode"];

var blockTargetInterval = 120;
var coinUnits = 100000000;
var symbol = 'QWC';
var refreshDelay = 30000;
var blocksPerPage = 20;
var whiteTheme = "css/light.css";
var nightTheme = "css/dark.css";
var addressPattern = new RegExp("^QWC[123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz]{95}$");
// pools stats by MainCoins
var poolsStat = 
	[
		["pool.qwertycoin.org","https://pool.qwertycoin.org:8119/stats"],
		["superblockchain.con-ip.com/qwc","https://superblockchain.con-ip.com:8333/stats"],
		["easyhash.pro/qwc","https://easyhash.pro/qwc/api/stats"],
		["qwertycoin.fairhash.org","https://qwertycoin.fairhash.org/api/stats"],
		["newpool.pw/qwc_b2b","https://minenice.newpool.pw:8205/stats"],
		["dxpool.com","https://api.qwertycoin.org/pools/dxpool-com"],
		["m2pool.eu","http://ro.qwc.m2pool.eu:9981/stats"],
		["qwertypool.com","https://qwertypool.com:7776/stats"]
    ];

	