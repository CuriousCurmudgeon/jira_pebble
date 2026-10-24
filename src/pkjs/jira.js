// XMLHttpRequest on iOS has no timeout property, so time requests out ourselves.
var TIMEOUT_MS = 15000;

function request(settings, method, path, callback) {
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

  xhr.open(method, settings.siteUrl + path, true);
  xhr.setRequestHeader('Authorization',
    'Basic ' + btoa(settings.email + ':' + settings.apiToken));
  xhr.setRequestHeader('Accept', 'application/json');

  // On iOS the handler's event has only a type, so read results from xhr.
  xhr.onload = function() {
    if (xhr.status === 401 || xhr.status === 403) {
      finish('Login failed. Check email and token.');
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

function getMyself(settings, callback) {
  request(settings, 'GET', '/rest/api/3/myself', callback);
}

module.exports = {
  getMyself: getMyself
};
