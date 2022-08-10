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

// Get Websocket
    console.log("Websocket...");
    let urls = await api.v1console();
    let url = urls[0]
    mySocket = new WebSocket(url);
    mySocket.send("Hello Chip Tool!");
           
    console.log('Run completed');
    return;
}
main().catch((err) => {
    console.error(err);
});
