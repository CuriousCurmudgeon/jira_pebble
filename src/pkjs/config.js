module.exports = [
  {
    "type": "heading",
    "defaultValue": "Jira"
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Account"
      },
      {
        "type": "input",
        "messageKey": "SiteUrl",
        "label": "Site",
        "description": "For example, yourcompany.atlassian.net",
        "attributes": {
          "placeholder": "yourcompany.atlassian.net",
          "type": "text",
          "inputmode": "url",
          "autocapitalize": "off",
          "autocorrect": "off"
        }
      },
      {
        "type": "input",
        "messageKey": "Email",
        "label": "Email",
        "attributes": {
          "type": "email",
          "autocapitalize": "off",
          "autocorrect": "off"
        }
      },
      {
        "type": "input",
        "messageKey": "ApiToken",
        "label": "API token",
        "description": "Create one with scopes at id.atlassian.com/manage-profile/security/api-tokens, with read:jira-work, read:jira-user, read:board-scope:jira-software, read:board-scope.admin:jira-software, read:project:jira and read:sprint:jira-software. Tokens expire, so paste a new one here when it does.",
        "attributes": {
          "type": "password",
          "autocapitalize": "off",
          "autocorrect": "off"
        }
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Sprint"
      },
      {
        "type": "input",
        "messageKey": "Board",
        "label": "Board",
        "description": "The board whose active sprint to show: its name, its ID, or its project's key.",
        "attributes": {
          "placeholder": "ENG",
          "autocapitalize": "off",
          "autocorrect": "off"
        }
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save"
  }
];
