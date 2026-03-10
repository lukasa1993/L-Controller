const os = require('node:os');
const { URL } = require('node:url');

const DEFAULT_PANEL_PORT = 80;
const DEFAULT_DISCOVERY_TIMEOUT_MS = 1_200;
const DEFAULT_DISCOVERY_CONCURRENCY = 32;
const PANEL_MARKERS = [
	'LNH Nordic Mission Console',
	'data-panel-page="dashboard"',
	'id="dashboard-view"',
	'src="/main.js"',
];

function normalizePort(rawValue) {
	const parsed = Number.parseInt(String(rawValue || DEFAULT_PANEL_PORT), 10);
	if (!Number.isInteger(parsed) || parsed < 1 || parsed > 65_535) {
		throw new Error(`Invalid panel port: ${rawValue}`);
	}

	return parsed;
}

function normalizeBaseUrl(rawValue, fallbackPort = DEFAULT_PANEL_PORT) {
	const trimmed = String(rawValue || '').trim();
	if (!trimmed) {
		throw new Error('Panel base URL is empty.');
	}

	const candidate = trimmed.includes('://')
		? trimmed
		: `http://${trimmed}`;
	const url = new URL(candidate);
	if (!url.protocol.startsWith('http')) {
		throw new Error(`Panel base URL must use http or https, received: ${trimmed}`);
	}
	if (!url.port) {
		url.port = String(fallbackPort);
	}
	url.pathname = '';
	url.search = '';
	url.hash = '';
	return url.toString().replace(/\/$/, '');
}

function isPrivateIpv4(address) {
	if (!address || !/^\d{1,3}(\.\d{1,3}){3}$/.test(address)) {
		return false;
	}

	const [first, second] = address.split('.').map((part) => Number.parseInt(part, 10));
	if (first === 10) {
		return true;
	}
	if (first === 172 && second >= 16 && second <= 31) {
		return true;
	}
	return first === 192 && second === 168;
}

function localPrivateIpv4Addresses() {
	const interfaces = os.networkInterfaces();
	const addresses = [];

	for (const entries of Object.values(interfaces)) {
		for (const entry of entries || []) {
			if (!entry || entry.internal || entry.family !== 'IPv4' || !isPrivateIpv4(entry.address)) {
				continue;
			}

			addresses.push(entry.address);
		}
	}

	return [...new Set(addresses)];
}

function detectSubnetPrefixes(env = process.env) {
	const explicitSubnet = String(env.LNH_PANEL_SUBNET || '').trim();
	if (explicitSubnet) {
		const match = explicitSubnet.match(/^(\d{1,3}\.\d{1,3}\.\d{1,3})(?:\.\d{1,3})?$/);
		if (!match) {
			throw new Error(`LNH_PANEL_SUBNET must look like 192.168.1 or 192.168.1.0, received: ${explicitSubnet}`);
		}
		return [match[1]];
	}

	const addresses = localPrivateIpv4Addresses();
	const prefixes = addresses.map((address) => address.split('.').slice(0, 3).join('.'));
	return [...new Set(prefixes)];
}

function buildDiscoveryCandidates(env = process.env) {
	const port = normalizePort(env.LNH_PANEL_PORT || DEFAULT_PANEL_PORT);
	const explicitHosts = String(env.LNH_PANEL_HOSTS || '')
		.split(',')
		.map((value) => value.trim())
		.filter(Boolean);

	if (explicitHosts.length > 0) {
		return explicitHosts.map((host) => normalizeBaseUrl(host, port));
	}

	const localAddresses = new Set(localPrivateIpv4Addresses());
	const candidates = [];
	for (const prefix of detectSubnetPrefixes(env)) {
		for (let host = 1; host <= 254; host += 1) {
			const address = `${prefix}.${host}`;
			if (localAddresses.has(address)) {
				continue;
			}

			candidates.push(normalizeBaseUrl(address, port));
		}
	}

	return candidates;
}

async function probePanelShell(baseUrl, options = {}) {
	const timeoutMs = Number.parseInt(
		String(options.timeoutMs || process.env.LNH_PANEL_DISCOVERY_TIMEOUT_MS || DEFAULT_DISCOVERY_TIMEOUT_MS),
		10,
	);
	const response = await fetch(`${baseUrl}/`, {
		method: 'GET',
		redirect: 'manual',
		signal: AbortSignal.timeout(timeoutMs),
	});
	if (!response.ok) {
		return false;
	}

	const contentType = response.headers.get('content-type') || '';
	if (!contentType.includes('text/html')) {
		return false;
	}

	const body = await response.text();
	return PANEL_MARKERS.every((marker) => body.includes(marker));
}

async function discoverPanelBaseUrl(env = process.env) {
	const candidates = buildDiscoveryCandidates(env);
	if (candidates.length === 0) {
		throw new Error(
			'Could not infer a private subnet for panel auto-discovery. Set LNH_PANEL_BASE_URL or LNH_PANEL_SUBNET.',
		);
	}

	const concurrency = Math.max(
		1,
		Math.min(
			Number.parseInt(String(env.LNH_PANEL_DISCOVERY_CONCURRENCY || DEFAULT_DISCOVERY_CONCURRENCY), 10) || DEFAULT_DISCOVERY_CONCURRENCY,
			candidates.length,
		),
	);

	let nextIndex = 0;
	let winner = null;

	async function worker() {
		while (winner === null) {
			const index = nextIndex;
			nextIndex += 1;
			if (index >= candidates.length) {
				return;
			}

			const candidate = candidates[index];
			try {
				if (await probePanelShell(candidate, { timeoutMs: env.LNH_PANEL_DISCOVERY_TIMEOUT_MS })) {
					winner = candidate;
					return;
				}
			} catch (error) {
				// Ignore timeouts and connection failures while scanning the subnet.
			}
		}
	}

	await Promise.all(Array.from({ length: concurrency }, () => worker()));
	if (winner !== null) {
		return winner;
	}

	throw new Error(
		`No panel shell was discovered on the local private subnet. Set LNH_PANEL_BASE_URL explicitly or narrow the scan with LNH_PANEL_SUBNET. Scanned ${candidates.length} candidates.`,
	);
}

async function resolvePanelBaseUrl(env = process.env) {
	const explicit = String(env.LNH_PANEL_BASE_URL || '').trim();
	if (explicit && explicit.toLowerCase() !== 'auto') {
		return normalizeBaseUrl(explicit, normalizePort(env.LNH_PANEL_PORT || DEFAULT_PANEL_PORT));
	}

	return discoverPanelBaseUrl(env);
}

function resolvePanelCredentials(baseUrl, env = process.env) {
	const username = String(env.LNH_PANEL_USERNAME || env.CONFIG_APP_ADMIN_USERNAME || '').trim();
	const password = String(env.LNH_PANEL_PASSWORD || env.CONFIG_APP_ADMIN_PASSWORD || '');
	if (username && password) {
		return { username, password };
	}

	throw new Error(
		`Missing panel credentials for ${baseUrl}. Set LNH_PANEL_USERNAME/LNH_PANEL_PASSWORD or CONFIG_APP_ADMIN_USERNAME/CONFIG_APP_ADMIN_PASSWORD before running the Playwright smoke.`,
	);
}

module.exports = {
	PANEL_MARKERS,
	buildDiscoveryCandidates,
	detectSubnetPrefixes,
	discoverPanelBaseUrl,
	normalizeBaseUrl,
	normalizePort,
	probePanelShell,
	resolvePanelBaseUrl,
	resolvePanelCredentials,
};

if (require.main === module) {
	resolvePanelBaseUrl()
		.then((baseUrl) => {
			process.stdout.write(`${baseUrl}\n`);
		})
		.catch((error) => {
			process.stderr.write(`${error.message}\n`);
			process.exitCode = 1;
		});
}
