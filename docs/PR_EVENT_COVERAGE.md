# Pull Request Event Coverage Matrix

| Event Type | Support Status | Identity/ID | Timestamp Field | Actor/Nested Fields | Test Coverage | Notes |
|------------|----------------|-------------|-----------------|---------------------|---------------|-------|
| `added_to_project` | Planned (#298) | `id` | `created_at` | `actor.login` / `project_card` | Generic fallback representative: `review_dismissed` | Project family |
| `assigned` | Supported | `id` | `created_at` | `actor.login` / `assignee` | `testPullRequestTimelineSafeFallbackAndDeduplication` | |
| `automatic_base_change_failed` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback representative: `review_dismissed` | |
| `automatic_base_change_succeeded` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback representative: `review_dismissed` | |
| `base_ref_changed` | Planned (#297) | `id` | `created_at` | `actor.login` | Generic fallback representative: `review_dismissed` | Branch lifecycle |
| `base_ref_force_pushed` | Planned (#297) | `id` | `created_at` | `actor.login` | Generic fallback representative: `review_dismissed` | Branch lifecycle |
| `blocked` | Not applicable | N/A | N/A | N/A | None | Issue-only event (per github docs) |
| `closed` | Supported | `id` | `created_at` | `actor.login` | `testPullRequestTimelineSafeFallbackAndDeduplication` | |
| `commented` | Supported | `id` | `created_at` | `user.login` | `testPullRequestTimelineSafeFallbackAndDeduplication` | Standard comments |
| `committed` | Supported | `sha` | `author.date` | `author.name` | `testPullRequestTimelineSafeFallbackAndDeduplication` | No `id` or `created_at` |
| `connected` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback representative: `review_dismissed` | |
| `convert_to_draft` | Planned (#297) | `id` | `created_at` | `actor.login` | Generic fallback representative: `review_dismissed` | PR lifecycle |
| `converted_note_to_issue` | Not applicable | N/A | N/A | N/A | None | Issue-only project event (per github docs) |
| `converted_to_discussion` | Not applicable | N/A | N/A | N/A | None | Issue-only event (per github docs) |
| `cross-referenced` | Planned (#298) | None | `created_at` | `actor.login` / `source` | `testPullRequestTimelineMalformedIdsAndLaterPageFailure` | Missing `id`, nested `source` requires stable fingerprinting |
| `demilestoned` | Planned (#298) | `id` | `created_at` | `actor.login` / `milestone` | Generic fallback representative: `review_dismissed` | Metadata family |
| `deployed` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback representative: `review_dismissed` | |
| `deployment_environment_changed` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback representative: `review_dismissed` | |
| `disconnected` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback representative: `review_dismissed` | |
| `head_ref_deleted` | Planned (#297) | `id` | `created_at` | `actor.login` | Generic fallback representative: `review_dismissed` | Branch lifecycle |
| `head_ref_force_pushed` | Planned (#297) | `id` | `created_at` | `actor.login` | Generic fallback representative: `review_dismissed` | Branch lifecycle |
| `head_ref_restored` | Planned (#297) | `id` | `created_at` | `actor.login` | Generic fallback representative: `review_dismissed` | Branch lifecycle |
| `labeled` | Supported | `id` | `created_at` | `actor.login` / `label.name` | `testTimelineLabeledHtml` | |
| `locked` | Planned (#298) | `id` | `created_at` | `actor.login` / `lock_reason` | Generic fallback representative: `review_dismissed` | PR metadata lifecycle |
| `marked_as_duplicate` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback representative: `review_dismissed` | |
| `mentioned` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback representative: `review_dismissed` | |
| `merged` | Supported | `id` | `created_at` | `actor.login` / `commit_id` | `testPullRequestTimelineSafeFallbackAndDeduplication` | |
| `milestoned` | Planned (#298) | `id` | `created_at` | `actor.login` / `milestone` | Generic fallback representative: `review_dismissed` | Metadata family |
| `moved_columns_in_project` | Planned (#298) | `id` | `created_at` | `actor.login` / `project_card` | Generic fallback representative: `review_dismissed` | Project family |
| `pinned` | Not applicable | N/A | N/A | N/A | None | Issue-only event (per github docs) |
| `ready_for_review` | Planned (#297) | `id` | `created_at` | `actor.login` | Generic fallback representative: `review_dismissed` | PR lifecycle |
| `referenced` | Planned (#298) | `commit_id` | `created_at` | `actor.login` / `commit_url` | Generic fallback representative: `review_dismissed` | Reference family |
| `removed_from_project` | Planned (#298) | `id` | `created_at` | `actor.login` / `project_card` | Generic fallback representative: `review_dismissed` | Project family |
| `renamed` | Planned (#298) | `id` | `created_at` | `actor.login` / `rename` | Generic fallback representative: `review_dismissed` | Metadata family |
| `reopened` | Supported | `id` | `created_at` | `actor.login` | `testPullRequestTimelineSafeFallbackAndDeduplication` | |
| `review_dismissed` | Planned (#297) | `id` | `created_at` | `actor.login` / `dismissed_review` | `testPullRequestTimelineSafeFallbackAndDeduplication` | Has nested fields |
| `review_request_removed` | Planned (#297) | `id` | `created_at` | `actor.login` / `requested_reviewer` | Generic fallback representative: `review_dismissed` | Review family |
| `review_requested` | Planned (#297) | `id` | `created_at` | `actor.login` / `requested_reviewer` | Generic fallback representative: `review_dismissed` | Review family |
| `reviewed` | Planned (#297) | `id` | `submitted_at` | `user.login` / `body`, `state` | `testPullRequestTimelineSourceAwareIdentity` | Timestamp is `submitted_at`, actor is `user` |
| `subscribed` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback representative: `review_dismissed` | |
| `transferred` | Not applicable | N/A | N/A | N/A | None | Issue-only event (per github docs) |
| `unassigned` | Supported | `id` | `created_at` | `actor.login` / `assignee` | `testPullRequestTimelineSafeFallbackAndDeduplication` | |
| `unlabeled` | Supported | `id` | `created_at` | `actor.login` / `label.name` | `testPullRequestTimelineSafeFallbackAndDeduplication` | |
| `unlocked` | Planned (#298) | `id` | `created_at` | `actor.login` / `lock_reason` | Generic fallback representative: `review_dismissed` | PR metadata lifecycle |
| `unmarked_as_duplicate` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback representative: `review_dismissed` | |
| `unpinned` | Not applicable | N/A | N/A | N/A | None | Issue-only event (per github docs) |
| `unsubscribed` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback representative: `review_dismissed` | |
| `user_blocked` | Safe Fallback | `id` | `created_at` | `actor.login` | Generic fallback representative: `review_dismissed` | |
