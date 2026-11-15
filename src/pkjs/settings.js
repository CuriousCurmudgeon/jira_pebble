// Clay saves the config page's values under this key when getSettings() runs.
var STORAGE_KEY = 'clay-settings';

function load() {
  var raw = localStorage.getItem(STORAGE_KEY);
  var stored = raw ? JSON.parse(raw) : {};
  return {
    siteUrl: normalizeSiteUrl(stored.SiteUrl),
    email: (stored.Email || '').trim(),
    apiToken: (stored.ApiToken || '').trim(),
    board: (stored.Board || '').trim()
  };
}

function isComplete(settings) {
  return Boolean(settings.siteUrl && settings.email && settings.apiToken && settings.board);
}

// Accepts "yourcompany", "yourcompany.atlassian.net" or a full URL.
function normalizeSiteUrl(value) {
  var url = (value || '').trim().replace(/\/+$/, '');
  if (!url) {
    return '';
  }
  if (!/^https?:\/\//i.test(url)) {
    url = 'https://' + url;
  }
  if (!/\./.test(url.replace(/^https?:\/\//i, ''))) {
    url += '.atlassian.net';
  }
  return url;
}

module.exports = {
  load: load,
  isComplete: isComplete
};
