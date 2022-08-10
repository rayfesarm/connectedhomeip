var AvhApi = require('@arm-avh/avh-api');

var api = new AvhApi.ArmApi()
var apiToken = {
  "apiToken": "71819844e83655e4131c.90b2b6b6dc26b791ac7df5c5a9db36651ca10172d24a80a4e13cf217c4a5748e530c73525e9a7a446a91c63a7216dee29520d7ab8a86449cc62ba02b45f0e3c1"
}; // {ApiToken} Authorization Data
api.v1AuthLogin(apiToken).then(function(data) {
  console.log('API called successfully. Returned data: ' + data);
}, function(error) {
  console.error(error);
});
