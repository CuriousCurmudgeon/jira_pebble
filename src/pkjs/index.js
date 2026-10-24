var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var jira = require('./jira');
var settings = require('./settings');

// Handle Clay's events ourselves so the credentials stay on the phone
// instead of being sent to the watch.
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

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
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) {
    return;
  }
  // Saves the values to localStorage; the return value is not needed.
  clay.getSettings(e.response, false);
  refresh();
});
