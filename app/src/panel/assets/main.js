const NETWORK_CONNECTIVITY_LAN_UP_UPSTREAM_DEGRADED = 'lan-up-upstream-degraded';

const state = {
	authenticated: false,
	sessionUsername: '',
	status: null,
	refreshTimer: null,
	relayCommandPending: false,
	relayCommandDesiredState: false,
	relayFeedback: null,
};

const routes = {
	session: '/api/auth/session',
	login: '/api/auth/login',
	logout: '/api/auth/logout',
	status: '/api/status',
	relayDesiredState: '/api/relay/desired-state',
};

const connectivityLabels = {
	healthy: 'Healthy',
	connecting: 'Connecting',
	'not-ready': 'Not ready',
	'degraded-retrying': 'Recovering locally',
	[NETWORK_CONNECTIVITY_LAN_UP_UPSTREAM_DEGRADED]: 'LAN_UP_UPSTREAM_DEGRADED',
};

const elements = {
	alert: document.getElementById('panel-alert'),
	loginView: document.getElementById('login-view'),
	dashboardView: document.getElementById('dashboard-view'),
	loginForm: document.getElementById('login-form'),
	loginUsername: document.getElementById('login-username'),
	loginPassword: document.getElementById('login-password'),
	loginSubmit: document.getElementById('login-submit'),
	sessionChip: document.getElementById('session-chip'),
	networkPill: document.getElementById('network-pill'),
	refreshButton: document.getElementById('refresh-button'),
	logoutButton: document.getElementById('logout-button'),
	cards: {
		device: document.getElementById('device-card'),
		network: document.getElementById('network-card'),
		recovery: document.getElementById('recovery-card'),
		relay: document.getElementById('relay-card'),
		scheduler: document.getElementById('scheduler-card'),
		update: document.getElementById('update-card'),
	},
};

function escapeHtml(value) {
	return String(value)
		.replaceAll('&', '&amp;')
		.replaceAll('<', '&lt;')
		.replaceAll('>', '&gt;')
		.replaceAll('"', '&quot;')
		.replaceAll("'", '&#39;');
}

function boolLabel(value) {
	return value ? 'Yes' : 'No';
}

function relayStateLabel(value) {
	return value ? 'On' : 'Off';
}

function humanizeHyphenated(value) {
	return String(value || 'none')
		.split('-')
		.filter(Boolean)
		.map((part) => part.charAt(0).toUpperCase() + part.slice(1))
		.join(' ');
}

function relaySourceBadgeClass(source) {
	switch (source) {
	case 'manual-panel':
		return 'badge--ok';
	case 'recovery-policy':
	case 'safety-policy':
		return 'badge--warn';
	default:
		return '';
	}
}

function setRelayFeedback(message, tone = 'info') {
	state.relayFeedback = message ? { message, tone } : null;
}

function setAlert(message, tone = 'info') {
	if (!elements.alert) {
		return;
	}

	elements.alert.textContent = message;
	elements.alert.dataset.tone = tone;
}

function setBusy(button, busy, label) {
	if (!button) {
		return;
	}

	button.disabled = busy;
	if (label) {
		button.textContent = label;
	}
}

function showLoginView(message, tone = 'info') {
	state.authenticated = false;
	state.relayCommandPending = false;
	state.relayCommandDesiredState = false;
	setRelayFeedback(null);
	elements.loginView?.classList.remove('hidden');
	elements.dashboardView?.classList.add('hidden');
	elements.logoutButton?.setAttribute('disabled', 'disabled');
	elements.refreshButton?.setAttribute('disabled', 'disabled');
	elements.sessionChip.textContent = 'Authentication required';
	elements.sessionChip.className = 'badge badge--warn';
	if (message) {
		setAlert(message, tone);
	}
}

function showDashboardView() {
	state.authenticated = true;
	elements.loginView?.classList.add('hidden');
	elements.dashboardView?.classList.remove('hidden');
	elements.logoutButton?.removeAttribute('disabled');
	elements.refreshButton?.removeAttribute('disabled');
	elements.sessionChip.textContent = `Authenticated as ${state.sessionUsername || 'operator'}`;
	elements.sessionChip.className = 'badge badge--ok';
}

async function requestJson(url, options = {}) {
	const response = await fetch(url, {
		credentials: 'same-origin',
		headers: {
			'Content-Type': 'application/json',
			...(options.headers || {}),
		},
		...options,
	});

	let data = null;
	const contentType = response.headers.get('content-type') || '';
	if (contentType.includes('application/json')) {
		data = await response.json();
	}

	return { response, data };
}

function renderCard(title, eyebrow, rows, footer = '') {
	const metrics = rows
		.map(({ label, value }) => `
			<div class="metric-row">
				<div>
					<div class="metric-label">${escapeHtml(label)}</div>
				</div>
				<div class="metric-value">${escapeHtml(value)}</div>
			</div>
		`)
		.join('');

	return `
		<p class="card-eyebrow">${escapeHtml(eyebrow)}</p>
		<h3>${escapeHtml(title)}</h3>
		<div class="metric-list">${metrics}</div>
		${footer}
	`;
}

function updateNetworkChrome(network) {
	const connectivity = network?.connectivity || 'not-ready';
	const label = connectivityLabels[connectivity] || connectivity;
	const degraded = connectivity === NETWORK_CONNECTIVITY_LAN_UP_UPSTREAM_DEGRADED ||
		connectivity === 'degraded-retrying';

	elements.networkPill.textContent = label;
	elements.networkPill.className = `badge ${degraded ? 'badge--warn' : 'badge--ok'}`;
}

function relayFeedbackState(relay) {
	if (state.relayCommandPending) {
		return {
			tone: 'warn',
			message: `Pending: requesting relay ${state.relayCommandDesiredState ? 'on' : 'off'} from the device…`,
		};
	}

	if (state.relayFeedback) {
		return state.relayFeedback;
	}

	if (relay.blocked && relay.blockedReason && relay.blockedReason !== 'none') {
		return { tone: 'warn', message: relay.blockedReason };
	}

	return null;
}

function renderRelayCard(relay) {
	const actualState = relayStateLabel(relay.actualState);
	const desiredState = relayStateLabel(relay.desiredState);
	const pending = state.relayCommandPending || relay.pending;
	const blocked = relay.blocked;
	const available = relay.implemented && relay.available;
	const mismatch = relay.actualState !== relay.desiredState;
	const feedback = relayFeedbackState(relay);
	const sourceClass = relaySourceBadgeClass(relay.source);
	const buttonLabel = !available
		? 'Relay unavailable'
		: pending
			? 'Sending relay command…'
			: relay.actualState
				? 'Turn relay off'
				: 'Turn relay on';
	const actionCopy = !available
		? 'Relay control stays locked until the live runtime becomes available again.'
		: pending
			? 'The control stays locked until live status refresh confirms the result.'
			: 'One tap asks the local device to change the relay, then the panel refreshes from live status.';
	const badgeMarkup = `
		<div class="relay-badge-row">
			<span class="badge ${relay.actualState ? 'badge--ok' : ''}">Actual ${escapeHtml(actualState)}</span>
			<span class="badge ${sourceClass}">Source ${escapeHtml(humanizeHyphenated(relay.source))}</span>
			${blocked ? '<span class="badge badge--warn">Blocked</span>' : ''}
			${pending ? '<span class="badge badge--warn">Pending</span>' : ''}
		</div>
	`;
	const noteMarkup = [
		relay.safetyNote && relay.safetyNote !== 'none'
			? `<div class="relay-note relay-note--info">Safety note: ${escapeHtml(relay.safetyNote)}</div>`
			: '',
		mismatch
			? `<div class="relay-note relay-note--warn">Actual ${escapeHtml(actualState)} differs from remembered desired ${escapeHtml(desiredState)}.</div>`
			: '',
	].join('');
	const feedbackMarkup = feedback
		? `<div class="relay-feedback" data-tone="${escapeHtml(feedback.tone)}">${escapeHtml(feedback.message)}</div>`
		: '';

	elements.cards.relay.innerHTML = renderCard('Relay control', 'Phase 6 live surface', [
		{ label: 'Actual state', value: actualState },
		{ label: 'Remembered desired', value: desiredState },
		{ label: 'Control path', value: available ? 'Available' : 'Unavailable' },
		{ label: 'Reboot policy', value: humanizeHyphenated(relay.rebootPolicy) },
	], `${badgeMarkup}
		<div class="relay-action">
			<button class="button" type="button" data-relay-toggle ${pending || blocked || !available ? 'disabled' : ''}>${escapeHtml(buttonLabel)}</button>
			<p class="relay-action-copy muted">${escapeHtml(actionCopy)}</p>
		</div>
		${feedbackMarkup}
		${noteMarkup}`);

	elements.cards.relay.querySelector('[data-relay-toggle]')?.addEventListener('click', handleRelayToggle);
}

function renderStatus(statusPayload) {
	state.status = statusPayload;
	updateNetworkChrome(statusPayload.network);

	const networkLabel = connectivityLabels[statusPayload.network.connectivity] || statusPayload.network.connectivity;
	const degradedCopy = statusPayload.network.connectivity === NETWORK_CONNECTIVITY_LAN_UP_UPSTREAM_DEGRADED
		? '<div class="placeholder-copy">Local LAN access is healthy even though upstream internet reachability is degraded. The panel keeps rendering local status in this state.</div>'
		: '';

	elements.cards.device.innerHTML = renderCard('Device shell', 'Device', [
		{ label: 'Board', value: statusPayload.device.board },
		{ label: 'Panel port', value: statusPayload.device.panelPort },
		{ label: 'APP_READY path', value: boolLabel(statusPayload.device.ready) },
	]);

	elements.cards.network.innerHTML = renderCard('Connectivity', 'Network', [
		{ label: 'Connectivity', value: networkLabel },
		{ label: 'Wi-Fi ready', value: boolLabel(statusPayload.network.wifiReady) },
		{ label: 'Wi-Fi connected', value: boolLabel(statusPayload.network.wifiConnected) },
		{ label: 'IPv4 bound', value: boolLabel(statusPayload.network.ipv4Bound) },
		{ label: 'IPv4 address', value: statusPayload.network.ipv4Address || 'Pending' },
		{ label: 'Reachability', value: boolLabel(statusPayload.network.reachabilityOk) },
		{ label: 'Last failure stage', value: statusPayload.network.lastFailure.stage },
	]);
	elements.cards.network.innerHTML += degradedCopy;

	elements.cards.recovery.innerHTML = renderCard('Recovery posture', 'Recovery', [
		{ label: 'Reset cause available', value: boolLabel(statusPayload.recovery.available) },
		{ label: 'Recovery reset', value: boolLabel(statusPayload.recovery.recoveryReset) },
		{ label: 'Trigger', value: statusPayload.recovery.trigger },
		{ label: 'Failure stage', value: statusPayload.recovery.failureStage },
		{ label: 'Connectivity snapshot', value: statusPayload.recovery.connectivity },
		{ label: 'Reason', value: statusPayload.recovery.reason },
	]);

	renderRelayCard(statusPayload.relay);

	elements.cards.scheduler.innerHTML = renderCard('Scheduler surface', 'Phase 7 placeholder', [
		{ label: 'Implemented', value: boolLabel(statusPayload.scheduler.implemented) },
		{ label: 'Saved schedules', value: statusPayload.scheduler.scheduleCount },
		{ label: 'Enabled schedules', value: statusPayload.scheduler.enabledCount },
	], `<div class="placeholder-copy">${escapeHtml(statusPayload.scheduler.placeholder)}</div>`);

	elements.cards.update.innerHTML = renderCard('Update surface', 'Phase 8 placeholder', [
		{ label: 'Implemented', value: boolLabel(statusPayload.update.implemented) },
		{ label: 'Contract', value: 'Read-only placeholder only' },
	], `<div class="placeholder-copy">${escapeHtml(statusPayload.update.placeholder)}</div>`);
}

async function refreshStatus({ silent = false } = {}) {
	try {
		const { response, data } = await requestJson(routes.status, { method: 'GET' });
		if (response.status === 401) {
			showLoginView('The device no longer trusts this browser session. Log in again.', 'warn');
			return false;
		}

		if (!response.ok || !data) {
			throw new Error(`Status refresh failed (${response.status})`);
		}

		showDashboardView();
		renderStatus(data);
		setAlert('Protected status refreshed from the device.', silent ? 'info' : 'success');
		return true;
	} catch (error) {
		setAlert(error instanceof Error ? error.message : 'Status refresh failed.', 'error');
		return false;
	}
}

async function handleRelayToggle() {
	const relay = state.status?.relay;
	if (!relay || state.relayCommandPending || relay.blocked || !relay.implemented || !relay.available) {
		return;
	}

	const desiredState = !relay.actualState;
	state.relayCommandPending = true;
	state.relayCommandDesiredState = desiredState;
	setRelayFeedback(`Pending: requesting relay ${desiredState ? 'on' : 'off'} from the device…`, 'warn');
	renderRelayCard(relay);

	try {
		const { response, data } = await requestJson(routes.relayDesiredState, {
			method: 'POST',
			body: JSON.stringify({ desiredState }),
		});

		if (response.status === 401) {
			showLoginView('The device no longer trusts this browser session. Log in again.', 'warn');
			return;
		}

		if (!response.ok || !data?.accepted) {
			const detail = data?.detail || data?.error || `Relay command failed (${response.status})`;
			throw new Error(`Relay command failed: ${detail}`);
		}

		const refreshed = await refreshStatus({ silent: true });
		if (!refreshed) {
			setRelayFeedback('Relay command was accepted, but live status refresh did not complete yet.', 'warn');
			return;
		}

		setRelayFeedback(null);
		setAlert(`Relay ${desiredState ? 'on' : 'off'} request applied.`, 'success');
	} catch (error) {
		const message = error instanceof Error ? error.message : 'Relay command failed.';
		setRelayFeedback(message, 'error');
		setAlert(message, 'error');
	} finally {
		state.relayCommandPending = false;
		state.relayCommandDesiredState = false;
		if (state.status?.relay) {
			renderRelayCard(state.status.relay);
		}
	}
}

async function bootstrapSession() {
	setAlert('Checking whether this browser already has a valid local session…', 'info');
	try {
		const { response, data } = await requestJson(routes.session, { method: 'GET' });
		if (response.ok && data?.authenticated) {
			state.sessionUsername = data.username || 'operator';
			await refreshStatus({ silent: true });
			return;
		}
		showLoginView('Log in with the configured local admin credentials to unlock protected status.', 'info');
	} catch (error) {
		showLoginView('The panel could not confirm session state yet. Try logging in once the device is reachable.', 'warn');
	}
}

async function handleLogin(event) {
	event.preventDefault();
	const username = elements.loginUsername?.value?.trim() || '';
	const password = elements.loginPassword?.value || '';

	if (!username || !password) {
		setAlert('Enter both the username and password before continuing.', 'warn');
		return;
	}

	setBusy(elements.loginSubmit, true, 'Authenticating…');
	setAlert('Sending credentials to the local device…', 'info');

	try {
		const { response, data } = await requestJson(routes.login, {
			method: 'POST',
			body: JSON.stringify({ username, password }),
		});

		if (response.ok && data?.authenticated) {
			state.sessionUsername = username;
			elements.loginForm?.reset();
			await refreshStatus({ silent: false });
			return;
		}

		if (response.status === 429) {
			const retryAfterMs = data?.retryAfterMs || 0;
			setAlert(`Too many wrong-password attempts. Wait ${Math.ceil(retryAfterMs / 1000)} seconds and try again.`, 'warn');
			return;
		}

		setAlert('Authentication failed. Confirm the local admin credentials and try again.', 'error');
	} catch (error) {
		setAlert(error instanceof Error ? error.message : 'Authentication failed.', 'error');
	} finally {
		setBusy(elements.loginSubmit, false, 'Authenticate locally');
	}
}

async function handleLogout() {
	setBusy(elements.logoutButton, true, 'Logging out…');
	try {
		await fetch(routes.logout, {
			method: 'POST',
			credentials: 'same-origin',
		});
		state.sessionUsername = '';
		state.relayCommandPending = false;
		state.relayCommandDesiredState = false;
		setRelayFeedback(null);
		showLoginView('Session cleared. Log in again to view protected status.', 'success');
	} catch (error) {
		setAlert('Logout request failed, but the browser session may already be invalid.', 'warn');
		showLoginView('Session reset locally. Log in again if needed.', 'warn');
	} finally {
		setBusy(elements.logoutButton, false, 'Logout');
	}
}

function attachEvents() {
	elements.loginForm?.addEventListener('submit', handleLogin);
	elements.refreshButton?.addEventListener('click', () => refreshStatus({ silent: false }));
	elements.logoutButton?.addEventListener('click', handleLogout);
}

function startPolling() {
	if (state.refreshTimer) {
		clearInterval(state.refreshTimer);
	}

	state.refreshTimer = window.setInterval(() => {
		if (state.authenticated) {
			refreshStatus({ silent: true });
		}
	}, 15000);
}

attachEvents();
startPolling();
bootstrapSession();
