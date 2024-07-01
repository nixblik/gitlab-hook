# Introduction {#mainpage}

Gitlab-hook is a configurable HTTP(S) server that receives
[Webhook events from Gitlab](https://docs.gitlab.com/ee/user/project/integrations/webhook_events.html)
and processes them. If the event matches the configured criteria, gitlab-hook
executes the configured action. Typically, it executes a custom script.
