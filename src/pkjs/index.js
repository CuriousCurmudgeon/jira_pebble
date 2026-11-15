var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var jira = require('./jira');
var settings = require('./settings');

// Handle Clay's events ourselves so the credentials stay on the phone
// instead of being sent to the watch.
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

// In the emulator, Clay routes the config page through an old Pebble S3 site
// in a ~300 KB URL, which some browsers never open. Hand pebble-tool a data:
// URL instead; it opens the page as a local file with ?return_to= added.
var EMULATOR_PROXY = 'http://clay.pebble.com.s3-website-us-west-2.amazonaws.com/#';

function configUrl() {
  var url = clay.generateUrl();
  if (url.indexOf(EMULATOR_PROXY) !== 0) {
    return url;
  }
  var html = decodeURIComponent(url.slice(EMULATOR_PROXY.length)).replace(
    '"$$RETURN_TO$$"',
    function() {
      return 'new URLSearchParams(location.search).get("return_to")';
    });
  return 'data:text/html;charset=utf-8,' + encodeURIComponent(html);
}

function send(message) {
  Pebble.sendAppMessage(message, null, function(error) {
    console.log('AppMessage failed: ' + JSON.stringify(error));
  });
}

function refresh() {
  var current = settings.load();
  if (!settings.isComplete(current)) {
    send({ Error: 'Open the app settings on your phone to sign in.' });
    return;
  }

  jira.getMyself(current, function(error, user) {
    if (error) {
      console.log('getMyself failed: ' + error);
      send({ Error: error });
    } else {
      send({ DisplayName: user.displayName });
    }
  });
}

Pebble.addEventListener('ready', refresh);

Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL(configUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) {
    return;
  }
  // Saves the values to localStorage; the return value is not needed.
  clay.getSettings(e.response, false);
  refresh();
});
