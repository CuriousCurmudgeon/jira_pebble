// XMLHttpRequest on iOS has no timeout property, so time requests out ourselves.
var TIMEOUT_MS = 15000;

// Calls back with (error, parsed JSON). Pass auth to send basic auth.
function fetchJson(method, url, auth, callback) {
  var xhr = new XMLHttpRequest();
  var finished = false;

  function finish(error, data) {
    if (finished) {
      return;
    }
    finished = true;
    clearTimeout(timer);
    callback(error, data);
  }

  var timer = setTimeout(function() {
    xhr.abort();
    finish('Request timed out');
  }, TIMEOUT_MS);

  xhr.open(method, url, true);
  if (auth) {
    xhr.setRequestHeader('Authorization',
      'Basic ' + btoa(auth.email + ':' + auth.apiToken));
  }
  xhr.setRequestHeader('Accept', 'application/json');

  // On iOS the handler's event has only a type, so read results from xhr.
  xhr.onload = function() {
    if (xhr.status === 401 || xhr.status === 403) {
      finish('Login failed. Check email, token and its scopes.');
    } else if (xhr.status < 200 || xhr.status >= 300) {
      finish('Jira error ' + xhr.status);
    } else {
      try {
        finish(null, JSON.parse(xhr.responseText));
      } catch (e) {
        finish('Bad response from Jira');
      }
    }
  };
  xhr.onerror = function() {
    finish('Network error');
  };

  xhr.send();
}

// Scoped API tokens only work through Atlassian's API gateway, which
// identifies the site by its cloud ID.
var GATEWAY_URL = 'https://api.atlassian.com/ex/jira/';

// A site's cloud ID never changes, so look it up once per site.
var CLOUD_ID_PREFIX = 'cloud-id:';

function getCloudId(siteUrl, callback) {
  var cached = localStorage.getItem(CLOUD_ID_PREFIX + siteUrl);
  if (cached) {
    callback(null, cached);
    return;
  }
  fetchJson('GET', siteUrl + '/_edge/tenant_info', null, function(error, data) {
    if (error || !data || !data.cloudId) {
      callback('Could not find the Jira site ' + siteUrl.replace(/^https?:\/\//, ''));
      return;
    }
    localStorage.setItem(CLOUD_ID_PREFIX + siteUrl, data.cloudId);
    callback(null, data.cloudId);
  });
}

function request(settings, method, path, callback) {
  getCloudId(settings.siteUrl, function(error, cloudId) {
    if (error) {
      callback(error);
      return;
    }
    fetchJson(method, GATEWAY_URL + cloudId + path, settings, callback);
  });
}

var QUEUE_JQL = 'assignee = currentUser() AND statusCategory != Done ' +
  'ORDER BY priority DESC, updated DESC';

function query(params) {
  return Object.keys(params).map(function(name) {
    return encodeURIComponent(name) + '=' + encodeURIComponent(params[name]);
  }).join('&');
}

// Calls back with the raw issues of the user's open queue, highest priority first.
function searchMyQueue(settings, maxResults, callback) {
  var path = '/rest/api/3/search/jql?' + query({
    jql: QUEUE_JQL,
    fields: 'summary,status,priority,assignee',
    maxResults: maxResults
  });
  request(settings, 'GET', path, function(error, data) {
    callback(error, data && data.issues);
  });
}

module.exports = {
  searchMyQueue: searchMyQueue
};
