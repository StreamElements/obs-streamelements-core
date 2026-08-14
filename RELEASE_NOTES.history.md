### 26.8.14.840

- Fix: lazy video encoder creation on use to reduce GPU resource consumption
- Fix: cache encoder basic properties to reduce GPU resource consumption
- Fix: crash under certain conditions when scenes are changed
- Fix: use OBS private canvas objects in custom video compositions instead of OBS views directly
- Fix: adjust unknown encoder video formats to preferred NV12 video format
- Fix: crashes triggered by corrupted memory
- Infra: crash reporting backend switched to sentry.io

