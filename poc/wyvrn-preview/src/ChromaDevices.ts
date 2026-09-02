/**
 * Device geometry for `.chroma` playback.
 *
 * A `.chroma` file carries `deviceType` and `device`, but **not** the grid
 * dimensions - the reader has to supply them. These match the WYVRN Effects
 * Library's own `getDeviceMaxRow` / `getDeviceMaxCol` / `getDeviceMaxLeds`
 * exactly, and were independently confirmed against the assets on disk, where
 * each reproduces its file size byte-for-byte:
 *
 *     size === 10 + frameCount * (4 + ledCount * 4)
 *
 * `device` indexes within its type class, so the pair `(deviceType, device)` is
 * the key. Note the two keyboards: `device 0` is the standard 6x22 grid and
 * `device 3` the extended 8x24 one. Both appear in real assets under the same
 * `_Keyboard` filename, and only the header separates them.
 *
 * Artwork lives in `deviceSvgs.generated.ts` - the gallery's own SVG files.
 * `svgKey` selects one; both keyboards share a single drawing.
 */

export const DeviceType = {
	Linear: 0,
	Grid: 1,
} as const;

export interface ChromaDevice {
	/** Stable identifier, and the filename suffix the assets use. */
	readonly key: string;
	readonly label: string;
	readonly deviceType: number;
	readonly deviceId: number;
	/** Grid rows; 1 for a linear strip. */
	readonly rows: number;
	/** Grid columns; the LED count for a linear strip. */
	readonly columns: number;
	/** Colours per frame: rows * columns. */
	readonly ledCount: number;
	/** Key into DEVICE_SVGS. */
	readonly svgKey: string;
}

function device(
	key: string,
	label: string,
	deviceType: number,
	deviceId: number,
	rows: number,
	columns: number,
	svgKey: string,
): ChromaDevice {
	return {
		key,
		label,
		deviceType,
		deviceId,
		rows,
		columns,
		ledCount: rows * columns,
		svgKey,
	};
}

export const CHROMA_DEVICES: readonly ChromaDevice[] = [
	// Linear strips.
	device("ChromaLink", "Chroma Link", DeviceType.Linear, 0, 1, 5, "chromalink"),
	device("Headset", "Headset", DeviceType.Linear, 1, 1, 5, "headset"),
	device("Mousepad", "Mousepad", DeviceType.Linear, 2, 1, 15, "mousepad"),

	// Grids.
	device("Keyboard", "Keyboard", DeviceType.Grid, 0, 6, 22, "keyboard"),
	device("Keypad", "Keypad", DeviceType.Grid, 1, 4, 5, "keypad"),
	device("Mouse", "Mouse", DeviceType.Grid, 2, 9, 7, "mouse"),
	device(
		"KeyboardExtended",
		"Keyboard (extended)",
		DeviceType.Grid,
		3,
		8,
		24,
		"keyboard",
	),
];

/** Look up by the pair that actually identifies a device. */
export function findDevice(
	deviceType: number,
	deviceId: number,
): ChromaDevice | undefined {
	return CHROMA_DEVICES.find(
		(d) => d.deviceType === deviceType && d.deviceId === deviceId,
	);
}

/** Look up by the suffix used in asset filenames, e.g. `Aim_On_Keyboard.chroma`. */
export function findDeviceByKey(key: string): ChromaDevice | undefined {
	const lower = key.toLowerCase();
	return CHROMA_DEVICES.find((d) => d.key.toLowerCase() === lower);
}

/**
 * Recover the device from a filename, for choosing artwork before the bytes
 * arrive. Ambiguous for keyboards - both grids use `_Keyboard` - so the header
 * always wins when it is available.
 */
export function deviceFromFileName(fileName: string): ChromaDevice | undefined {
	const base = fileName.replace(/\.chroma$/i, "").toLowerCase();
	// Longest key first, so "ChromaLink" beats any shorter accidental match.
	const byLength = [...CHROMA_DEVICES].sort((a, b) => b.key.length - a.key.length);
	return byLength.find((d) => base.endsWith("_" + d.key.toLowerCase()));
}
