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

// The watch holds at most this many issues; see MAX_ISSUES in main.c.
var MAX_ISSUES = 25;

// Byte limits, including the terminating NUL, of the watch's string fields.
var LIMITS = { key: 16, summary: 160, status: 32, priority: 16, assignee: 32 };

// Matches the IssueCategory values in main.c.
var CATEGORIES = { 'new': 0, 'indeterminate': 1, 'done': 2 };

function utf8Length(text) {
  return unescape(encodeURIComponent(text)).length;
}

// Truncates text, with an ellipsis, so its UTF-8 encoding plus a NUL fits in
// maxBytes. Cuts between code points, so a surrogate pair is never split.
function fit(text, maxBytes) {
  text = text || '';
  if (utf8Length(text) < maxBytes) {
    return text;
  }
  var chars = Array.from(text);
  var budget = maxBytes - 1 - 3; // The ellipsis takes 3 bytes.
  var out = '';
  for (var i = 0; i < chars.length; i++) {
    if (utf8Length(out + chars[i]) > budget) {
      break;
    }
    out += chars[i];
  }
  return out + '\u2026';
}

// Blocked issues and the top priorities get a red stripe on the watch.
function isUrgent(fields) {
  var status = (fields.status && fields.status.name) || '';
  var priority = (fields.priority && fields.priority.name) || '';
  return /block/i.test(status) || /^(highest|blocker)$/i.test(priority);
}

function issueMessage(issue, index) {
  var fields = issue.fields || {};
  var status = fields.status || {};
  var category = CATEGORIES[status.statusCategory && status.statusCategory.key];
  return {
    IssueIndex: index,
    IssueKey: fit(issue.key, LIMITS.key),
    IssueSummary: fit(fields.summary, LIMITS.summary),
    IssueStatus: fit(status.name, LIMITS.status),
    IssueCategory: category === undefined ? CATEGORIES['new'] : category,
    IssuePriority: fit(fields.priority && fields.priority.name, LIMITS.priority),
    IssueAssignee: fit(fields.assignee && fields.assignee.displayName, LIMITS.assignee),
    IssueUrgent: isUrgent(fields) ? 1 : 0
  };
}

// AppMessage sends one message at a time, so send each after the last is acked.
function sendAll(messages) {
  if (messages.length === 0) {
    return;
  }
  Pebble.sendAppMessage(messages[0], function() {
    sendAll(messages.slice(1));
  }, function(error) {
    console.log('AppMessage failed: ' + JSON.stringify(error));
  });
}

function sendError(text) {
  sendAll([{ Error: fit(text, 128) }]);
}

function refresh() {
  var current = settings.load();
  if (!settings.isComplete(current)) {
    sendError('Open the app settings on your phone to sign in.');
    return;
  }

  jira.searchMyQueue(current, MAX_ISSUES, function(error, issues) {
    if (error) {
      console.log('searchMyQueue failed: ' + error);
      sendError(error);
      return;
    }
    issues = (issues || []).slice(0, MAX_ISSUES);
    sendAll([{ QueueCount: issues.length }].concat(issues.map(issueMessage)));
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
