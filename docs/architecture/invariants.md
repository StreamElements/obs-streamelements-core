# Enforced invariants

`tests/test_source_invariants.cpp` opens the production sources and asserts
that a fixed pattern is absent — or that a required one is present. **74
checks** run from its `main()`. They exist because this plug-in's worst bugs
are not type errors: they are a lock held one line too long, a strong pointer
where a weak one belonged, an `#ifdef` that silently stopped meaning anything.
A compiler cannot see any of those. A scanner over the source can.

## How to read and write one

Each check uses the file's own helpers — `slurp` to read a production file by
path, `strip_line_comments` so prose about a bug is not mistaken for the bug,
`function_body` to isolate one function, `scope_from` to walk a lexical scope,
and `check(condition, message)` to record a failure.

Three properties separate a check that earns its place from one that will be
deleted at the first inconvenience:

1. **It must be provably failable.** Reinstate the bug, run the suite, watch it
   fail. Every check here has been through that, and two of them turned out
   not to fire at all until mutation said so.
2. **It must be non-vacuous.** A scan whose pattern stops matching reports
   clean. So each carries a floor — "this must still find at least N sites" —
   and where a global floor would let one file's sites hide behind another's,
   the floor is **per file**.
3. **It must not cry wolf.** A check that flags correct code gets deleted. One
   here escapes legitimate cases by *shape* (a validator named `Is*`/
   `Validate*`/`Check*` one call deeper) rather than by "any preceding call",
   because the broader escape would have hidden a real defect.

Most run on comment-stripped and often whitespace-flattened source, so a
pattern cannot be defeated by wrapping a call across four lines — or by
clang-format joining it onto one.

## What they protect, grouped

### The root, the services and their lifetimes (CORE-757)

| check | rule |
| --- | --- |
| `check_the_root_is_not_a_singleton` | no `SEGlobalStateManager::GetInstance()`. Was a ratchet: 291 → 47 → … → 0, now absolute |
| `check_no_singletons_remain` | and not just the root — `SEConfig`, `SEPleaseWaitWindow`, `SENativeObsUIControlsManager` and a file-local source-data map were singletons too |
| `check_nothing_but_the_manager_owns_the_services_container` | exactly one strong `shared_ptr<SEApiServices>` member in the tree. This is what makes the rings unconstructible |
| `check_api_services_is_a_container_and_nothing_else` | no methods, no accessors, no policy on the container |
| `check_api_services_container_is_not_emptied_at_teardown` | the inverse of an old invariant: if a ring comes back the fix is a weak edge, not a teardown step |
| `check_injected_services_are_not_also_fetched_globally` | a consumer handed a service must not also reach for it globally |
| `check_every_published_service_is_watched_at_teardown` | add a service, watch it, or fail |
| `check_the_services_are_released_at_the_end_of_shutdown` | the audit runs where the container is released |
| `check_deferred_deletes_hold_services_weakly` | objects Qt destroys at a time nobody here chooses hold services weakly |
| `check_the_invoke_handler_is_not_immortal` | the auxiliary action invoker is a service, not a file-static `shared_ptr` built on first use and never released |
| `check_state_persistence_borrows_the_managers_it_serializes` | raw borrows, because of reverse-order destruction |
| `check_widget_manager_does_not_own_state_persistence` | the other half of the same ordering |
| `check_contract_default_arguments_agree_with_their_overrides` | a default argument on an `ISE*` method must match its implementation's — the compiler does not check this and the call site silently gets whichever it sees |

### Lifecycle and teardown (CORE-786 and friends)

| check | rule |
| --- | --- |
| `check_obs_close_is_watched` | the OBS main window is watched for close, because the update thread queues one before `FINISHED_LOADING` |
| `check_event_pumps_are_counted` | every event pump goes through one choke point |
| `check_dock_deletion_is_gated` | docks are not deleted when UI teardown is unsafe |
| `check_only_the_destructor_starts_teardown` | `Shutdown()` has exactly one caller |
| `check_the_config_is_owned_and_closed_explicitly` | the ini is owned, and closed on purpose |
| `check_the_config_is_closed_before_the_queue_is_drained` | both orders look harmless and one of them loses the user's layout |
| `check_the_dispatch_gate_opens_before_the_first_browser_widget` | `ready` is set above the block that creates widgets, so a page can never find it false |

### The host API

| check | rule |
| --- | --- |
| `check_api_handler_takes_its_services_by_injection` | handler bodies read `apiServices->…`, not a global |
| `check_api_handler_locks_the_services_once_per_dispatch` | the dispatcher locks once and bails; `self->m_services` never appears in a body |
| `check_the_api_call_lock_stays_per_translation_unit` | one macro definition, two lock domains — unifying them deadlocks every modal dialog |
| `check_handler_bodies_type_check_before_they_read` | a type check before every read of a page value, in all four spellings |
| `check_services_type_check_what_the_handlers_hand_them` | and in the services the handlers hand those values to |
| `check_handler_include_closure_only_goes_down` | the handler's include closure reaches 0 obs and 0 Qt headers; pinned absolutely |
| `check_api_handler_obs_qt_tokens_only_go_down` | a per-family ratchet on obs/Qt tokens in the handler file, floor and ceiling at once |
| `check_every_stand_in_can_answer_the_handler_that_calls_it` | the test fakes and stubs can actually return what each caller reads — four could not, and all four looked reasonable |
| `check_c10_no_duplicate_handler_setcurrentprofile` | `setCurrentProfile` appears once; the registry is a `std::map`, so a duplicate silently replaces the first |
| `check_c4_no_self_assign_cefclientid` | self-assignment in `batchInvokeSeries` left a field holding uninitialised heap memory, then routed it back to clients as a callback id |

### The data model

| check | rule |
| --- | --- |
| `check_cefparsejson_results_are_guarded_everywhere` | every `CefParseJSON` result is null-checked before it is dereferenced |
| `check_c5_cefparsejson_null_guarded` | the original instance of that, in `CefListValue::SetValue` |

### Compositions, scenes and signals

| check | rule |
| --- | --- |
| `check_no_shared_mutex_is_acquired_recursively` | four getters took a `shared_lock` and then called a sibling that takes the same one |
| `check_no_transition_call_runs_under_the_current_scene_lock` | no libobs transition call inside an `m_currentSceneMutex` scope — the recursion the check above cannot see, because it leaves through a signal callback |
| `check_the_scene_manager_drains_outside_its_mutex` | the destructor does not hold its mutex through cleanup and wait |
| `check_scene_signal_handler_lifetime` | the signal context outlives its connections |
| `check_signal_handler_data_holds_its_owners_weakly` | and does not keep its owners alive |
| `check_signal_handler_data_holds_the_composition_per_path` | strongly when the scene manager created it, weakly when the composition did, and the difference is deliberate |
| `check_the_scene_manager_is_attached_where_it_is_built` | a constructor cannot hand out a weak handle to itself, so attachment is where construction is |

### Outputs and encoders

| check | rule |
| --- | --- |
| `check_encoder_released_once_per_object` | acquired once for the first consumer, so released once for the last — not once per consumer |
| `check_counted_references_are_given_back_to_the_counter` | a counted getter parked in an `OBS*AutoRelease` must be paired, or the leak signal lies |
| `check_c2_video_encoder_template_match` | `IsMatchingIndex` compares with `==`; the original assigned with `=` and returned the truthiness of the assignment |
| `check_c6_audio_encoder_bounds` | `>=`, not `>`: valid indices are `[0, MAX_AUDIO_MIXES)` |

### UI and widgets

| check | rule |
| --- | --- |
| `check_widget_maps_hold_qpointer` | Qt owns the docks and widgets and destroys them when it likes |
| `check_widget_manager_dtor_does_not_drain_events` | the destructor does not pump |
| `check_widget_teardown_leaves_the_paint_tree` | a dock leaves the paint tree before destruction — Qt aborts in `_purecall` if a virtual call reaches a half-destroyed widget |
| `check_no_dock_is_added_to_an_invalid_area` | `Qt::NoDockWidgetArea` is not a valid `addDockWidget()` argument |
| `check_menu_manager_update_guarded` | `Update()` is not called on a null manager during construction |
| `check_non_modal_dialogs_are_closed_by_their_owner` | the dialog manager's destructor both rejects and satisfies every parked promise |

### Crash reporting (CORE-860 … CORE-968)

| check | rule |
| --- | --- |
| `check_both_sentry_doors_are_owned` | `sentry_init()` installs an SEH filter **and** a `SIGABRT` handler; owning only the first sent every `abort()` somewhere else |
| `check_abort_path_drops_own_frames` | the SIGABRT handler captures its own context, so our frames are on every abort in the process |
| `check_wer_skip_is_conditional_on_the_handler_flag` | …and dropping them must stay conditional, because when the fault is inside our handler they are there legitimately |
| `check_sentry_init_is_diagnosable` | a failing `sentry_init()` says why, and does not depend on the working directory |
| `check_sentry_wer_is_not_beside_the_host_executable` | sentry's WER module must never sit next to `obs64.exe`, or it claims every fast-fail in the process |
| `check_wer_module_gates_before_forwarding` | ours gates first: it runs in `WerFault.exe` with no way to ask anything afterwards |
| `check_wer_registration_prefix_is_asserted` | our registration block's prefix is sentry's, byte for byte, because sentry does a fixed-size read of it |
| `check_prompt_discloses_automatic_reports` | heap corruption and fast-fail are reported without a prior answer, so the prompt must say so |
| `check_consent_prompt_is_bounded` | the prompt cannot run unbounded on a crash path |
| `check_consent_prompt_does_not_pump_foreign_messages` | a Win32 modal loop dispatches every message queued for the thread, which on a crash path is fatal |
| `check_queued_tasks_suppressed_during_crash` | our queued Qt work does not run once reporting has begun |
| `check_oom_handler_observes_and_chains` | the allocation door is owned, chained, and not seized |
| `check_guard_buffer_released_first` | the reserved headroom is released before anything allocates |
| `check_crash_path_filesystem_calls_never_throw` | filesystem calls on the crash path do not throw |

### Config, HTTP and data scans

| check | rule |
| --- | --- |
| `check_config_never_hands_libobs_a_null_config` | `config_open()`'s result is checked; libobs leaves the handle null on failure |
| `check_no_config_handle_escapes_an_early_return` | no `config_t` leaks through a `return` between open and close |
| `check_rejected_signature_stops_the_handler` | the file server's rejection branch returns rather than falling through to serve the path |
| `check_data_driven_scans_do_not_call_os_file_exists` | `os_file_exists("")` leaks one libobs allocation per call — an upstream bug that cost 37 per backup scan |
| `check_the_trace_log_level_memo_is_computed_once` | the memo is a once-guaranteed static, not a hand-rolled flag read from every logging thread |

### Razer WYVRN

| check | rule |
| --- | --- |
| `check_wyvrn_signature_check_is_not_disabled` | the SDK is never built with its Authenticode verification skipped |
| `check_wyvrn_sdk_calls_stay_on_the_sdk_thread` | the Chroma stack beneath it is COM-based and thread-affine |

### Portability and hygiene

| check | rule |
| --- | --- |
| `check_every_source_file_ends_with_a_newline` | clang promotes `-Wnewline-eof` to an error under `-Werror`; one missing byte failed a macOS build out of 95 translation units |
| `check_no_platform_guard_uses_the_bare_APPLE_macro` | `APPLE` is a CMake variable, not a compiler define — three `#ifdef APPLE` branches had never been compiled, and one made a host API method answer false on macOS while doing nothing |

### CI and release plumbing

| check | rule |
| --- | --- |
| `check_every_emitted_output_is_declared` | a composite action exposes only the outputs it names |
| `check_full_release_adopts_before_cancelling` | a full release adopts the issues of the releases it overtakes before any of them are cancelled |

## When a check fails

Read the message: each one states the rule and, usually, the incident. Then
decide which of two things is true — the change reintroduced the pattern, or
the rule has genuinely moved. If it is the second, the check is the place to
record the new rule, and the commit should say why the old one stopped
applying. **Do not loosen a check to make a build green**; a check that cannot
fail is worth nothing, and the only way to know which kind you have is to
mutate the source and watch.

## Related

- [Testing](testing.md) — the suite these live in, and mutation testing.
- Every other page here links to the checks that hold its rules.
