/**
 * WYVRN effect preview — proof of concept.
 *
 * Self-contained: it loads `.chroma` and `.haps` from any URL and renders them.
 * Nothing here touches the SE.Live host API, so it runs in a plain browser tab.
 *
 * Every class exposes `async dispose(): Promise<void>`.
 */

export {
	Disposable,
	DisposableBag,
	type IDisposable,
	type Teardown,
} from "./Disposable.js";

export {
	CHROMA_DEVICES,
	DeviceType,
	findDevice,
	findDeviceByKey,
	deviceFromFileName,
	type ChromaDevice,
} from "./ChromaDevices.js";

export {
	ChromaEffect,
	ChromaParseError,
	type ChromaFrame,
	type ChromaLoadOptions,
} from "./ChromaEffect.js";

export {
	HapsEffect,
	HapsParseError,
	sampleEnvelope,
	type HapsEnvelope,
	type HapsKeyframe,
	type HapsLoadOptions,
	type HapsMelody,
	type HapsNote,
	type HapsTransient,
} from "./HapsEffect.js";

export { DEVICE_SVGS } from "./deviceSvgs.generated.js";
export { ChromaRenderer, type ChromaRendererOptions } from "./ChromaRenderer.js";
export { HapsRenderer, type HapsRendererOptions } from "./HapsRenderer.js";
export {
	SequenceClock,
	type SequenceClockOptions,
	type SequenceSample,
} from "./SequenceClock.js";

export {
	HapticBodyRenderer,
	type HapticBodyRendererOptions,
	type HapticSource,
	type HapticTargeting,
	type BodyRegion,
} from "./HapticBodyRenderer.js";
