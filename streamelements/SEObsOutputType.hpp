#pragma once

//
// Which of OBS's three output kinds an output is.
//
// Its own header because it is vocabulary rather than behaviour.
// SEOutputManager's entire surface is keyed on this enum and it needs
// nothing else from SEOutput.hpp -- which arrives with obs-frontend-api.h,
// obs.h and twenty-one external headers behind it, for one enumeration of
// three values.
//
// Part 2 of CORE-757 requires ISEOutputManager's declarations to name no
// type that is only reachable through libobs, so the enum has to sit
// somewhere a clean header can include. Nothing here does.
//
enum ObsOutputType { StreamingOutput, RecordingOutput, ReplayBufferOutput };
