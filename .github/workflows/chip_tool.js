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
  let rpi_instance = instances.find((instance) => instance.name === "Chip Tool");

	console.log("Wait for instance to be on");
         await rpi_instance.waitForState('on');

// Print dump of the device console log
    console.log("Console output:\n");
    let data = await rpi_instance.consoleLog();
    console.log(data);
    
    // Delete instance
    // console.log('Deleting the instance...');
    // instance.destroy();
    
    console.log('Run completed');
    return;
}
main().catch((err) => {
    console.error(err);
});
