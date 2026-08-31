# Defer settings save

`window.host`

## `beginDeferSaveTransaction(ResultCallback<transactionHandle>)`

Begins deferring OBS settings save until all defer save transactions complete or time out. Transactions time out after 60 seconds.

**Available since API version 1.33**

## `completeDeferSaveTransaction(transactionHandle, ResultCallback<success>)`

Completes a previously started defer save transaction identified by *transactionHandle*. Once all transactions complete or time out, settings save deferral is released and OBS settings are saved.

**Available since API version 1.33**
