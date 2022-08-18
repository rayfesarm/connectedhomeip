const readline = require('readline')
const { ArmApi, ApiClient } = require('@arm-avh/avh-api');
const W3CWebSocket = require('websocket').w3cwebsocket;

const BearerAuth = ApiClient.instance.authentications['BearerAuth']
const api = new ArmApi()

function delay(time) {
	return new Promise(resolve => setTimeout(resolve, time));
}

async function main() {
	console.log('Logging in...');
	const apiToken = process.env.API_TOKEN
	const authInfo = await api.v1AuthLogin({ apiToken });
	BearerAuth.accessToken = authInfo.token
	
	console.log("Locate instance named chip-tool...");
	console.log("[User must edit JavaScript if instance has different name (or >10000 instances)]");
	let instances = await api.v1GetInstances()
	for(let i=0; i<10000; i++){
		var instance = instances[i];
		if (instance.name == "chip-tool") break;
	}
	
	console.log("Open Websocket...");
	let url = await api.v1GetInstanceConsole(instance.id);
	var mySocket = await new W3CWebSocket(url.url);
	
	mySocket.onopen = function() {
		console.log(`WebSocket open.`);};
	mySocket.onerror = function() {
		console.log('WebSocket Connection Error.')};
	
	console.log("Wait 60s to ensure lighting-app is initialized...");
	await delay(60000);
	
	console.log("Turn light on...");
	mySocket.send("./out/debug/chip-tool onoff on 0x11 1\n");
	
	console.log("Wait 5 seconds...");
	await delay(5000);
	
	console.log("Turn light off...");
	mySocket.send("./out/debug/chip-tool onoff off 0x11 1\n");
	
	console.log("Wait 5 seconds...");
	await delay(5000);
	
	console.log("Turn light on again...");
	mySocket.send("./out/debug/chip-tool onoff on 0x11 1\n");
	
	console.log("Wait 5 seconds...");
	await delay(5000);
	
	console.log("Turn light off again...");
	mySocket.send("./out/debug/chip-tool onoff off 0x11 1\n");
	
	console.log("Wait 5 seconds...");
	await delay(5000);
	
	console.log('Closing WebSocket...');
	mySocket.close();
		mySocket.onclose = function() {
		console.log(`WebSocket closed.`);};
	mySocket.onerror = function() {
		console.log('WebSocket Error.')};
	
	return;
}

main().catch((err) => {
    console.error(err);
});
