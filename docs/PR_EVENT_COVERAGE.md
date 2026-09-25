# Pull Request Event Coverage Matrix

| Event Type | Support Status | Identity/ID | Timestamp Field | Actor/Nested Fields | Notes |
|------------|----------------|-------------|-----------------|---------------------|-------|
| `added_to_project` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `assigned` | Supported | `id` | `created_at` | `actor.login` | |
| `automatic_base_change_failed` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `automatic_base_change_succeeded` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `base_ref_changed` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `base_ref_force_pushed` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `blocked` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `closed` | Supported | `id` | `created_at` | `actor.login` | |
| `commented` | Supported | `id` | `created_at` | `user.login` / `author.name` | |
| `committed` | Supported | `sha` | `author.date` | `user.login` / `author.name` | |
| `connected` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `convert_to_draft` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `converted_note_to_issue` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `converted_to_discussion` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `cross-referenced` | Planned (#297 / #298) | `id` | `created_at` | `actor.login` | |
| `demilestoned` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `deployed` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `deployment_environment_changed` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `disconnected` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `head_ref_deleted` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `head_ref_force_pushed` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `head_ref_restored` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `labeled` | Supported | `id` | `created_at` | `actor.login` | |
| `locked` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `marked_as_duplicate` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `mentioned` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `merged` | Supported | `id` | `created_at` | `actor.login` | |
| `milestoned` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `moved_columns_in_project` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `pinned` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `ready_for_review` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `referenced` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `removed_from_project` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `renamed` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `reopened` | Supported | `id` | `created_at` | `actor.login` | |
| `review_dismissed` | Planned (#297 / #298) | `id` | `created_at` | `actor.login` | |
| `review_request_removed` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `review_requested` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `reviewed` | Planned (#297 / #298) | `id` | `created_at` | `actor.login` | |
| `subscribed` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `transferred` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `unassigned` | Supported | `id` | `created_at` | `actor.login` | |
| `unlabeled` | Supported | `id` | `created_at` | `actor.login` | |
| `unlocked` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `unmarked_as_duplicate` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `unpinned` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `unsubscribed` | Safe Fallback | `id` | `created_at` | `actor.login` | |
| `user_blocked` | Safe Fallback | `id` | `created_at` | `actor.login` | |
