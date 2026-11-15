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

function query(params) {
  return Object.keys(params).map(function(name) {
    return encodeURIComponent(name) + '=' + encodeURIComponent(params[name]);
  }).join('&');
}

// A board's ID never changes, so look each board name up once per site.
var BOARD_ID_PREFIX = 'board-id:';

function pickBoard(boards, name) {
  var exact = boards.filter(function(board) {
    return board.name.toLowerCase() === name.toLowerCase();
  });
  if (exact.length === 1) {
    return exact[0];
  }
  return boards.length === 1 ? boards[0] : null;
}

// Takes a board ID, a board name or the key of the project the board belongs to.
function getBoardId(settings, board, callback) {
  if (/^\d+$/.test(board)) {
    callback(null, board);
    return;
  }
  var cacheKey = BOARD_ID_PREFIX + settings.siteUrl + ':' + board;
  var cached = localStorage.getItem(cacheKey);
  if (cached) {
    callback(null, cached);
    return;
  }

  function found(match) {
    localStorage.setItem(cacheKey, String(match.id));
    callback(null, String(match.id));
  }

  var byName = '/rest/agile/1.0/board?' + query({ name: board });
  request(settings, 'GET', byName, function(error, data) {
    if (error) {
      callback(error);
      return;
    }
    var boards = (data && data.values) || [];
    var match = pickBoard(boards, board);
    if (match) {
      found(match);
      return;
    }
    if (boards.length > 1) {
      callback(boards.length + ' boards match "' + board + '". Enter the board ID.');
      return;
    }
    var byProject = '/rest/agile/1.0/board?' + query({ projectKeyOrId: board, type: 'scrum' });
    request(settings, 'GET', byProject, function(error, data) {
      var boards = (!error && data && data.values) || [];
      if (boards.length === 1) {
        found(boards[0]);
      } else {
        callback(boards.length > 1 ?
          boards.length + ' boards match "' + board + '". Enter the board ID.' :
          'No board named "' + board + '"');
      }
    });
  });
}

// Calls back with the board's active sprint, or null if none is active.
function getActiveSprint(settings, boardId, callback) {
  var path = '/rest/agile/1.0/board/' + boardId + '/sprint?' + query({ state: 'active' });
  request(settings, 'GET', path, function(error, data) {
    if (error) {
      callback(error === 'Jira error 400' ? 'The board has no sprints' : error);
      return;
    }
    callback(null, (data && data.values && data.values[0]) || null);
  });
}

var SEARCH_PAGE_SIZE = 100;

// Calls back with up to maxIssues issues, following nextPageToken across pages.
function searchAll(settings, jql, fields, maxIssues, callback) {
  var issues = [];

  function fetchPage(pageToken) {
    var params = { jql: jql, fields: fields, maxResults: SEARCH_PAGE_SIZE };
    if (pageToken) {
      params.nextPageToken = pageToken;
    }
    request(settings, 'GET', '/rest/api/3/search/jql?' + query(params), function(error, data) {
      if (error) {
        callback(error);
        return;
      }
      issues = issues.concat((data && data.issues) || []);
      if (data && data.nextPageToken && !data.isLast && issues.length < maxIssues) {
        fetchPage(data.nextPageToken);
      } else {
        callback(null, issues.slice(0, maxIssues));
      }
    });
  }

  fetchPage(null);
}

// Calls back with the active sprint of the board and its issues in rank order.
function getSprintIssues(settings, board, maxIssues, callback) {
  getBoardId(settings, board, function(error, boardId) {
    if (error) {
      callback(error);
      return;
    }
    getActiveSprint(settings, boardId, function(error, sprint) {
      if (error || !sprint) {
        callback(error || 'No active sprint');
        return;
      }
      searchAll(settings, 'sprint = ' + sprint.id + ' ORDER BY Rank ASC',
        'summary,status,priority,assignee', maxIssues, function(error, issues) {
          callback(error, sprint, issues);
        });
    });
  });
}

module.exports = {
  getSprintIssues: getSprintIssues
};
