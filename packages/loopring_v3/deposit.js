const Web3 = require("web3");
const fs = require("fs");
const PrivateKeyProvider = require("truffle-privatekey-provider");

const privateKey = "3c3cb9b2fcab41e588d5aa0066928f855f2cf09e5c817fc41350eae9cfe8dc36";
const localUrl = "http://localhost:8545";
const provider = new PrivateKeyProvider(privateKey, localUrl);

const web3 = new Web3(provider);

async function sendTx() {
    const from = (await web3.eth.getAccounts())[0];
    try {
	await web3.eth.sendTransaction({
	    from,
	    to: from,
	    value: "0",
	    gasLimit: "100000",
	    gasPrice: "0"
	});
	
	const blockNumber = await web3.eth.getBlockNumber();
	console.log("blockNumber:", blockNumber);
	
	process.exit(0);
    } catch (err) {
	console.error(err);
	process.exit(1);
    }
}


sendTx();
