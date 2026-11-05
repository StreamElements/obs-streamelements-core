# Outputs and encoders

An output is the thing that takes a composition's video and audio and sends it
somewhere: an RTMP server, a file, a replay buffer. OBS has one of each. SE.Live
adds as many as the user configures, across all its compositions, and this is
where the plug-in spends most of its runtime.

## The output family

`SEOutputBase` is the base; six concrete classes sit on it, in two mirrored
sets of three:

| kind | OBS's own | the plug-in's |
| --- | --- | --- |
| streaming | `SEObsNativeStreamingOutput` | `SECustomStreamingOutput` |
| recording | `SEObsNativeRecordingOutput` | `SECustomRecordingOutput` |
| replay buffer | `SEObsNativeReplayBufferOutput` | `SECustomReplayBufferOutput` |

`SEOutputManager` (behind `ISEOutputManager`) owns the set — add, remove,
enable, disable, enumerate — and its whole surface is keyed on
**`ObsOutputType`**, an enum of three values in its own header
(`SEObsOutputType.hpp`). That header exists for a reason worth imitating: the
enum is *vocabulary rather than behaviour*, and keeping it out of `SEOutput.hpp`
means a clean interface can name it without dragging in `obs.h`,
`obs-frontend-api.h` and twenty-one external headers for an enumeration of
three values.

`SEOutputSettingsManager` (`ISEOutputSettingsManager`) is the settings side:
encoding settings, streaming settings, and the available encoder catalogues.
17 host API methods reach it.

## OBS's own single stream has exactly one owner

`SEHostStreamingManager` is the one class in the plug-in that calls
`obs_frontend_streaming_start()` / `_stop()`. `ISEHostStreamingManager` carries
what the two host API methods need — `SerializeStreamingStatus` and
`RequestStop` — and the concrete class additionally offers `IsStreaming()` and
`RequestStart()` to `SENativeObsUIControlsManager`.

That dependency direction is deliberate: **UI controls → streaming**, not the
reverse. A UI class asking a service for the host's streaming state is right;
a service reaching back into a UI class to start a stream is not.

Note that `requestStreamingStart` as a *host API method* still routes to
`SENativeObsUIControlsManager`, because starting a stream here **is** the
native-button handover: the plug-in clicks OBS's own Start Streaming button so
that OBS's confirmation and bandwidth-test paths run.

!!! warning "Two commented-out libobs calls are load-bearing"
    `OnStartStopStreamingButtonClicked` and `BeginStartStreaming` each carry a
    disabled `obs_frontend_streaming_{stop,start}()` beside a live
    `ClickNativeStartStopStreamingButton()`. Tidying those two lines into
    `RequestStop()`/`RequestStart()` would be a behaviour change disguised as
    a refactor — the click is what runs OBS's own dialogs.

`obs_frontend_streaming_active()` is still *read* in a dozen places across nine
files, including `SEOutputSettingsManager`, which also stops recording and the
replay buffer. Consolidating those reads behind the same owner is a known,
unstarted tidy-up.

## Lazy encoder providers, and counting them correctly

Several outputs can share one encoder, and an encoder is expensive, so
`SELazyOBSVideoEncoderProvider` creates it on first use and releases it when
the last consumer goes away. The interesting type is the small RAII handle:

```cpp
SELazyOBSEncoderReference(…)  { m_provider->AddConsumer(); }
~SELazyOBSEncoderReference()  { m_provider->RemoveConsumer(); }
```

`AddConsumer` / `RemoveConsumer` log every transition with the allocator, the
object pointer, the refcount and the call site, which makes the log the
primary diagnostic: **`AddConsumer` and `RemoveConsumer` counts must balance**
in a clean session. `test_lazy_encoder_refcount` covers the counting, and
`check_encoder_released_once_per_object` pins the release path.

Two details that have each cost time:

- A **negative** consumer count is the genuine error in that class and is
  logged as one. The ordinary last-consumer message is informational — it used
  to open with the text `"error: "` at `LOG_INFO`, which made every clean
  teardown read as a failure.
- `check_c2_video_encoder_template_match` and
  `test_video_encoder_template_demo` exist because the encoder template
  machinery is the kind of code where a mismatch compiles fine and produces
  the wrong encoder.

## Output references are counted, and the counter is annotated

`GetOutput()` and its overrides return an **owned** `obs_output_t *` reference,
annotated for `SETrace`. So:

```cpp
OBSOutputAutoRelease output = self->GetOutput();
SETRACE_DECREF(output.Get());          // the destructor's release is unannotated

if (!output)
        return;
```

Calling `GetOutput()` twice and discarding both results — once for a null test,
once as an argument — leaked two references per call. That was the shape of a
real defect in `SECustomReplayBufferOutput::handle_output_saved()`, found by
scanning for the pattern rather than by observation, because it never fired in
a measured run.

`check_counted_references_are_given_back_to_the_counter` now pins this across
the composition, scene and output sources. It accepts both spellings —
`SETRACE_AUTODECREF(...)` and a following `SETRACE_DECREF(x.Get())` — because
eight of the nine existing call sites use the second one. See
[Diagnostics](diagnostics.md) for what the counter does and does not mean.

## Output events

`check_every_emitted_output_is_declared` keeps the set of emitted output
events and the declared set in step. Output signals are libobs signals, so the
scene-signal discipline in [Compositions and scenes](compositions.md) applies
here too — `handle_output_*` callbacks are file statics receiving their object
as `void *data`.

## Related

- [Compositions and scenes](compositions.md) — what feeds an output.
- [Diagnostics](diagnostics.md) — the reference-count balance line.
- The API reference's
  [streaming outputs](https://streamelements.github.io/obs-streamelements-core/api/host/streaming-outputs/),
  [recording outputs](https://streamelements.github.io/obs-streamelements-core/api/host/recording-outputs/)
  and
  [replay buffer](https://streamelements.github.io/obs-streamelements-core/api/host/replay-buffer-outputs/)
  groups.
