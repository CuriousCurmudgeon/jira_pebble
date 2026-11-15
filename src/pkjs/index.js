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

// The watch holds at most this many issues per list; see MAX_ISSUES in main.c.
var MAX_ISSUES = 50;

// The most sprint issues the phone loads from Jira.
var MAX_SPRINT_ISSUES = 500;

// Byte limits, including the terminating NUL, of the watch's string fields.
var LIMITS = { key: 16, summary: 160, status: 32, priority: 16, assignee: 32, sprint: 32 };

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

function categoryOf(issue) {
  var status = (issue.fields && issue.fields.status) || {};
  var category = CATEGORIES[status.statusCategory && status.statusCategory.key];
  return category === undefined ? CATEGORIES['new'] : category;
}

function issueMessage(issue, index) {
  var fields = issue.fields || {};
  var status = fields.status || {};
  return {
    IssueIndex: index,
    IssueKey: fit(issue.key, LIMITS.key),
    IssueSummary: fit(fields.summary, LIMITS.summary),
    IssueStatus: fit(status.name, LIMITS.status),
    IssueCategory: categoryOf(issue),
    IssuePriority: fit(fields.priority && fields.priority.name, LIMITS.priority),
    IssueAssignee: fit(fields.assignee && fields.assignee.displayName, LIMITS.assignee),
    IssueUrgent: isUrgent(fields) ? 1 : 0
  };
}

var SEND_RETRIES = 3;
var RETRY_DELAY_MS = 200;

// Bumped by each sendAll(), so a newer batch cancels the rest of an older one.
var sendGeneration = 0;

// AppMessage sends one message at a time, so send each after the last is
// acked. A message can be refused while the watch is sending, so retry.
function sendAll(messages) {
  var generation = ++sendGeneration;

  function sendNext(index, retries) {
    if (generation !== sendGeneration || index >= messages.length) {
      return;
    }
    Pebble.sendAppMessage(messages[index], function() {
      sendNext(index + 1, SEND_RETRIES);
    }, function(error) {
      if (retries > 0) {
        setTimeout(function() {
          sendNext(index, retries - 1);
        }, RETRY_DELAY_MS);
      } else {
        console.log('AppMessage failed: ' + JSON.stringify(error));
      }
    });
  }

  sendNext(0, SEND_RETRIES);
}

function sendError(text) {
  sendAll([{ Error: fit(text, 128) }]);
}

// The sprint's issues, loaded when the app opens; null until they arrive.
var sprintIssues = null;
// A category the watch asked for before the issues arrived.
var pendingCategory = null;

function daysLeft(sprint) {
  var end = Date.parse(sprint.endDate);
  return isNaN(end) ? 0 : Math.ceil((end - Date.now()) / (24 * 60 * 60 * 1000));
}

function sprintMessage(sprint, issues) {
  var counts = [0, 0, 0];
  issues.forEach(function(issue) {
    counts[categoryOf(issue)]++;
  });
  return {
    SprintName: fit(sprint.name, LIMITS.sprint),
    SprintDaysLeft: daysLeft(sprint),
    CountToDo: counts[CATEGORIES['new']],
    CountInProgress: counts[CATEGORIES['indeterminate']],
    CountDone: counts[CATEGORIES['done']]
  };
}

function categoryMessages(category) {
  var issues = sprintIssues.filter(function(issue) {
    return categoryOf(issue) === category;
  });
  var shown = issues.slice(0, MAX_ISSUES);
  return [{ ListCategory: category, ListCount: issues.length }]
    .concat(shown.map(issueMessage));
}

function refresh() {
  sprintIssues = null;
  var current = settings.load();
  if (!settings.isComplete(current)) {
    sendError('Open the app settings on your phone to sign in and pick a board.');
    return;
  }

  jira.getSprintIssues(current, current.board, MAX_SPRINT_ISSUES,
    function(error, sprint, issues) {
      if (error) {
        console.log('getSprintIssues failed: ' + error);
        sendError(error);
        return;
      }
      sprintIssues = issues || [];
      var messages = [sprintMessage(sprint, sprintIssues)];
      if (pendingCategory !== null) {
        messages = messages.concat(categoryMessages(pendingCategory));
        pendingCategory = null;
      }
      sendAll(messages);
    });
}

Pebble.addEventListener('appmessage', function(e) {
  var category = e.payload.RequestCategory;
  if (category === undefined) {
    return;
  }
  if (sprintIssues) {
    sendAll(categoryMessages(category));
  } else {
    pendingCategory = category;
  }
});

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
