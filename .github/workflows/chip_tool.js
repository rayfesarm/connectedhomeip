const readline = require('readline')
const { ArmApi, ApiClient } = require('@arm-avh/avh-api');

const BearerAuth = ApiClient.instance.authentications['BearerAuth']
const api = new ArmApi()

const path = require("path");
const fs = require("fs");

async function main() {
    const apiToken = process.env.API_TOKEN || await prompt('Please enter AVH API Token: ')
    
    console.log('Logging in...');
    const authInfo = await api.v1AuthLogin({ apiToken });
    BearerAuth.accessToken = authInfo.token
    
    console.log('Listing projects...');
    let projects = await api.v1GetProjects();
    let project = projects[0];
    
    console.log("Getting Chip Tool instance...");
    let instances = await api.v1GetInstances()
    let instance = instances[0];
    
    console.log("Get Websocket...");
    let url = await api.instance.v1console();
    mySocket = new WebSocket(url);
	
    console.log("Testing...");
    mySocket.send("This is a test");

/*
    for(let loop=0; loop<5; loop++>){
        console.log("Turn light on...");
        mySocket.send("./out/debug/chip-tool onoff on 0x11 1");
        
        console.log("Wait 3 seconds...");
        await delay(3000);

        console.log("Turn light off...");
        mySocket.send("./out/debug/chip-tool onoff off 0x11 1");

        console.log("Wait 3 seconds...");
        await delay(3000);
    }
*/    
    console.log('Run completed');
    return;
}

main().catch((err) => {
    console.error(err);
});
