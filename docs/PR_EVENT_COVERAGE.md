# Pull Request Event Coverage Matrix

| Event Type | Support Status | Identity/ID | Timestamp Field | Actor/Nested Fields | Test Coverage | Notes |
|------------|----------------|-------------|-----------------|---------------------|---------------|-------|
| `added_to_project` | Planned (#298) | `id` | `created_at` | `actor.login` / `project_card` | Generic fallback | Project family |
| `assigned` | Supported | `id` | `created_at` | `actor.login` / `assignee` | Yes | |
| `automatic_base_change_failed` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback | |
| `automatic_base_change_succeeded` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback | |
| `base_ref_changed` | Planned (#297) | `id` | `created_at` | `actor.login` | Generic fallback | Branch lifecycle |
| `base_ref_force_pushed` | Planned (#297) | `id` | `created_at` | `actor.login` | Generic fallback | Branch lifecycle |
| `blocked` | Not applicable | N/A | N/A | N/A | None | Issue-only event |
| `closed` | Supported | `id` | `created_at` | `actor.login` | Yes | |
| `commented` | Supported | `id` | `created_at` | `user.login` | Yes | Standard comments |
| `committed` | Supported | `sha` | `author.date` | `author.name` | Yes | No `id` or `created_at` |
| `connected` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback | |
| `convert_to_draft` | Planned (#297) | `id` | `created_at` | `actor.login` | Generic fallback | PR lifecycle |
| `converted_note_to_issue` | Not applicable | N/A | N/A | N/A | None | Issue-only project event |
| `converted_to_discussion` | Not applicable | N/A | N/A | N/A | None | Issue-only event |
| `cross-referenced` | Planned (#298) | None | `created_at` | `actor.login` / `source` | Yes | Missing `id`, nested `source` |
| `demilestoned` | Planned (#298) | `id` | `created_at` | `actor.login` / `milestone` | Generic fallback | Metadata family |
| `deployed` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback | |
| `deployment_environment_changed` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback | |
| `disconnected` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback | |
| `head_ref_deleted` | Planned (#297) | `id` | `created_at` | `actor.login` | Generic fallback | Branch lifecycle |
| `head_ref_force_pushed` | Planned (#297) | `id` | `created_at` | `actor.login` | Generic fallback | Branch lifecycle |
| `head_ref_restored` | Planned (#297) | `id` | `created_at` | `actor.login` | Generic fallback | Branch lifecycle |
| `labeled` | Supported | `id` | `created_at` | `actor.login` / `label.name` | Yes | |
| `locked` | Planned (#297) | `id` | `created_at` | `actor.login` | Generic fallback | PR lifecycle |
| `marked_as_duplicate` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback | |
| `mentioned` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback | |
| `merged` | Supported | `id` | `created_at` | `actor.login` / `commit_id` | Yes | |
| `milestoned` | Planned (#298) | `id` | `created_at` | `actor.login` / `milestone` | Generic fallback | Metadata family |
| `moved_columns_in_project` | Planned (#298) | `id` | `created_at` | `actor.login` / `project_card` | Generic fallback | Project family |
| `pinned` | Not applicable | N/A | N/A | N/A | None | Issue-only event |
| `ready_for_review` | Planned (#297) | `id` | `created_at` | `actor.login` | Generic fallback | PR lifecycle |
| `referenced` | Planned (#298) | `commit_id` | `created_at` | `actor.login` / `commit_url` | Generic fallback | Reference family |
| `removed_from_project` | Planned (#298) | `id` | `created_at` | `actor.login` / `project_card` | Generic fallback | Project family |
| `renamed` | Planned (#298) | `id` | `created_at` | `actor.login` / `rename` | Generic fallback | Metadata family |
| `reopened` | Supported | `id` | `created_at` | `actor.login` | Yes | |
| `review_dismissed` | Planned (#297) | `id` | `created_at` | `actor.login` / `dismissed_review` | Yes | Has nested fields |
| `review_request_removed` | Planned (#297) | `id` | `created_at` | `actor.login` / `requested_reviewer` | Generic fallback | Review family |
| `review_requested` | Planned (#297) | `id` | `created_at` | `actor.login` / `requested_reviewer` | Generic fallback | Review family |
| `reviewed` | Planned (#297) | `id` | `submitted_at` | `user.login` / `body`, `state` | Yes | Timestamp is `submitted_at`, actor is `user` |
| `subscribed` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback | |
| `transferred` | Not applicable | N/A | N/A | N/A | None | Issue-only event |
| `unassigned` | Supported | `id` | `created_at` | `actor.login` / `assignee` | Yes | |
| `unlabeled` | Supported | `id` | `created_at` | `actor.login` / `label.name` | Yes | |
| `unlocked` | Planned (#297) | `id` | `created_at` | `actor.login` | Generic fallback | PR lifecycle |
| `unmarked_as_duplicate` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback | |
| `unpinned` | Not applicable | N/A | N/A | N/A | None | Issue-only event |
| `unsubscribed` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback | |
| `user_blocked` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback | |
