const readline = require('readline')
const { ArmApi, ApiClient } = require('@arm-avh/avh-api');
const W3CWebSocket = require('websocket').w3cwebsocket;

const BearerAuth = ApiClient.instance.authentications['BearerAuth']
const api = new ArmApi()

// const path = require("path");
// const fs = require("fs");

function delay(time) {
	return new Promise(resolve => setTimeout(resolve, time));
}

async function main() {
	const apiToken = process.env.API_TOKEN
	
	console.log('Logging in...');
	const authInfo = await api.v1AuthLogin({ apiToken });
	BearerAuth.accessToken = authInfo.token
	
	console.log("Verify instance...");
	let instances = await api.v1GetInstances()
	let instance = instances[0];
	console.log(`Instance: %s`, instance.name);
	
	console.log("Wait 20s for chip-lighting-app to initialize...");
	await delay(20000);
	
	console.log("Open Websocket...");
	let url = await api.v1GetInstanceConsole(instance.id);
	var mySocket = await new W3CWebSocket(url.url);
	console.log(`UEL: %s`, url.url);
	
	mySocket.onopen = function() {
		console.log(`WebSocket open...`);};
	mySocket.onerror = function() {
		console.log('Connection Error')};

	console.log("Wait 1 second...");
	await delay(1000);

	console.log("Turn light on...");
	mySocket.send("./out/debug/chip-tool onoff on 0x11 1\n");
	
	console.log("Wait 3 seconds...");
	await delay(3000);

	console.log("Turn light off...");
	mySocket.send("./out/debug/chip-tool onoff off 0x11 1\n");

	console.log("Wait 3 seconds...");
	await delay(3000);

	console.log("Turn light on again...");
	mySocket.send("./out/debug/chip-tool onoff on 0x11 1\n");

	console.log("Wait 3 seconds...");
	await delay(3000);

	console.log("Turn light off again...");
	mySocket.send("./out/debug/chip-tool onoff off 0x11 1\n");

	console.log('Close WebSocket...');
	mySocket.close();
	
	return;
}

main().catch((err) => {
    console.error(err);
});
