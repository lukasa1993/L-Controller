const NETWORK_CONNECTIVITY_LAN_UP_UPSTREAM_DEGRADED = 'lan-up-upstream-degraded';

function createSchedulerFormState(overrides = {}) {
	return {
		mode: 'create',
		scheduleId: '',
		minute: '0',
		hour: '*',
		dayOfMonth: '*',
		month: '*',
		dayOfWeek: '*',
		actionKey: 'relay-on',
		enabled: true,
		...overrides,
	};
}

function createActionFormState(overrides = {}) {
	return {
		mode: 'create',
		actionId: '',
		displayName: '',
		outputKey: '',
		commandKey: 'relay-on',
		enabled: true,
		...overrides,
	};
}

const state = {
	authenticated: false,
	sessionUsername: '',
	status: null,
	actionsSnapshot: null,
	scheduleSnapshot: null,
	updateSnapshot: null,
	refreshTimer: null,
	refreshPromise: null,
	refreshNeedsAnnouncement: false,
	actionsBusy: false,
	actionFeedback: null,
	relayCommandPending: false,
	relayCommandDesiredState: false,
	relayFeedback: null,
	schedulerBusy: false,
	schedulerFeedback: null,
	updateBusy: false,
	updateFeedback: null,
	actionForm: createActionFormState(),
	schedulerForm: createSchedulerFormState(),
	actionEditorSeed: '',
	scheduleEditorSeed: '',
	pageAbortController: new AbortController(),
};

const routes = {
	session: '/api/auth/session',
	login: '/api/auth/login',
	logout: '/api/auth/logout',
	status: '/api/status',
	actions: '/api/actions',
	actionCreate: '/api/actions/create',
	actionUpdate: '/api/actions/update',
	updateStatus: '/api/update',
	updateUpload: '/api/update/upload',
	updateNow: '/api/update/remote-check',
	updateApply: '/api/update/apply',
	updateClear: '/api/update/clear',
	relayDesiredState: '/api/relay/desired-state',
	schedules: '/api/schedules',
	scheduleCreate: '/api/schedules/create',
	scheduleUpdate: '/api/schedules/update',
	scheduleDelete: '/api/schedules/delete',
	scheduleSetEnabled: '/api/schedules/set-enabled',
};

const RETRYABLE_PANEL_GET_ROUTES = new Set([
	routes.session,
	routes.status,
	routes.actions,
	routes.updateStatus,
	routes.schedules,
]);
const RETRYABLE_PANEL_STATUSES = new Set([409, 503]);
const RETRYABLE_PANEL_ATTEMPTS = 3;
const RETRYABLE_PANEL_BACKOFF_MS = 150;

const pages = {
	login: 'login',
	overview: 'overview',
	actions: 'actions',
	actionEditor: 'action-editor',
	schedules: 'schedules',
	scheduleEditor: 'schedule-editor',
	updates: 'updates',
};

const navSections = {
	overview: 'overview',
	actions: 'actions',
	schedules: 'schedules',
	updates: 'updates',
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
	loginForm: document.getElementById('login-form'),
	loginUsername: document.getElementById('login-username'),
	loginPassword: document.getElementById('login-password'),
	loginSubmit: document.getElementById('login-submit'),
	sessionChip: document.getElementById('session-chip'),
	networkPill: document.getElementById('network-pill'),
	panelSummary: document.getElementById('panel-summary'),
	refreshButton: document.getElementById('refresh-button'),
	logoutButton: document.getElementById('logout-button'),
	navLinks: Array.from(document.querySelectorAll('[data-panel-nav]')),
	pageCtas: Array.from(document.querySelectorAll('[data-page-cta]')),
	cards: {
		device: document.getElementById('device-card'),
		network: document.getElementById('network-card'),
		recovery: document.getElementById('recovery-card'),
		relay: document.getElementById('relay-card'),
		actionsInfo: document.getElementById('actions-info-card'),
		scheduler: document.getElementById('scheduler-card'),
		update: document.getElementById('update-card'),
	},
};

const currentPage = document.body?.dataset.panelPage || pages.actions;
const flashStorageKey = 'lnh-panel-flash';
const SESSION_EXPIRED_MESSAGE = 'Session expired. Sign in again.';

const ui = {
	badgeBase: 'inline-flex items-center gap-2 rounded-full border border-slate-200 bg-white px-3 py-1.5 text-[0.72rem] font-semibold uppercase tracking-[0.16em] text-slate-600',
	badgeOk: 'border-emerald-200 bg-emerald-50 text-emerald-700',
	badgeWarn: 'border-amber-200 bg-amber-50 text-amber-700',
	badgeDanger: 'border-rose-200 bg-rose-50 text-rose-700',
	buttonBase: 'inline-flex w-full items-center justify-center rounded-xl bg-indigo-600 px-4 py-2.5 text-sm font-semibold text-white shadow-sm transition hover:bg-indigo-500 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-indigo-600 disabled:cursor-wait disabled:opacity-60',
	buttonGhost: 'w-auto border border-slate-200 bg-white text-slate-700 hover:bg-slate-50',
	buttonSecondary: 'border border-slate-200 bg-white text-slate-700 hover:bg-slate-50',
	buttonActive: 'bg-indigo-50 text-indigo-700 ring-1 ring-indigo-200',
	inputBase: 'block w-full rounded-xl border border-slate-300 bg-white px-3.5 py-2.5 text-sm text-slate-900 shadow-sm outline-none placeholder:text-slate-400 focus:border-indigo-500 focus:ring-2 focus:ring-indigo-200',
	alertBase: 'rounded-2xl border border-slate-200 bg-white px-4 py-3 text-sm leading-relaxed text-slate-700 shadow-sm',
	alertWarn: 'border-amber-200 bg-amber-50 text-amber-800',
	alertError: 'border-rose-200 bg-rose-50 text-rose-800',
	alertSuccess: 'border-emerald-200 bg-emerald-50 text-emerald-800',
	eyebrow: 'mb-2 text-xs font-semibold uppercase tracking-[0.18em] text-indigo-600',
	title: 'm-0 text-lg font-semibold tracking-tight text-slate-900',
	muted: 'text-sm leading-6 text-slate-600',
	metricList: 'mt-4 grid gap-3',
	metricRow: 'grid grid-cols-[minmax(0,1fr)_auto] gap-4 border-t border-slate-200 pt-3 first:border-t-0 first:pt-0',
	metricLabel: 'text-xs font-medium uppercase tracking-[0.12em] text-slate-500',
	metricValue: 'text-right text-sm font-semibold text-slate-900',
	placeholder: 'mt-4 rounded-2xl border border-dashed border-slate-300 bg-slate-50 px-4 py-4 text-sm leading-relaxed text-slate-600',
	noticeBase: 'rounded-2xl border border-slate-200 bg-slate-50 px-4 py-4 text-sm leading-relaxed text-slate-700',
	insetPanel: 'rounded-2xl border border-slate-200 bg-slate-50 p-5',
	rowFlex: 'flex flex-wrap items-center gap-2.5',
	gridGap3: 'grid gap-4',
	gridGap4: 'grid gap-6',
	summaryGrid: 'mt-4 grid gap-3 md:grid-cols-4',
	schedulerHeader: 'mt-6 grid gap-4 xl:grid-cols-[minmax(0,1.2fr)_minmax(0,0.8fr)]',
	schedulerLayout: 'mt-6 grid gap-6 xl:grid-cols-[minmax(0,1.1fr)_minmax(320px,0.9fr)]',
	updateLayout: 'mt-6 grid gap-6 xl:grid-cols-[minmax(0,1.15fr)_minmax(320px,0.85fr)]',
	uploadLayout: 'grid gap-3 md:grid-cols-[minmax(0,1fr)_auto] md:items-end',
	schedulerFields: 'grid gap-3 md:grid-cols-2 xl:grid-cols-5',
	schedulerRow: 'grid gap-3 rounded-2xl border border-slate-200 bg-white p-4 md:grid-cols-[minmax(0,1fr)_auto] md:items-center',
	navLinkBase: 'inline-flex items-center justify-between rounded-xl px-3 py-2 text-sm font-medium transition',
	navLinkActive: 'bg-indigo-50 text-indigo-700 ring-1 ring-indigo-200',
	navLinkInactive: 'text-slate-600 hover:bg-slate-100 hover:text-slate-900',
	summaryTile: 'rounded-3xl border border-slate-200 bg-white p-5 shadow-sm',
	actionButtons: 'grid gap-3 sm:grid-cols-2',
	code: 'rounded-md bg-slate-100 px-1.5 py-0.5 text-sm text-slate-700',
	list: 'm-0 list-disc pl-[18px] text-sm leading-6 text-slate-600',
};

function classNames(...values) {
	return values.filter(Boolean).join(' ');
}

function badgeToneClass(tone) {
	switch (tone) {
	case 'ok':
		return ui.badgeOk;
	case 'warn':
		return ui.badgeWarn;
	case 'danger':
		return ui.badgeDanger;
	default:
		return '';
	}
}

function badgeClass(tone = '') {
	return classNames(ui.badgeBase, badgeToneClass(tone));
}

function alertClass(tone = 'info') {
	switch (tone) {
	case 'warn':
		return classNames(ui.alertBase, ui.alertWarn);
	case 'error':
		return classNames(ui.alertBase, ui.alertError);
	case 'success':
		return classNames(ui.alertBase, ui.alertSuccess);
	default:
		return ui.alertBase;
	}
}

function noticeClass(tone = 'info', dashed = false) {
	const base = dashed ? ui.placeholder : ui.noticeBase;
	switch (tone) {
	case 'warn':
		return classNames(base, ui.alertWarn);
	case 'error':
		return classNames(base, ui.alertError);
	case 'success':
		return classNames(base, ui.alertSuccess);
	default:
		return base;
	}
}

function buttonClass({ ghost = false, full = true } = {}) {
	return classNames(ui.buttonBase, !full && 'w-auto', ghost && ui.buttonGhost);
}

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

function formatUtcMinute(normalizedUtcMinute) {
	const date = new Date(Number(normalizedUtcMinute) * 60000);
	if (!Number.isFinite(Number(normalizedUtcMinute)) || Number(normalizedUtcMinute) < 0 || Number.isNaN(date.getTime())) {
		return 'Pending';
	}

	return `${date.toISOString().slice(0, 16).replace('T', ' ')} UTC`;
}

function schedulerClockTone(clockState) {
	switch (clockState) {
	case 'trusted':
		return 'ok';
	case 'degraded':
		return 'warn';
	default:
		return 'danger';
	}
}

function relaySourceBadgeClass(source) {
	switch (source) {
	case 'manual-panel':
		return 'ok';
	case 'recovery-policy':
	case 'safety-policy':
		return 'warn';
	default:
		return '';
	}
}

function setRelayFeedback(message, tone = 'info') {
	state.relayFeedback = message ? { message, tone } : null;
}

function setActionFeedback(message, tone = 'info') {
	state.actionFeedback = message ? { message, tone } : null;
}

function setSchedulerFeedback(message, tone = 'info') {
	state.schedulerFeedback = message ? { message, tone } : null;
}

function setUpdateFeedback(message, tone = 'info') {
	state.updateFeedback = message ? { message, tone } : null;
}

function setAlert(message, tone = 'info') {
	if (!elements.alert) {
		return;
	}

	elements.alert.hidden = false;
	elements.alert.textContent = message;
	elements.alert.dataset.tone = tone;
	elements.alert.className = alertClass(tone);
}

function clearAlert() {
	if (!elements.alert) {
		return;
	}

	elements.alert.hidden = true;
	elements.alert.textContent = '';
	delete elements.alert.dataset.tone;
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

function resetSchedulerForm(overrides = {}) {
	state.schedulerForm = createSchedulerFormState(overrides);
}

function resetActionForm(overrides = {}) {
	state.actionForm = createActionFormState(overrides);
}

function isLoginPage() {
	return currentPage === pages.login;
}

function stashFlashMessage(message, tone = 'info') {
	if (!message) {
		return;
	}

	try {
		window.sessionStorage.setItem(flashStorageKey, JSON.stringify({ message, tone }));
	} catch (error) {
		// Ignore storage failures in constrained browsers.
	}
}

function consumeFlashMessage() {
	try {
		const rawValue = window.sessionStorage.getItem(flashStorageKey);
		if (!rawValue) {
			return null;
		}

		window.sessionStorage.removeItem(flashStorageKey);
		const parsed = JSON.parse(rawValue);
		if (!parsed || typeof parsed.message !== 'string') {
			return null;
		}

		return {
			message: parsed.message,
			tone: parsed.tone || 'info',
		};
	} catch (error) {
		return null;
	}
}

function normalizeNextPath(rawValue) {
	const candidate = String(rawValue || '').trim();
	if (!candidate.startsWith('/') || candidate.startsWith('//')) {
		return '/';
	}

	try {
		const parsed = new URL(candidate, window.location.origin);
		const normalized = `${parsed.pathname}${parsed.search}${parsed.hash}`;
		if (parsed.origin !== window.location.origin || normalized.startsWith('/api/') ||
			normalized === '/login' || normalized.startsWith('/login?')) {
			return '/';
		}

		return normalized || '/';
	} catch (error) {
		return '/';
	}
}

function currentPath() {
	return normalizeNextPath(`${window.location.pathname}${window.location.search}${window.location.hash}`);
}

function requestedNextPath() {
	return normalizeNextPath(new URL(window.location.href).searchParams.get('next') || '/');
}

function loginPath(nextPath = '/') {
	const url = new URL('/login', window.location.origin);
	const safeNextPath = normalizeNextPath(nextPath);
	if (safeNextPath !== '/') {
		url.searchParams.set('next', safeNextPath);
	}

	return `${url.pathname}${url.search}`;
}

function panelPathname(path = currentPath()) {
	try {
		return new URL(normalizeNextPath(path), window.location.origin).pathname || '/';
	} catch (error) {
		return '/';
	}
}

function sectionForPath(path = currentPath()) {
	const pathname = panelPathname(path);
	if (pathname === '/overview') {
		return navSections.overview;
	}
	if (pathname.startsWith('/schedules')) {
		return navSections.schedules;
	}
	if (pathname.startsWith('/updates')) {
		return navSections.updates;
	}
	return navSections.actions;
}

function isActionEditorPage() {
	return currentPage === pages.actionEditor;
}

function isScheduleEditorPage() {
	return currentPage === pages.scheduleEditor;
}

function updateNavigationState() {
	const currentSection = sectionForPath();
	elements.navLinks.forEach((link) => {
		const section = link.dataset.panelNav || '';
		const active = section === currentSection;
		link.className = navLinkClass(active);
		if (active) {
			link.setAttribute('aria-current', 'page');
		} else {
			link.removeAttribute('aria-current');
		}
	});

	elements.pageCtas.forEach((link) => {
		const active = link.getAttribute('href') === panelPathname();
		link.className = classNames(
			ui.buttonBase,
			'w-auto',
			active ? '' : 'bg-white text-slate-700 ring-1 ring-slate-200 hover:bg-slate-50 hover:text-slate-900',
		);
		if (active) {
			link.setAttribute('aria-current', 'page');
		} else {
			link.removeAttribute('aria-current');
		}
	});
}

function navigateTo(path) {
	window.location.assign(path);
}

function showLoginView(message, tone = 'info') {
	state.authenticated = false;
	state.sessionUsername = '';
	state.status = null;
	state.actionsSnapshot = null;
	state.scheduleSnapshot = null;
	state.updateSnapshot = null;
	state.actionsBusy = false;
	state.relayCommandPending = false;
	state.relayCommandDesiredState = false;
	state.schedulerBusy = false;
	state.updateBusy = false;
	setActionFeedback(null);
	setRelayFeedback(null);
	setSchedulerFeedback(null);
	setUpdateFeedback(null);
	resetActionForm();
	resetSchedulerForm();
	state.actionEditorSeed = '';
	state.scheduleEditorSeed = '';
	if (!isLoginPage()) {
		stashFlashMessage(message, tone);
		navigateTo(loginPath(currentPath()));
		return;
	}

	if (elements.sessionChip) {
		elements.sessionChip.textContent = 'Session';
		elements.sessionChip.className = badgeClass('warn');
	}
	elements.logoutButton?.setAttribute('disabled', 'disabled');
	elements.refreshButton?.setAttribute('disabled', 'disabled');
	if (message) {
		setAlert(message, tone);
	}
}

function showDashboardView() {
	state.authenticated = true;
	elements.logoutButton?.removeAttribute('disabled');
	elements.refreshButton?.removeAttribute('disabled');
	if (elements.sessionChip) {
		elements.sessionChip.textContent = `Authenticated as ${state.sessionUsername || 'operator'}`;
		elements.sessionChip.className = badgeClass('ok');
	}
	updateNavigationState();
}

function panelRequestTimeoutMs() {
	const configuredTimeout = Number(window.__PANEL_CONFIG__?.requestTimeoutMs || 0);
	if (Number.isFinite(configuredTimeout) && configuredTimeout > 0) {
		return configuredTimeout;
	}

	return 10_000;
}

function delay(ms) {
	return new Promise((resolve) => {
		window.setTimeout(resolve, ms);
	});
}

function requestMethod(options = {}) {
	return String(options.method || 'GET').toUpperCase();
}

function shouldRetryPanelRequest(url, options, attempt, response) {
	return requestMethod(options) === 'GET' &&
		RETRYABLE_PANEL_GET_ROUTES.has(url) &&
		RETRYABLE_PANEL_STATUSES.has(response.status) &&
		attempt + 1 < RETRYABLE_PANEL_ATTEMPTS;
}

function createTimedRequestContext(externalSignals = []) {
	const timeoutMs = panelRequestTimeoutMs();
	const controller = new AbortController();
	const cleanup = [];
	const timeoutId = window.setTimeout(() => {
		controller.abort(new DOMException(`Timed out after ${Math.ceil(timeoutMs / 1000)} seconds.`, 'TimeoutError'));
	}, timeoutMs);

	cleanup.push(() => window.clearTimeout(timeoutId));

	for (const externalSignal of externalSignals.filter(Boolean)) {
		const forwardAbort = () => {
			controller.abort(externalSignal.reason || new DOMException('Request aborted.', 'AbortError'));
		};

		if (externalSignal.aborted) {
			forwardAbort();
		} else {
			externalSignal.addEventListener('abort', forwardAbort, { once: true });
			cleanup.push(() => externalSignal.removeEventListener('abort', forwardAbort));
		}
	}

	return {
		signal: controller.signal,
		timeoutMs,
		cleanup() {
			cleanup.forEach((entry) => entry());
		},
	};
}

function normalizeTimedRequestError(error, timeoutMs) {
	if (error instanceof DOMException && (error.name === 'AbortError' || error.name === 'TimeoutError')) {
		return new Error(`Request timed out after ${Math.ceil(timeoutMs / 1000)} seconds.`);
	}

	return error;
}

async function requestJson(url, options = {}) {
	for (let attempt = 0; attempt < RETRYABLE_PANEL_ATTEMPTS; attempt += 1) {
		const requestContext = createTimedRequestContext([
			options.signal || null,
			state.pageAbortController?.signal || null,
		]);

		try {
			const response = await fetch(url, {
				credentials: 'same-origin',
				headers: {
					'Content-Type': 'application/json',
					...(options.headers || {}),
				},
				...options,
				signal: requestContext.signal,
			});

			if (shouldRetryPanelRequest(url, options, attempt, response)) {
				await delay(RETRYABLE_PANEL_BACKOFF_MS * (attempt + 1));
				continue;
			}

			if (requestMethod(options) === 'GET' &&
				RETRYABLE_PANEL_GET_ROUTES.has(url) &&
				RETRYABLE_PANEL_STATUSES.has(response.status)) {
				throw new Error('Panel is still finishing another request. Retry in a moment.');
			}

			let data = null;
			const contentType = response.headers.get('content-type') || '';
			if (contentType.includes('application/json')) {
				data = await response.json();
			}

			return { response, data };
		} catch (error) {
			if (attempt + 1 >= RETRYABLE_PANEL_ATTEMPTS) {
				throw normalizeTimedRequestError(error, requestContext.timeoutMs);
			}

			throw error;
		} finally {
			requestContext.cleanup();
		}
	}

	throw new Error('Panel request retries were exhausted.');
}

async function requestBinaryUpload(url, file) {
	const response = await fetch(url, {
		method: 'POST',
		credentials: 'same-origin',
		headers: {
			'Content-Type': 'application/octet-stream',
		},
		body: file,
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
			<div class="${ui.metricRow}">
				<div>
					<div class="${ui.metricLabel}">${escapeHtml(label)}</div>
				</div>
				<div class="${ui.metricValue}">${escapeHtml(value)}</div>
			</div>
		`)
		.join('');

	return `
		<p class="${ui.eyebrow}">${escapeHtml(eyebrow)}</p>
		<h3 class="${ui.title}">${escapeHtml(title)}</h3>
		<div class="${ui.metricList}">${metrics}</div>
			${footer}
	`;
}

function navLinkClass(active) {
	return classNames(ui.navLinkBase, active ? ui.navLinkActive : ui.navLinkInactive);
}

function summaryTile(title, value, detail, tone = '') {
	return `
		<article class="${ui.summaryTile}">
			<div class="${ui.rowFlex}">
				<span class="${badgeClass(tone)}">${escapeHtml(title)}</span>
			</div>
			<div class="mt-4">
				<strong class="block text-[1.15rem] font-semibold text-slate-900">${escapeHtml(value)}</strong>
				<p class="mt-2 ${ui.muted}">${escapeHtml(detail)}</p>
			</div>
		</article>
	`;
}

function effectiveRelaySnapshot(relay) {
	if (!relay) {
		return null;
	}

	if (!state.relayCommandPending) {
		return relay;
	}

	return {
		...relay,
		actualState: state.relayCommandDesiredState,
		desiredState: state.relayCommandDesiredState,
		source: 'manual-panel',
		pending: true,
	};
}

function renderPanelSummary() {
	if (!elements.panelSummary) {
		return;
	}

	if (!state.status) {
		elements.panelSummary.innerHTML = [
			summaryTile('Device', 'Loading', 'Waiting for the authenticated shell to load device truth.'),
			summaryTile('Network', 'Loading', 'Checking local reachability and connectivity state.'),
			summaryTile('Actions', 'Loading', 'Primary control surface is not ready yet.'),
			summaryTile('Updates', 'Loading', 'OTA state appears once the update snapshot responds.'),
		].join('');
		return;
	}

	const actionsSnapshot = state.actionsSnapshot;
	const networkLabel = connectivityLabels[state.status.network.connectivity] || humanizeHyphenated(state.status.network.connectivity);
	const updateState = state.updateSnapshot?.state
		? humanizeHyphenated(state.updateSnapshot.state)
		: 'Unavailable';
	const updateDetail = state.updateSnapshot
		? (state.updateSnapshot.applyReady ? 'A staged image is ready for explicit apply.' : (state.updateSnapshot.pendingWarning || 'No staged image is pending.'))
		: 'Update snapshot not loaded yet.';
	const actionCount = Number(actionsSnapshot?.actionCount || 0);
	const readyCount = (actionsSnapshot?.actions || []).filter((action) => action.usabilityCode === 'ready').length;
	const disabledCount = (actionsSnapshot?.actions || []).filter((action) => action.usabilityCode === 'disabled').length;
	const needsAttentionCount = (actionsSnapshot?.actions || []).filter((action) => action.usabilityCode === 'needs-attention').length;
	const actionsValue = !actionsSnapshot
		? 'Loading'
		: (actionCount === 0 ? 'No actions' : `${actionCount} configured`);
	const actionsDetail = !actionsSnapshot
		? 'Configured action truth is still loading.'
		: (actionCount === 0
			? 'Create your first action to bind an approved output and relay command.'
			: `Ready ${readyCount} · Disabled ${disabledCount} · Needs attention ${needsAttentionCount}`);
	const actionsTone = !actionsSnapshot ? '' : (needsAttentionCount > 0 ? 'warn' : (actionCount > 0 ? 'ok' : ''));

	elements.panelSummary.innerHTML = [
		summaryTile('Device', state.status.device.board || 'Unavailable', `Panel port ${state.status.device.panelPort} · APP_READY ${boolLabel(state.status.device.ready)}`, state.status.device.ready ? 'ok' : 'warn'),
		summaryTile('Network', networkLabel, state.status.network.ipv4Address || 'IPv4 pending', state.status.network.reachabilityOk ? 'ok' : 'warn'),
		summaryTile('Actions', actionsValue, actionsDetail, actionsTone),
		summaryTile('Updates', updateState, updateDetail, state.updateSnapshot?.applyReady ? 'warn' : ''),
	].join('');
}

function renderOverviewSurface(statusPayload) {
	if (!elements.cards.device || !elements.cards.network || !elements.cards.recovery) {
		return;
	}

	if (!statusPayload) {
		elements.cards.device.innerHTML = renderCard('Device shell', 'Device', [
			{ label: 'State', value: 'Loading' },
		], `<div class="${ui.placeholder}">Protected device details appear here once authenticated status loads.</div>`);
		elements.cards.network.innerHTML = renderCard('Connectivity', 'Network', [
			{ label: 'State', value: 'Loading' },
		], `<div class="${ui.placeholder}">Network supervision truth appears here once the device responds.</div>`);
		elements.cards.recovery.innerHTML = renderCard('Recovery posture', 'Recovery', [
			{ label: 'State', value: 'Loading' },
		], `<div class="${ui.placeholder}">Recovery breadcrumbs and reset cause stay on the dedicated overview surface.</div>`);
		return;
	}

	const networkLabel = connectivityLabels[statusPayload.network.connectivity] || statusPayload.network.connectivity;
	const degradedCopy = statusPayload.network.connectivity === NETWORK_CONNECTIVITY_LAN_UP_UPSTREAM_DEGRADED
		? `<div class="${ui.placeholder}">Local LAN access is healthy even though upstream internet reachability is degraded. The panel stays locally usable in this state.</div>`
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
	], degradedCopy);

	elements.cards.recovery.innerHTML = renderCard('Recovery posture', 'Recovery', [
		{ label: 'Reset cause available', value: boolLabel(statusPayload.recovery.available) },
		{ label: 'Recovery reset', value: boolLabel(statusPayload.recovery.recoveryReset) },
		{ label: 'Trigger', value: statusPayload.recovery.trigger },
		{ label: 'Failure stage', value: statusPayload.recovery.failureStage },
		{ label: 'Connectivity snapshot', value: statusPayload.recovery.connectivity },
		{ label: 'Reason', value: statusPayload.recovery.reason },
	]);
}

function renderActionsRail(snapshot) {
	if (!elements.cards.actionsInfo) {
		return;
	}

	if (!snapshot) {
		elements.cards.actionsInfo.innerHTML = `
			<p class="${ui.eyebrow}">Catalog posture</p>
			<h3 class="${ui.title}">Waiting for action truth</h3>
			<div class="${ui.placeholder}">Approved outputs, command choices, and operator guidance appear here after the action snapshot loads.</div>
		`;
		return;
	}

	const outputChoices = snapshot.outputChoices || [];
	const commandChoices = snapshot.commandChoices || [];
	const actionCount = Number(snapshot.actionCount || 0);
	const readyCount = (snapshot.actions || []).filter((action) => action.usabilityCode === 'ready').length;
	const needsAttentionCount = (snapshot.actions || []).filter((action) => action.usabilityCode === 'needs-attention').length;

	if (!outputChoices.length) {
		elements.cards.actionsInfo.innerHTML = `
			<p class="${ui.eyebrow}">Catalog posture</p>
			<div class="grid gap-4">
				<div>
					<h3 class="${ui.title}">No approved outputs yet</h3>
					<p class="${ui.muted}">The action catalog is live, but the firmware has not published any approved output bindings yet. Actions stay visible without pretending they are runnable hardware controls.</p>
				</div>
				<div class="${ui.rowFlex}">
					<span class="${badgeClass('warn')}">Outputs missing</span>
					<span class="${badgeClass()}">Catalog only</span>
				</div>
				<div class="${noticeClass('warn')}">Create and edit still work, but downstream schedule and execution flows remain constrained by the server-owned output contract.</div>
			</div>
		`;
		return;
	}

	elements.cards.actionsInfo.innerHTML = `
		<p class="${ui.eyebrow}">Catalog posture</p>
		<div class="grid gap-4">
			<div>
				<h3 class="${ui.title}">Action catalog at a glance</h3>
				<p class="${ui.muted}">This page now focuses on operator-readable catalog state. Dedicated editor routes keep form work separate from the review surface.</p>
			</div>
			<div class="${ui.rowFlex}">
				<span class="${badgeClass(actionCount ? 'ok' : 'warn')}">${actionCount}/${snapshot.maxActions} stored</span>
				<span class="${badgeClass(readyCount ? 'ok' : 'warn')}">${readyCount} ready</span>
				${needsAttentionCount ? `<span class="${badgeClass('warn')}">${needsAttentionCount} need attention</span>` : ''}
			</div>
			<div class="${ui.insetPanel}">
				<div class="${ui.metricRow}">
					<div>
						<div class="${ui.metricLabel}">Approved outputs</div>
					</div>
					<div class="${ui.metricValue}">${outputChoices.length}</div>
				</div>
				<div class="${ui.metricRow}">
					<div>
						<div class="${ui.metricLabel}">Command choices</div>
					</div>
					<div class="${ui.metricValue}">${commandChoices.length}</div>
				</div>
				<div class="${ui.metricRow}">
					<div>
						<div class="${ui.metricLabel}">Next workflow</div>
					</div>
					<div class="${ui.metricValue}">Dedicated editor pages</div>
				</div>
			</div>
			<div class="${noticeClass('info')}">Open a dedicated action editor to create or revise catalog entries without losing context on the action list.</div>
		</div>
	`;
}

function actionUsabilityTone(action) {
	switch (action?.usabilityCode) {
	case 'ready':
		return 'ok';
	case 'disabled':
		return 'warn';
	case 'needs-attention':
	default:
		return 'danger';
	}
}

function actionFeedbackMarkup() {
	if (!state.actionFeedback) {
		return '';
	}

	return `<div class="${noticeClass(state.actionFeedback.tone)}">${escapeHtml(state.actionFeedback.message)}</div>`;
}

function ensureActionFormChoices(snapshot) {
	const outputChoices = snapshot?.outputChoices || [];
	const commandChoices = snapshot?.commandChoices || [];

	if (outputChoices.length && !outputChoices.some((choice) => choice.outputKey === state.actionForm.outputKey)) {
		state.actionForm.outputKey = outputChoices[0].outputKey;
	}

	if (commandChoices.length && !commandChoices.some((choice) => choice.commandKey === state.actionForm.commandKey)) {
		state.actionForm.commandKey = commandChoices[0].commandKey;
	}

	if (state.actionForm.mode === 'edit') {
		const stillExists = (snapshot?.actions || []).some((action) => action.actionId === state.actionForm.actionId);
		if (!stillExists) {
			resetActionForm({
				outputKey: outputChoices[0]?.outputKey || '',
				commandKey: commandChoices[0]?.commandKey || 'relay-on',
			});
		}
	}
}

function renderActionCatalogRows(snapshot) {
	if (!snapshot?.actions?.length) {
		return `
			<div class="${ui.placeholder}">
				<strong class="block text-slate-900">No configured actions</strong>
				<p class="mt-2 ${ui.muted}">Create your first action to bind an approved output and relay command. Manual execution and schedule selection stay deferred until Phase 10.</p>
				<a class="${buttonClass({ full: false })} mt-4" href="/actions/new">Create action</a>
			</div>
		`;
	}

	return snapshot.actions.map((action) => `
		<div class="${ui.schedulerRow}">
			<div class="grid gap-3">
				<div class="${ui.rowFlex}">
					<strong>${escapeHtml(action.displayName)}</strong>
					<span class="${badgeClass(actionUsabilityTone(action))}">${escapeHtml(action.usability)}</span>
					<span class="${badgeClass(action.enabled ? 'ok' : 'warn')}">${action.enabled ? 'Enabled' : 'Disabled'}</span>
				</div>
				<div class="${classNames(ui.rowFlex, ui.muted)}">
					<code class="${ui.code}">${escapeHtml(action.actionId)}</code>
					<span>${escapeHtml(action.outputSummary)}</span>
				</div>
				<div class="${ui.muted}">${escapeHtml(action.usabilityDetail || 'Server-owned action truth')}</div>
			</div>
			<div class="${ui.rowFlex}">
				<a class="${buttonClass({ ghost: true, full: false })}" href="/actions/edit?actionId=${encodeURIComponent(action.actionId)}">Edit action</a>
			</div>
		</div>
	`).join('');
}

function renderActionForm(snapshot) {
	const formState = state.actionForm;
	const outputChoices = snapshot?.outputChoices || [];
	const commandChoices = snapshot?.commandChoices || [];
	const heading = formState.mode === 'edit' ? `Edit ${formState.displayName || formState.actionId}` : 'Create action';
	const primaryLabel = state.actionsBusy
		? (formState.mode === 'edit' ? 'Saving action…' : 'Creating action…')
		: (formState.mode === 'edit' ? 'Save action changes' : 'Create action');

	if (!snapshot) {
		return `
			<div class="${ui.insetPanel}">
				<p class="${ui.eyebrow}">Action form</p>
				<h3 class="${ui.title}">Loading action contract</h3>
				<div class="${ui.placeholder}">Waiting for the configured-action snapshot before the management form can render.</div>
			</div>
		`;
	}

	return `
		<div class="${ui.insetPanel}">
			<p class="${ui.eyebrow}">${escapeHtml(formState.mode === 'edit' ? 'Edit action' : 'Create action')}</p>
			<h3 class="${ui.title}">${escapeHtml(heading)}</h3>
			<p class="${ui.muted}">One configured action equals one executable relay command. This phase only manages the catalog; execute controls and schedule selection stay deferred until Phase 10.</p>
			${actionFeedbackMarkup()}
			<form class="${ui.gridGap3} mt-4" data-action-form>
				${formState.mode === 'edit' ? `
					<div class="grid gap-2">
						<label for="action-id" class="text-sm font-medium text-slate-900">Action ID</label>
						<input id="action-id" class="${ui.inputBase}" name="actionId" value="${escapeHtml(formState.actionId)}" readonly>
					</div>
				` : `<input type="hidden" name="actionId" value="">`}
				<div class="grid gap-2">
					<label for="action-display-name" class="text-sm font-medium text-slate-900">Display name</label>
					<input id="action-display-name" class="${ui.inputBase}" name="displayName" value="${escapeHtml(formState.displayName)}" placeholder="Relay 0 On" required>
				</div>
				<div class="grid gap-2">
					<label for="action-output-key" class="text-sm font-medium text-slate-900">Approved output</label>
					<select id="action-output-key" class="${ui.inputBase}" name="outputKey">
						${outputChoices.map((choice) => `<option value="${escapeHtml(choice.outputKey)}" ${choice.outputKey === formState.outputKey ? 'selected' : ''}>${escapeHtml(choice.label)}</option>`).join('')}
					</select>
				</div>
				<div class="grid gap-2">
					<label for="action-command-key" class="text-sm font-medium text-slate-900">Command</label>
					<select id="action-command-key" class="${ui.inputBase}" name="commandKey">
						${commandChoices.map((choice) => `<option value="${escapeHtml(choice.commandKey)}" ${choice.commandKey === formState.commandKey ? 'selected' : ''}>${escapeHtml(choice.label)}</option>`).join('')}
					</select>
				</div>
				<label class="inline-flex items-center gap-2.5 text-sm text-slate-700">
					<input type="checkbox" name="enabled" ${formState.enabled ? 'checked' : ''}>
					<span>Enabled immediately after save</span>
				</label>
				<div class="${noticeClass('info')}">The server generates the stable action ID on create and preserves it on later edits so future schedule links do not churn when the display name changes.</div>
				<div class="${ui.rowFlex}">
					<button class="${buttonClass({ full: false })}" type="submit" ${state.actionsBusy ? 'disabled' : ''}>${escapeHtml(primaryLabel)}</button>
					<a class="${buttonClass({ ghost: true, full: false })}" href="/actions">Cancel</a>
				</div>
			</form>
		</div>
	`;
}

function renderActionsPage(snapshot) {
	if (!elements.cards.relay || !elements.cards.actionsInfo) {
		return;
	}

	elements.cards.relay.innerHTML = `
		<p class="${ui.eyebrow}">Configured action catalog</p>
		<div class="grid gap-4">
			<div class="flex flex-wrap items-start justify-between gap-4">
				<div>
				<h3 class="${ui.title}">Configured actions</h3>
				<p class="${ui.muted}">Friendly names, approved outputs, enabled state, and usability all come from the device. Create and edit now live on dedicated routes so this page stays review-first.</p>
				</div>
				<a class="${buttonClass({ full: false })}" href="/actions/new">New action</a>
			</div>
			<div class="${ui.rowFlex}">
				<span class="${badgeClass(snapshot?.actionCount ? 'ok' : 'warn')}">${snapshot ? `${snapshot.actionCount}/${snapshot.maxActions} stored` : 'Loading catalog'}</span>
				<span class="${badgeClass()}">Server-truth first</span>
				<span class="${badgeClass('warn')}">Execution deferred</span>
			</div>
			<div class="grid gap-3">${renderActionCatalogRows(snapshot)}</div>
		</div>
	`;
	renderActionsRail(snapshot);
}

function ensureActionEditorSeed(snapshot) {
	if (!isActionEditorPage()) {
		return;
	}

	const actionId = new URL(window.location.href).searchParams.get('actionId') || '';
	const seedKey = actionId || '__create__';
	if (state.actionEditorSeed === seedKey) {
		return;
	}

	ensureActionFormChoices(snapshot);
	if (!actionId) {
		resetActionForm({
			outputKey: snapshot?.outputChoices?.[0]?.outputKey || '',
			commandKey: snapshot?.commandChoices?.[0]?.commandKey || 'relay-on',
		});
		state.actionEditorSeed = seedKey;
		return;
	}

	const action = snapshot?.actions?.find((entry) => entry.actionId === actionId);
	if (!action) {
		resetActionForm({
			outputKey: snapshot?.outputChoices?.[0]?.outputKey || '',
			commandKey: snapshot?.commandChoices?.[0]?.commandKey || 'relay-on',
		});
		setActionFeedback('That configured action is no longer available. You can create a new one instead.', 'warn');
		state.actionEditorSeed = seedKey;
		return;
	}

	resetActionForm({
		mode: 'edit',
		actionId: action.actionId,
		displayName: action.displayName,
		outputKey: action.outputKey,
		commandKey: action.commandKey,
		enabled: action.enabled,
	});
	state.actionEditorSeed = seedKey;
}

function renderActionEditorPage(snapshot) {
	if (!elements.cards.relay || !elements.cards.actionsInfo) {
		return;
	}

	ensureActionEditorSeed(snapshot);
	ensureActionFormChoices(snapshot);
	elements.cards.relay.innerHTML = renderActionForm(snapshot);
	elements.cards.actionsInfo.innerHTML = `
		<p class="${ui.eyebrow}">Editor guide</p>
		<div class="grid gap-4">
			<div>
				<h3 class="${ui.title}">${state.actionForm.mode === 'edit' ? 'Editing catalog entry' : 'Create a catalog entry'}</h3>
				<p class="${ui.muted}">Dedicated editor routes keep list review separate from mutations. Save returns you to the catalog with refreshed device truth.</p>
			</div>
			<div class="${ui.insetPanel}">
				<div class="${ui.metricRow}">
					<div><div class="${ui.metricLabel}">Approved outputs</div></div>
					<div class="${ui.metricValue}">${snapshot?.outputChoices?.length || 0}</div>
				</div>
				<div class="${ui.metricRow}">
					<div><div class="${ui.metricLabel}">Command choices</div></div>
					<div class="${ui.metricValue}">${snapshot?.commandChoices?.length || 0}</div>
				</div>
				<div class="${ui.metricRow}">
					<div><div class="${ui.metricLabel}">Mode</div></div>
					<div class="${ui.metricValue}">${state.actionForm.mode === 'edit' ? 'Edit' : 'Create'}</div>
				</div>
			</div>
			<div class="${noticeClass('info')}">Action IDs stay stable after creation so schedules can continue referring to the same catalog record.</div>
		</div>
	`;
}

function renderCurrentPage() {
	renderPanelSummary();
	switch (currentPage) {
	case pages.overview:
		renderOverviewSurface(state.status);
		break;
	case pages.schedules:
		renderSchedulesPage(state.scheduleSnapshot);
		break;
	case pages.scheduleEditor:
		renderScheduleEditorPage(state.scheduleSnapshot);
		break;
	case pages.updates:
		renderUpdateSurface(state.updateSnapshot || state.status?.update || null);
		break;
	case pages.actionEditor:
		renderActionEditorPage(state.actionsSnapshot);
		break;
	case pages.actions:
	default:
		renderActionsPage(state.actionsSnapshot);
		break;
	}
}

function updateVersionLabel(version) {
	if (!version) {
		return 'Unavailable';
	}

	if (typeof version === 'string') {
		return version || 'Unavailable';
	}

	if (version.label) {
		return version.label;
	}

	if (!version.available) {
		return 'Unavailable';
	}

	return `${version.major}.${version.minor}.${version.revision}+${version.buildNum}`;
}

function updateInputHasSelectedFile() {
	return Boolean(elements.cards.update?.querySelector('[data-update-file]')?.files?.length);
}

function syncUpdateFileMeta() {
	const fileInput = elements.cards.update?.querySelector('[data-update-file]');
	const meta = elements.cards.update?.querySelector('[data-update-file-meta]');
	const file = fileInput?.files?.[0];

	if (!meta) {
		return;
	}

	if (!file) {
		meta.textContent = 'Choose a signed firmware image from this computer. Same-version and downgrade uploads are rejected.';
		return;
	}

	const sizeMb = file.size > 0 ? `${(file.size / (1024 * 1024)).toFixed(2)} MB` : '0.00 MB';
	meta.textContent = `${file.name} · ${sizeMb}`;
}

function setUpdateCardBusy(busy, labels = {}) {
	const uploadButton = elements.cards.update?.querySelector('[data-update-upload]');
	const updateNowButton = elements.cards.update?.querySelector('[data-update-now]');
	const applyButton = elements.cards.update?.querySelector('[data-update-apply]');
	const clearButton = elements.cards.update?.querySelector('[data-update-clear]');

	state.updateBusy = busy;
	setBusy(uploadButton, busy, labels.uploadLabel || 'Stage local firmware');
	setBusy(updateNowButton, busy, labels.updateNowLabel || 'Update now');
	setBusy(applyButton, busy, labels.applyLabel || 'Apply staged update');
	setBusy(clearButton, busy, labels.clearLabel || 'Clear staged image');
}

function updateFeedbackMarkup() {
	if (!state.updateFeedback) {
		return '';
	}

	return `<div class="${noticeClass(state.updateFeedback.tone)}">${escapeHtml(state.updateFeedback.message)}</div>`;
}

function renderUpdateSurface(snapshot) {
	if (!elements.cards.update) {
		return;
	}

	if (!snapshot) {
		elements.cards.update.innerHTML = renderCard('Firmware updates', 'Phase 8 OTA surface', [
			{ label: 'State', value: 'Loading' },
		], `<div class="${ui.placeholder}">Authenticated update routes will populate this surface once the device responds.</div>`);
		return;
	}

	const currentVersion = updateVersionLabel(snapshot.currentVersion);
	const stagedVersion = snapshot.stagedVersion?.available
		? updateVersionLabel(snapshot.stagedVersion)
		: 'No staged image';
	const lastResultCode = snapshot.lastResult?.recorded
		? humanizeHyphenated(snapshot.lastResult.code)
		: 'No result yet';
	const lastResultDetail = snapshot.lastResult?.detail || 'No staged or applied firmware result is recorded yet.';
	const stateLabel = humanizeHyphenated(snapshot.state);
	const remoteBusy = Boolean(snapshot.remoteBusy);
	const remoteState = humanizeHyphenated(snapshot.remoteState || 'idle');
	const applyReady = Boolean(snapshot.applyReady);
	const clearAvailable = snapshot.state && snapshot.state !== 'idle';
	const rollbackDetected = Boolean(snapshot.lastResult?.rollbackDetected);
	const badges = [
		`<span class="${badgeClass(applyReady ? 'warn' : '')}">State ${escapeHtml(stateLabel)}</span>`,
		`<span class="${badgeClass(snapshot.imageConfirmed ? 'ok' : 'warn')}">Image ${snapshot.imageConfirmed ? 'Confirmed' : 'Unconfirmed'}</span>`,
		`<span class="${badgeClass(applyReady ? 'warn' : 'ok')}">Apply ${applyReady ? 'Ready' : 'Not ready'}</span>`,
		`<span class="${badgeClass(remoteBusy ? 'warn' : '')}">Remote ${escapeHtml(remoteState)}</span>`,
		rollbackDetected ? `<span class="${badgeClass('warn')}">Rollback flagged</span>` : '',
	].filter(Boolean).join('');

	elements.cards.update.innerHTML = `
		<p class="${ui.eyebrow}">Phase 8 OTA surface</p>
		<div class="grid gap-[18px]">
			<div>
				<h3 class="${ui.title}">Firmware updates</h3>
				<p class="${ui.muted}">Local uploads stream straight into the shared OTA pipeline, stage first, and only reboot after one explicit operator apply.</p>
			</div>
			<div class="${ui.rowFlex}">${badges}</div>
			<div class="${noticeClass(applyReady || snapshot.state === 'apply-requested' ? 'warn' : 'info')}">
				<strong>${escapeHtml(snapshot.pendingWarning || 'No staged firmware image is waiting.')}</strong>
				<div class="${ui.muted} mt-1.5">${escapeHtml(snapshot.sessionWarning || 'Applying an update clears the current browser session after reboot.')}</div>
			</div>
			<div class="${ui.summaryGrid}">
				<div class="${ui.insetPanel}">
					<span class="${ui.eyebrow}">Current version</span>
					<strong class="mb-1.5 block text-base text-slate-900">${escapeHtml(currentVersion)}</strong>
					<small class="${ui.muted}">${snapshot.currentVersion?.available ? 'Running image from device truth' : 'Running image metadata unavailable'}</small>
				</div>
				<div class="${ui.insetPanel}">
					<span class="${ui.eyebrow}">Staged version</span>
					<strong class="mb-1.5 block text-base text-slate-900">${escapeHtml(stagedVersion)}</strong>
					<small class="${ui.muted}">${snapshot.stagedVersion?.available ? 'Ready to apply after confirmation' : 'No staged image eligible for apply'}</small>
				</div>
				<div class="${ui.insetPanel}">
					<span class="${ui.eyebrow}">Last result</span>
					<strong class="mb-1.5 block text-base text-slate-900">${escapeHtml(lastResultCode)}</strong>
					<small class="${ui.muted}">${escapeHtml(lastResultDetail)}</small>
				</div>
				<div class="${ui.insetPanel}">
					<span class="${ui.eyebrow}">Staging bytes</span>
					<strong class="mb-1.5 block text-base text-slate-900">${escapeHtml(String(snapshot.lastResult?.bytesWritten || 0))}</strong>
					<small class="${ui.muted}">${rollbackDetected ? `Rollback reason ${snapshot.lastResult?.rollbackReason || 0}` : 'Latest OTA attempt byte count'}</small>
				</div>
			</div>
			${updateFeedbackMarkup()}
			<div class="${ui.updateLayout}">
				<div class="${ui.insetPanel}">
					<p class="${ui.eyebrow}">Stage local firmware</p>
					<h3 class="${ui.title} mb-2">Upload a newer signed image</h3>
					<p class="${ui.muted}">The device rejects same-version reinstall and downgrade attempts before the staged image becomes apply-ready.</p>
					<div class="${ui.uploadLayout}">
						<div class="grid gap-2">
							<label for="update-file" class="text-sm font-medium text-slate-900">Firmware image</label>
							<input id="update-file" class="${ui.inputBase}" data-update-file type="file" accept=".bin,.hex,.img,application/octet-stream" ${state.updateBusy || snapshot.state === 'apply-requested' ? 'disabled' : ''}>
						</div>
						<button class="${buttonClass({ full: false })}" type="button" data-update-upload ${state.updateBusy || snapshot.state === 'apply-requested' ? 'disabled' : ''}>Stage local firmware</button>
					</div>
					<div class="${ui.muted} mt-3" data-update-file-meta>Choose a signed firmware image from this computer. Same-version and downgrade uploads are rejected.</div>
				</div>
				<div class="${ui.insetPanel}">
					<p class="${ui.eyebrow}">GitHub Releases</p>
					<h3 class="${ui.title} mb-2">Remote update now</h3>
					<p class="${ui.muted}">The device checks the latest stable release from <code class="${ui.code}">${escapeHtml(snapshot.remotePolicy?.githubOwner || 'lukasa1993')}/${escapeHtml(snapshot.remotePolicy?.githubRepo || 'L-Controller')}</code>, downloads the expected artifact through the same OTA pipeline, and only reboots if a newer image is applied.</p>
					<div class="${ui.rowFlex} mt-4">
						<button class="${buttonClass({ full: false })}" type="button" data-update-now ${state.updateBusy || remoteBusy || snapshot.state !== 'idle' ? 'disabled' : ''}>${remoteBusy ? 'Checking GitHub release…' : 'Update now'}</button>
					</div>
					<div class="${ui.muted} mt-3">Automatic remote checks stay enabled every ${escapeHtml(String(snapshot.remotePolicy?.checkIntervalHours || 24))} hour(s) and retry on future cycles after failure.</div>
				</div>
				<div class="${ui.insetPanel}">
					<p class="${ui.eyebrow}">Apply or clear</p>
					<h3 class="${ui.title} mb-2">Explicit reboot boundary</h3>
					<p class="${ui.muted}">Applying the staged update reboots the device, drops the panel connection, and requires a fresh login once startup completes.</p>
					<div class="${ui.rowFlex} mt-4">
						<button class="${buttonClass({ full: false })}" type="button" data-update-apply ${state.updateBusy || !applyReady ? 'disabled' : ''}>Apply staged update</button>
						<button class="${buttonClass({ ghost: true, full: false })}" type="button" data-update-clear ${state.updateBusy || !clearAvailable ? 'disabled' : ''}>Clear staged image</button>
					</div>
					<div class="${ui.muted} mt-3">The rest of the dashboard remains usable while a staged image waits for explicit apply.</div>
				</div>
			</div>
		</div>
	`;

	syncUpdateFileMeta();
}

function updateNetworkChrome(network) {
	const connectivity = network?.connectivity || 'not-ready';
	const label = connectivityLabels[connectivity] || connectivity;
	const degraded = connectivity === NETWORK_CONNECTIVITY_LAN_UP_UPSTREAM_DEGRADED ||
		connectivity === 'degraded-retrying';

	if (!elements.networkPill) {
		return;
	}

	elements.networkPill.textContent = label;
	elements.networkPill.className = badgeClass(degraded ? 'warn' : 'ok');
}

function relayFeedbackState(relay) {
	if (!relay) {
		return null;
	}

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
	if (!elements.cards.relay) {
		return;
	}

	if (!relay) {
		elements.cards.relay.innerHTML = renderCard('Primary actions', 'Manual control', [
			{ label: 'State', value: 'Loading' },
		], `<div class="${ui.placeholder}">The primary action surface will appear here once authenticated relay truth loads.</div>`);
		return;
	}

	if (relay.configured === false) {
		elements.cards.relay.innerHTML = renderCard('Primary actions', 'Manual control', [
			{ label: 'Configured outputs', value: '0' },
			{ label: 'State', value: 'Not configured' },
		], `<div class="${noticeClass('warn')}">Relay GPIO configuration has not been created yet, so the panel no longer exposes manual relay buttons.</div>`);
		return;
	}

	const actualState = relayStateLabel(relay.actualState);
	const desiredState = relayStateLabel(relay.desiredState);
	const pending = state.relayCommandPending || relay.pending;
	const blocked = relay.blocked;
	const available = relay.implemented && relay.available;
	const mismatch = relay.actualState !== relay.desiredState;
	const feedback = relayFeedbackState(relay);
	const sourceClass = relaySourceBadgeClass(relay.source);
	const actionCopy = !available
		? 'Relay control stays locked until the live runtime becomes available again.'
		: pending
			? 'The control responds immediately, then waits for the refresh cycle to confirm the requested state.'
			: 'Press On or Off and the control will react immediately before live device truth settles the final state.';
	const badgeMarkup = `
		<div class="${ui.rowFlex}">
			<span class="${badgeClass(relay.actualState ? 'ok' : '')}">Actual ${escapeHtml(actualState)}</span>
			<span class="${badgeClass(sourceClass)}">Source ${escapeHtml(humanizeHyphenated(relay.source))}</span>
			${blocked ? `<span class="${badgeClass('warn')}">Blocked</span>` : ''}
			${pending ? `<span class="${badgeClass('warn')}">Pending</span>` : ''}
		</div>
	`;
	const noteMarkup = [
		relay.safetyNote && relay.safetyNote !== 'none'
			? `<div class="${noticeClass('info')}">Safety note: ${escapeHtml(relay.safetyNote)}</div>`
			: '',
		mismatch
			? `<div class="${noticeClass('warn')}">Actual ${escapeHtml(actualState)} differs from remembered desired ${escapeHtml(desiredState)}.</div>`
			: '',
	].join('');
	const feedbackMarkup = feedback
		? `<div class="${noticeClass(feedback.tone)}">${escapeHtml(feedback.message)}</div>`
		: '';
	const actionButtons = `
		<div class="${ui.actionButtons}">
			<button
				class="${classNames(buttonClass(), relay.actualState ? ui.buttonActive : ui.buttonSecondary)}"
				type="button"
				data-relay-set="true"
				${pending || blocked || !available ? 'disabled' : ''}>
				Turn on
			</button>
			<button
				class="${classNames(buttonClass(), !relay.actualState ? ui.buttonActive : ui.buttonSecondary)}"
				type="button"
				data-relay-set="false"
				${pending || blocked || !available ? 'disabled' : ''}>
				Turn off
			</button>
		</div>
	`;

	elements.cards.relay.innerHTML = renderCard('Primary actions', 'Manual control', [
		{ label: 'Active output', value: 'Relay 0' },
		{ label: 'Actual state', value: actualState },
		{ label: 'Remembered desired', value: desiredState },
		{ label: 'Control path', value: available ? 'Available' : 'Unavailable' },
		{ label: 'Reboot policy', value: humanizeHyphenated(relay.rebootPolicy) },
	], `${badgeMarkup}
		<div class="mt-4 rounded-2xl border border-slate-200 bg-slate-50 p-4">
			<div class="${ui.metricLabel}">Immediate action</div>
			<div class="mt-2 text-[1.35rem] font-semibold text-slate-900">${escapeHtml(actualState)}</div>
			<p class="mt-2 ${ui.muted}">${escapeHtml(actionCopy)}</p>
		</div>
		<div class="mt-4 grid gap-3">
			${actionButtons}
		</div>
		${feedbackMarkup}
		${noteMarkup}`);
}

function splitCronExpression(expression) {
	const parts = String(expression || '').trim().split(/\s+/).filter(Boolean);
	if (parts.length !== 5) {
		return {
			minute: '0',
			hour: '*',
			dayOfMonth: '*',
			month: '*',
			dayOfWeek: '*',
		};
	}

	return {
		minute: parts[0],
		hour: parts[1],
		dayOfMonth: parts[2],
		month: parts[3],
		dayOfWeek: parts[4],
	};
}

function schedulerCronExpressionFromForm(formState) {
	return [
		formState.minute,
		formState.hour,
		formState.dayOfMonth,
		formState.month,
		formState.dayOfWeek,
	].map((value) => String(value || '').trim() || '*').join(' ');
}

function syncSchedulerFormFromDom(formElement) {
	if (!formElement) {
		return;
	}

	const formData = new FormData(formElement);
	state.schedulerForm = createSchedulerFormState({
		mode: state.schedulerForm.mode,
		scheduleId: String(formData.get('scheduleId') || '').trim(),
		minute: String(formData.get('minute') || '').trim(),
		hour: String(formData.get('hour') || '').trim(),
		dayOfMonth: String(formData.get('dayOfMonth') || '').trim(),
		month: String(formData.get('month') || '').trim(),
		dayOfWeek: String(formData.get('dayOfWeek') || '').trim(),
		actionKey: String(formData.get('actionKey') || '').trim(),
		enabled: Boolean(formElement.querySelector('[name="enabled"]')?.checked),
	});
}

function ensureSchedulerFormChoice(snapshot) {
	const actionChoices = snapshot?.actionChoices || [];
	if (!actionChoices.length) {
		return;
	}

	if (!actionChoices.some((choice) => choice.key === state.schedulerForm.actionKey)) {
		state.schedulerForm.actionKey = actionChoices[0].key;
	}

	if (state.schedulerForm.mode === 'edit') {
		const stillExists = (snapshot?.schedules || []).some((schedule) => schedule.scheduleId === state.schedulerForm.scheduleId);
		if (!stillExists) {
			resetSchedulerForm({ actionKey: actionChoices[0].key });
		}
	}
}

function schedulerSummaryCard(title, value, detail) {
	return `
		<div class="${ui.insetPanel}">
			<span class="${ui.eyebrow}">${escapeHtml(title)}</span>
			<strong class="mb-1.5 block text-base text-slate-900">${escapeHtml(value)}</strong>
			<small class="${ui.muted}">${escapeHtml(detail)}</small>
		</div>
	`;
}

function schedulerFeedbackMarkup() {
	if (!state.schedulerFeedback) {
		return '';
	}

	return `<div class="${noticeClass(state.schedulerFeedback.tone)}">${escapeHtml(state.schedulerFeedback.message)}</div>`;
}

function schedulerOutputsConfigured(snapshot) {
	return Boolean(snapshot?.outputsConfigured);
}

function renderSchedulerRows(snapshot) {
	if (!schedulerOutputsConfigured(snapshot)) {
		return `<div class="${ui.placeholder}">No GPIO-backed outputs are configured yet, so relay schedules are hidden until that configuration exists.</div>`;
	}

	if (!snapshot.schedules?.length) {
		return `<div class="${ui.placeholder}">No saved schedules yet. Create one below to start the scheduler flow.</div>`;
	}

	return snapshot.schedules.map((schedule) => `
		<div class="${ui.schedulerRow}">
			<div>
				<div class="${ui.rowFlex}">
					<strong>${escapeHtml(schedule.scheduleId)}</strong>
					<span class="${badgeClass(schedule.enabled ? 'ok' : 'warn')}">${schedule.enabled ? 'Enabled' : 'Disabled'}</span>
					${schedule.isNextRun ? `<span class="${badgeClass('ok')}">Next run</span>` : ''}
				</div>
				<div class="${classNames(ui.rowFlex, ui.muted)}">
					<span>${escapeHtml(schedule.actionLabel)}</span>
					<code class="${ui.code}">${escapeHtml(schedule.cronExpression)}</code>
				</div>
			</div>
			<div class="${ui.rowFlex}">
				<a class="${buttonClass({ ghost: true, full: false })}" href="/schedules/edit?scheduleId=${encodeURIComponent(schedule.scheduleId)}">Edit</a>
				<button class="${buttonClass({ ghost: true, full: false })}" type="button" data-scheduler-toggle="${escapeHtml(schedule.scheduleId)}" data-enabled="${schedule.enabled ? 'false' : 'true'}" ${state.schedulerBusy ? 'disabled' : ''}>${schedule.enabled ? 'Disable' : 'Enable'}</button>
				<button class="${buttonClass({ ghost: true, full: false })}" type="button" data-scheduler-delete="${escapeHtml(schedule.scheduleId)}" ${state.schedulerBusy ? 'disabled' : ''}>Delete</button>
			</div>
		</div>
	`).join('');
}

function renderSchedulerProblems(snapshot) {
	if (!snapshot.problems?.length) {
		return `<div class="${ui.placeholder}">No recent scheduler problems are recorded right now.</div>`;
	}

	return `
		<ul class="${ui.list}">
			${snapshot.problems.map((problem) => `
				<li>
					<strong>${escapeHtml(humanizeHyphenated(problem.code))}</strong>
					— ${escapeHtml(problem.scheduleId && problem.scheduleId !== 'none' ? problem.scheduleId : 'global scheduler state')}
					${problem.actionLabel && problem.actionLabel !== 'none' ? ` · ${escapeHtml(problem.actionLabel)}` : ''}
					${Number(problem.normalizedUtcMinute) >= 0 ? ` · ${escapeHtml(formatUtcMinute(problem.normalizedUtcMinute))}` : ''}
				</li>
			`).join('')}
		</ul>
	`;
}

function renderSchedulerForm(snapshot) {
	const actionChoices = snapshot.actionChoices || [];
	const formState = state.schedulerForm;
	const cronPreview = schedulerCronExpressionFromForm(formState);
	const primaryLabel = state.schedulerBusy
		? (formState.mode === 'edit' ? 'Saving schedule…' : 'Creating schedule…')
		: (formState.mode === 'edit' ? 'Save schedule changes' : 'Create schedule');
	const heading = formState.mode === 'edit'
		? `Edit ${formState.scheduleId}`
		: 'Create schedule';

	if (!schedulerOutputsConfigured(snapshot) || !actionChoices.length) {
		return `
			<div class="${ui.insetPanel}">
				<p class="${ui.eyebrow}">Output configuration required</p>
				<h3 class="${ui.title}">No schedulable outputs yet</h3>
				<div class="${noticeClass('warn')}">Relay schedules stay hidden until GPIO-backed outputs are explicitly configured. The current hard-wired relay path is no longer exposed as if it were ready.</div>
			</div>
		`;
	}

	return `
		<div class="${ui.insetPanel}">
			<p class="${ui.eyebrow}">${escapeHtml(formState.mode === 'edit' ? 'Edit schedule' : 'Create schedule')}</p>
			<h3 class="${ui.title}">${escapeHtml(heading)}</h3>
			<p class="${ui.muted}">Dedicated schedule editor routes keep the list view focused on live automation status and recent problems.</p>
			${schedulerFeedbackMarkup()}
			<form class="${ui.gridGap3}" data-scheduler-form>
				<div class="grid gap-2">
					<label for="schedule-id" class="text-sm font-medium text-slate-900">Schedule ID</label>
					<input id="schedule-id" class="${ui.inputBase}" name="scheduleId" value="${escapeHtml(formState.scheduleId)}" ${formState.mode === 'edit' ? 'readonly' : ''} required>
				</div>
				<div class="grid gap-2">
					<label for="schedule-action" class="text-sm font-medium text-slate-900">Action</label>
					<select id="schedule-action" class="${ui.inputBase}" name="actionKey">
						${actionChoices.map((choice) => `<option value="${escapeHtml(choice.key)}" ${choice.key === formState.actionKey ? 'selected' : ''}>${escapeHtml(choice.label)}</option>`).join('')}
					</select>
				</div>
				<div class="${ui.schedulerFields}">
					<div class="grid gap-2">
						<label for="cron-minute" class="text-sm font-medium text-slate-900">Minute</label>
						<input id="cron-minute" class="${ui.inputBase}" name="minute" value="${escapeHtml(formState.minute)}" placeholder="0" required>
					</div>
					<div class="grid gap-2">
						<label for="cron-hour" class="text-sm font-medium text-slate-900">Hour</label>
						<input id="cron-hour" class="${ui.inputBase}" name="hour" value="${escapeHtml(formState.hour)}" placeholder="*" required>
					</div>
					<div class="grid gap-2">
						<label for="cron-dom" class="text-sm font-medium text-slate-900">Day of month</label>
						<input id="cron-dom" class="${ui.inputBase}" name="dayOfMonth" value="${escapeHtml(formState.dayOfMonth)}" placeholder="*" required>
					</div>
					<div class="grid gap-2">
						<label for="cron-month" class="text-sm font-medium text-slate-900">Month</label>
						<input id="cron-month" class="${ui.inputBase}" name="month" value="${escapeHtml(formState.month)}" placeholder="*" required>
					</div>
					<div class="grid gap-2">
						<label for="cron-dow" class="text-sm font-medium text-slate-900">Day of week</label>
						<input id="cron-dow" class="${ui.inputBase}" name="dayOfWeek" value="${escapeHtml(formState.dayOfWeek)}" placeholder="*" required>
					</div>
				</div>
				<label class="inline-flex items-center gap-2.5 text-sm text-slate-700">
					<input type="checkbox" name="enabled" ${formState.enabled ? 'checked' : ''}>
					<span>Enabled immediately after save</span>
				</label>
				<div class="${ui.muted}">
					All schedule fields are explicit <code class="${ui.code}">UTC</code>. Preview: <code class="${ui.code}">${escapeHtml(cronPreview)}</code>
				</div>
				<div class="${ui.rowFlex}">
					<button class="${buttonClass({ full: false })}" type="submit" ${state.schedulerBusy ? 'disabled' : ''}>${escapeHtml(primaryLabel)}</button>
					<a class="${buttonClass({ ghost: true, full: false })}" href="/schedules">Cancel</a>
				</div>
			</form>
		</div>
	`;
}

function renderSchedulesPage(snapshot) {
	if (!elements.cards.scheduler) {
		return;
	}

	if (!snapshot) {
		elements.cards.scheduler.innerHTML = renderCard('Schedule management', 'Phase 7 live surface', [
			{ label: 'State', value: 'Loading' },
		], `<div class="${ui.placeholder}">Authenticated schedule routes will populate this surface once the device responds.</div>`);
		return;
	}

	ensureSchedulerFormChoice(snapshot);
	const nextRunCopy = snapshot.nextRun?.available
		? `${formatUtcMinute(snapshot.nextRun.normalizedUtcMinute)} · ${snapshot.nextRun.actionLabel}`
		: 'No future enabled run is currently queued';
	const lastResultCopy = snapshot.lastResult?.available
		? `${humanizeHyphenated(snapshot.lastResult.code)} · ${formatUtcMinute(snapshot.lastResult.normalizedUtcMinute)}`
		: 'No last result recorded yet';
	const clockCopy = snapshot.clockTrusted
		? 'Trusted UTC time acquired'
		: `Clock ${humanizeHyphenated(snapshot.clockState)}`;
	const automationCopy = snapshot.automationActive
		? 'Automation active for enabled schedules'
		: 'Automation inactive until clock trust and enabled schedules align';
	const historyCopy = snapshot.problemCount
		? `${snapshot.problemCount} recent scheduler problems recorded`
		: 'No recent scheduler problems';
	const schedulerIntro = schedulerOutputsConfigured(snapshot)
		? 'Create, review, enable, disable, and delete UTC cron schedules without hiding the live automation state behind the editor form.'
		: 'No GPIO-backed outputs are configured yet, so schedule creation stays unavailable until that configuration model exists.';

	elements.cards.scheduler.innerHTML = `
		<p class="${ui.eyebrow}">Phase 7 live surface</p>
		<div class="grid gap-4">
			<div class="flex flex-wrap items-start justify-between gap-4">
				<div>
				<h3 class="${ui.title}">Schedule management</h3>
				<p class="${ui.muted}">${escapeHtml(schedulerIntro)}</p>
				</div>
				<a class="${buttonClass({ full: false })}" href="/schedules/new">New schedule</a>
			</div>
			<div class="${ui.schedulerHeader}">
				<div class="${ui.insetPanel}">
					<p class="${ui.eyebrow}">Clock and automation</p>
					<div class="${ui.rowFlex}">
						<span class="${badgeClass(schedulerClockTone(snapshot.clockState))}">Clock ${escapeHtml(humanizeHyphenated(snapshot.clockState))}</span>
						<span class="${badgeClass(snapshot.automationActive ? 'ok' : 'warn')}">Automation ${snapshot.automationActive ? 'Active' : 'Inactive'}</span>
						<span class="${badgeClass()}">UTC only</span>
					</div>
					<div class="${ui.muted}">
						${escapeHtml(clockCopy)}${snapshot.degradedReason && snapshot.degradedReason !== 'none' ? ` · ${escapeHtml(humanizeHyphenated(snapshot.degradedReason))}` : ''}
					</div>
				</div>
				<div class="${ui.insetPanel}">
					<p class="${ui.eyebrow}">Scheduler history</p>
					<div class="${ui.muted}">${escapeHtml(historyCopy)}</div>
					<div class="${ui.muted}">${escapeHtml(automationCopy)}</div>
				</div>
			</div>
			<div class="${ui.summaryGrid}">
				${schedulerSummaryCard('Next run', nextRunCopy, snapshot.nextRun?.available ? `${snapshot.nextRun.scheduleId} · ${snapshot.nextRun.actionKey}` : 'Trusted time is required before a next run can appear')}
				${schedulerSummaryCard('Last result', lastResultCopy, snapshot.lastResult?.available ? `${snapshot.lastResult.scheduleId || 'scheduler'} · ${snapshot.lastResult.actionLabel}` : 'A due schedule minute must run before last result appears')}
				${schedulerSummaryCard('Clock', clockCopy, snapshot.degradedReason && snapshot.degradedReason !== 'none' ? humanizeHyphenated(snapshot.degradedReason) : 'Scheduler uses future-only UTC baselines')}
				${schedulerSummaryCard('Schedule counts', `${snapshot.enabledCount}/${snapshot.scheduleCount} enabled`, `Up to ${snapshot.maxSchedules} schedules in this phase`) }
			</div>
			${schedulerFeedbackMarkup()}
			<div class="${ui.insetPanel}">
				<p class="${ui.eyebrow}">Saved schedules</p>
				<div class="${ui.muted}">Dedicated editor routes handle create and edit. This list stays focused on live truth, toggles, deletes, and recent run context.</div>
				<div class="mt-4 grid gap-3">${renderSchedulerRows(snapshot)}</div>
			</div>
			<div class="${ui.insetPanel}">
				<p class="${ui.eyebrow}">Recent problems and history</p>
				${renderSchedulerProblems(snapshot)}
			</div>
		</div>
	`;
}

function ensureScheduleEditorSeed(snapshot) {
	if (!isScheduleEditorPage()) {
		return;
	}

	const scheduleId = new URL(window.location.href).searchParams.get('scheduleId') || '';
	const seedKey = scheduleId || '__create__';
	if (state.scheduleEditorSeed === seedKey) {
		return;
	}

	ensureSchedulerFormChoice(snapshot);
	if (!scheduleId) {
		resetSchedulerForm({ actionKey: snapshot?.actionChoices?.[0]?.key || 'relay-on' });
		state.scheduleEditorSeed = seedKey;
		return;
	}

	const schedule = snapshot?.schedules?.find((entry) => entry.scheduleId === scheduleId);
	if (!schedule) {
		resetSchedulerForm({ actionKey: snapshot?.actionChoices?.[0]?.key || 'relay-on' });
		setSchedulerFeedback('That schedule is no longer available. You can create a new one instead.', 'warn');
		state.scheduleEditorSeed = seedKey;
		return;
	}

	resetSchedulerForm({
		mode: 'edit',
		scheduleId: schedule.scheduleId,
		actionKey: schedule.actionKey,
		enabled: schedule.enabled,
		...splitCronExpression(schedule.cronExpression),
	});
	state.scheduleEditorSeed = seedKey;
}

function renderScheduleEditorPage(snapshot) {
	if (!elements.cards.scheduler) {
		return;
	}

	if (!snapshot) {
		elements.cards.scheduler.innerHTML = renderCard('Schedule editor', 'Scheduler', [
			{ label: 'State', value: 'Loading' },
		], `<div class="${ui.placeholder}">Authenticated scheduler data is still loading.</div>`);
		return;
	}

	ensureScheduleEditorSeed(snapshot);
	ensureSchedulerFormChoice(snapshot);
	const nextRunCopy = snapshot.nextRun?.available
		? `${formatUtcMinute(snapshot.nextRun.normalizedUtcMinute)} · ${snapshot.nextRun.actionLabel}`
		: 'No future enabled run is currently queued';

	elements.cards.scheduler.innerHTML = `
		<p class="${ui.eyebrow}">Schedule editor</p>
		<div class="grid gap-6 xl:grid-cols-[minmax(0,1.2fr)_320px]">
			<div>${renderSchedulerForm(snapshot)}</div>
			<div class="grid gap-4">
				<div class="${ui.insetPanel}">
					<p class="${ui.eyebrow}">Automation context</p>
					<div class="${ui.metricRow}">
						<div><div class="${ui.metricLabel}">Clock state</div></div>
						<div class="${ui.metricValue}">${escapeHtml(humanizeHyphenated(snapshot.clockState))}</div>
					</div>
					<div class="${ui.metricRow}">
						<div><div class="${ui.metricLabel}">Automation</div></div>
						<div class="${ui.metricValue}">${snapshot.automationActive ? 'Active' : 'Inactive'}</div>
					</div>
					<div class="${ui.metricRow}">
						<div><div class="${ui.metricLabel}">Next run</div></div>
						<div class="${ui.metricValue}">${escapeHtml(nextRunCopy)}</div>
					</div>
				</div>
				<div class="${ui.insetPanel}">
					<p class="${ui.eyebrow}">Recent problems</p>
					${renderSchedulerProblems(snapshot)}
				</div>
			</div>
		</div>
	`;
}

function renderStatus(statusPayload, updatePayload = null) {
	state.status = statusPayload;
	state.updateSnapshot = updatePayload;
	updateNetworkChrome(statusPayload.network);
	renderCurrentPage();
}

async function refreshDashboard({ silent = false } = {}) {
	state.refreshNeedsAnnouncement = state.refreshNeedsAnnouncement || !silent;
	if (state.refreshPromise) {
		return state.refreshPromise;
	}

	state.refreshPromise = (async () => {
		try {
			// Keep the device load predictable: one refresh flow at a time, one endpoint at a time.
			const statusResult = await requestJson(routes.status, { method: 'GET' });
			if (statusResult.response.status === 401) {
				showLoginView(SESSION_EXPIRED_MESSAGE, 'warn');
				return false;
			}
			if (!statusResult.response.ok || !statusResult.data) {
				throw new Error(`Status refresh failed (${statusResult.response.status})`);
			}

			const actionsResult = await requestJson(routes.actions, { method: 'GET' });
			if (actionsResult.response.status === 401) {
				showLoginView(SESSION_EXPIRED_MESSAGE, 'warn');
				return false;
			}
			if (!actionsResult.response.ok || !actionsResult.data) {
				throw new Error(`Action refresh failed (${actionsResult.response.status})`);
			}

			const updateResult = await requestJson(routes.updateStatus, { method: 'GET' });
			if (updateResult.response.status === 401) {
				showLoginView(SESSION_EXPIRED_MESSAGE, 'warn');
				return false;
			}
			if (!updateResult.response.ok || !updateResult.data) {
				throw new Error(`Update refresh failed (${updateResult.response.status})`);
			}

			const schedulesResult = await requestJson(routes.schedules, { method: 'GET' });
			if (schedulesResult.response.status === 401) {
				showLoginView(SESSION_EXPIRED_MESSAGE, 'warn');
				return false;
			}
			if (!schedulesResult.response.ok || !schedulesResult.data) {
				throw new Error(`Schedule refresh failed (${schedulesResult.response.status})`);
			}

			state.status = statusResult.data;
			state.actionsSnapshot = actionsResult.data;
			state.scheduleSnapshot = schedulesResult.data;
			state.updateSnapshot = updateResult.data;
			if (!state.authenticated) {
				showDashboardView();
			} else {
				renderCurrentPage();
			}
			updateNetworkChrome(statusResult.data.network);
			if (state.refreshNeedsAnnouncement) {
				setAlert('Protected status, actions, schedules, and OTA truth refreshed from the device.', 'success');
			}
			return true;
		} catch (error) {
			if (state.updateSnapshot?.state === 'apply-requested') {
				setAlert('Device reboot is in progress for the staged update. Log in again once it returns.', 'warn');
				return false;
			}

			setAlert(error instanceof Error ? error.message : 'Status refresh failed.', 'error');
			return false;
		} finally {
			state.refreshPromise = null;
			state.refreshNeedsAnnouncement = false;
		}
	})();

	return state.refreshPromise;
}

async function handleRelaySet(desiredState) {
	const relay = state.status?.relay;
	if (!relay || state.relayCommandPending || relay.blocked || !relay.implemented || !relay.available) {
		return;
	}

	if (relay.configured === false) {
		setRelayFeedback('Relay GPIO is not configured yet.', 'warn');
		setAlert('Relay GPIO is not configured yet.', 'warn');
		renderCurrentPage();
		return;
	}

	const normalizedDesiredState = Boolean(desiredState);
	if (!state.relayCommandPending && relay.actualState === normalizedDesiredState &&
		relay.desiredState === normalizedDesiredState) {
		return;
	}

	state.relayCommandPending = true;
	state.relayCommandDesiredState = normalizedDesiredState;
	setRelayFeedback(`Pending: requesting relay ${normalizedDesiredState ? 'on' : 'off'} from the device…`, 'warn');
	renderCurrentPage();

	try {
		const { response, data } = await requestJson(routes.relayDesiredState, {
			method: 'POST',
			body: JSON.stringify({ desiredState: normalizedDesiredState }),
		});

		if (response.status === 401) {
			showLoginView(SESSION_EXPIRED_MESSAGE, 'warn');
			return;
		}

		if (!response.ok || !data?.accepted) {
			const detail = data?.detail || data?.error || `Relay command failed (${response.status})`;
			throw new Error(`Relay command failed: ${detail}`);
		}

		const refreshed = await refreshDashboard({ silent: true });
		if (!refreshed) {
			setRelayFeedback('Relay command was accepted, but live refresh did not complete yet.', 'warn');
			return;
		}

		setRelayFeedback(null);
		setAlert(`Relay ${normalizedDesiredState ? 'on' : 'off'} request applied.`, 'success');
	} catch (error) {
		const message = error instanceof Error ? error.message : 'Relay command failed.';
		setRelayFeedback(message, 'error');
		setAlert(message, 'error');
	} finally {
		state.relayCommandPending = false;
		state.relayCommandDesiredState = false;
		renderCurrentPage();
	}
}

function syncActionFormFromDom(formElement) {
	if (!formElement) {
		return;
	}

	const formData = new FormData(formElement);
	state.actionForm = createActionFormState({
		mode: state.actionForm.mode,
		actionId: String(formData.get('actionId') || '').trim(),
		displayName: String(formData.get('displayName') || '').trim(),
		outputKey: String(formData.get('outputKey') || '').trim(),
		commandKey: String(formData.get('commandKey') || '').trim(),
		enabled: Boolean(formElement.querySelector('[name="enabled"]')?.checked),
	});
}

function actionApiErrorMessage(data, fallbackPrefix) {
	const fieldPrefix = data?.field
		? `${humanizeHyphenated(String(data.field).replace(/[A-Z]/g, (match) => `-${match.toLowerCase()}`))}: `
		: '';
	return `${fallbackPrefix}${fieldPrefix}${data?.detail || data?.error || 'Unknown error.'}`;
}

async function runActionMutation(route, payload) {
	state.actionsBusy = true;
	setActionFeedback('Refreshing configured action truth after this change…', 'warn');
	renderCurrentPage();

	try {
		const { response, data } = await requestJson(route, {
			method: 'POST',
			body: JSON.stringify(payload),
		});

		if (response.status === 401) {
			showLoginView(SESSION_EXPIRED_MESSAGE, 'warn');
			return false;
		}

		if (!response.ok || !data?.accepted) {
			throw new Error(actionApiErrorMessage(data, 'Configured action update failed: '));
		}

		const refreshed = await refreshDashboard({ silent: true });
		if (!refreshed) {
			setActionFeedback('The configured action change was accepted, but the live action refresh did not complete yet.', 'warn');
			return false;
		}

		setActionFeedback(null);
		setAlert(data.detail || 'Configured action updated.', 'success');
		return true;
	} catch (error) {
		const message = error instanceof Error ? error.message : 'Configured action update failed.';
		setActionFeedback(message, 'error');
		setAlert(message, 'error');
		return false;
	} finally {
		state.actionsBusy = false;
		renderCurrentPage();
	}
}

async function handleActionFormSubmit() {
	const formState = state.actionForm;
	if (!state.actionsSnapshot) {
		setActionFeedback('Configured action truth is still loading.', 'warn');
		renderCurrentPage();
		return;
	}

	const payload = {
		displayName: formState.displayName.trim(),
		outputKey: formState.outputKey,
		commandKey: formState.commandKey,
		enabled: formState.enabled,
	};

	if (formState.mode === 'edit') {
		payload.actionId = formState.actionId;
	}

	if (!payload.displayName || !payload.outputKey || !payload.commandKey) {
		setActionFeedback('Enter a display name and choose both an approved output and command before saving.', 'warn');
		setAlert('Enter a display name and choose both an approved output and command before saving.', 'warn');
		renderCurrentPage();
		return;
	}

	const route = formState.mode === 'edit' ? routes.actionUpdate : routes.actionCreate;
	const ok = await runActionMutation(route, payload);
	if (ok) {
		stashFlashMessage(formState.mode === 'edit' ? 'Configured action updated.' : 'Configured action created.', 'success');
		navigateTo('/actions');
	}
}

function schedulerApiErrorMessage(data, fallbackPrefix) {
	const fieldPrefix = data?.field ? `${humanizeHyphenated(String(data.field).replace(/[A-Z]/g, (match) => `-${match.toLowerCase()}`))}: ` : '';
	return `${fallbackPrefix}${fieldPrefix}${data?.detail || data?.error || 'Unknown error.'}`;
}

async function runSchedulerMutation(route, payload) {
	state.schedulerBusy = true;
	setSchedulerFeedback('Refreshing scheduler truth after this change…', 'warn');
	renderCurrentPage();

	try {
		const { response, data } = await requestJson(route, {
			method: 'POST',
			body: JSON.stringify(payload),
		});

		if (response.status === 401) {
			showLoginView(SESSION_EXPIRED_MESSAGE, 'warn');
			return false;
		}

		if (!response.ok || !data?.accepted) {
			throw new Error(schedulerApiErrorMessage(data, 'Schedule update failed: '));
		}

		const refreshed = await refreshDashboard({ silent: true });
		if (!refreshed) {
			setSchedulerFeedback('The schedule change was accepted, but the live scheduler refresh did not complete yet.', 'warn');
			return false;
		}

		setSchedulerFeedback(null);
		setAlert(data.detail || 'Schedule updated.', 'success');
		return true;
	} catch (error) {
		const message = error instanceof Error ? error.message : 'Schedule update failed.';
		setSchedulerFeedback(message, 'error');
		setAlert(message, 'error');
		return false;
	} finally {
		state.schedulerBusy = false;
		renderCurrentPage();
	}
}

async function handleSchedulerFormSubmit() {
	const formState = state.schedulerForm;
	if (!schedulerOutputsConfigured(state.scheduleSnapshot)) {
		setSchedulerFeedback('Configure GPIO-backed outputs before creating relay schedules.', 'warn');
		setAlert('Configure GPIO-backed outputs before creating relay schedules.', 'warn');
		renderCurrentPage();
		return;
	}

	const payload = {
		scheduleId: formState.scheduleId.trim(),
		cronExpression: schedulerCronExpressionFromForm(formState),
		actionKey: formState.actionKey,
		enabled: formState.enabled,
	};

	if (!payload.scheduleId || !payload.actionKey) {
		setSchedulerFeedback('Enter a schedule ID and choose an action before saving.', 'warn');
		setAlert('Enter a schedule ID and choose an action before saving.', 'warn');
		renderCurrentPage();
		return;
	}

	const route = formState.mode === 'edit' ? routes.scheduleUpdate : routes.scheduleCreate;
	const ok = await runSchedulerMutation(route, payload);
	if (ok) {
		stashFlashMessage(formState.mode === 'edit' ? 'Schedule updated.' : 'Schedule created.', 'success');
		navigateTo('/schedules');
	}
}

async function handleSchedulerToggle(scheduleId, enabled) {
	await runSchedulerMutation(routes.scheduleSetEnabled, { scheduleId, enabled });
}

async function handleSchedulerDelete(scheduleId) {
	if (!window.confirm(`Delete schedule ${scheduleId}?`)) {
		return;
	}

	const ok = await runSchedulerMutation(routes.scheduleDelete, { scheduleId });
	if (ok && state.schedulerForm.mode === 'edit' && state.schedulerForm.scheduleId === scheduleId) {
		resetSchedulerForm({ actionKey: state.scheduleSnapshot?.actionChoices?.[0]?.key || 'relay-on' });
		renderCurrentPage();
	}
}

function handleUpdateFileChange() {
	if (state.updateFeedback?.tone === 'error') {
		setUpdateFeedback(null);
	}

	syncUpdateFileMeta();
}

async function handleUpdateUpload() {
	const fileInput = elements.cards.update?.querySelector('[data-update-file]');
	const file = fileInput?.files?.[0];

	if (!file || state.updateBusy) {
		setUpdateFeedback('Choose a firmware image before staging it.', 'warn');
		setAlert('Choose a firmware image before staging it.', 'warn');
		renderUpdateSurface(state.updateSnapshot);
		return;
	}

	setUpdateCardBusy(true, {
		uploadLabel: 'Staging firmware…',
		applyLabel: 'Apply staged update',
		clearLabel: 'Clear staged image',
	});
	setAlert('Streaming the firmware image into the device OTA slot…', 'info');

	try {
		const { response, data } = await requestBinaryUpload(routes.updateUpload, file);

		if (response.status === 401) {
			showLoginView(SESSION_EXPIRED_MESSAGE, 'warn');
			return;
		}

		if (!response.ok || !data?.accepted) {
			throw new Error(data?.detail || `Firmware upload failed (${response.status})`);
		}

		setUpdateFeedback(data.detail || 'Firmware image staged.', 'success');
		setAlert(data.detail || 'Firmware image staged.', 'success');
				await refreshDashboard({ silent: false });
	} catch (error) {
		const message = error instanceof Error ? error.message : 'Firmware upload failed.';
		setUpdateFeedback(message, 'error');
		setAlert(message, 'error');
	} finally {
		setUpdateCardBusy(false);
		renderUpdateSurface(state.updateSnapshot);
	}
}

async function handleUpdateNow() {
	if (state.updateBusy) {
		return;
	}

	setUpdateCardBusy(true, {
		uploadLabel: 'Stage local firmware',
		updateNowLabel: 'Checking GitHub release…',
		applyLabel: 'Apply staged update',
		clearLabel: 'Clear staged image',
	});
	setAlert('Checking GitHub Releases for the latest stable update…', 'info');

	try {
		const { response, data } = await requestJson(routes.updateNow, { method: 'POST' });

		if (response.status === 401) {
			showLoginView(SESSION_EXPIRED_MESSAGE, 'warn');
			return;
		}

		if (!response.ok || !data?.accepted) {
			throw new Error(data?.detail || `Update now failed (${response.status})`);
		}

		setUpdateFeedback(data.detail || 'GitHub update check started.', 'info');
		setAlert(data.detail || 'GitHub update check started.', 'info');
				await refreshDashboard({ silent: false });
	} catch (error) {
		const message = error instanceof Error ? error.message : 'Update now failed.';
		setUpdateFeedback(message, 'error');
		setAlert(message, 'error');
	} finally {
		setUpdateCardBusy(false);
		renderUpdateSurface(state.updateSnapshot);
	}
}

async function handleUpdateClear() {
	if (!state.updateSnapshot || state.updateBusy) {
		return;
	}

	const confirmed = window.confirm(
		state.updateSnapshot.state === 'staging'
			? 'Cancel the current staging attempt and clear the OTA slot eligibility?'
			: 'Clear the staged firmware image so it can no longer be applied?'
	);
	if (!confirmed) {
		return;
	}

	setUpdateCardBusy(true, {
		uploadLabel: 'Stage local firmware',
		applyLabel: 'Apply staged update',
		clearLabel: 'Clearing staged image…',
	});
	setAlert('Clearing staged firmware eligibility…', 'info');

	try {
		const { response, data } = await requestJson(routes.updateClear, { method: 'POST' });

		if (response.status === 401) {
			showLoginView(SESSION_EXPIRED_MESSAGE, 'warn');
			return;
		}

		if (!response.ok || !data?.accepted) {
			throw new Error(data?.detail || `Clear staged image failed (${response.status})`);
		}

		if (state.updateSnapshot) {
			state.updateSnapshot = {
				...state.updateSnapshot,
				state: 'idle',
				applyReady: false,
				stagedVersion: { ...(state.updateSnapshot.stagedVersion || {}), available: false, label: 'Unavailable' },
				pendingWarning: 'No staged firmware image is currently waiting for apply.',
			};
		}

		setUpdateFeedback(data.detail || 'Staged firmware eligibility cleared.', 'success');
		setAlert(data.detail || 'Staged firmware eligibility cleared.', 'success');
		await refreshDashboard({ silent: true });
	} catch (error) {
		const message = error instanceof Error ? error.message : 'Clear staged image failed.';
		setUpdateFeedback(message, 'error');
		setAlert(message, 'error');
	} finally {
		setUpdateCardBusy(false);
		renderUpdateSurface(state.updateSnapshot);
	}
}

async function handleUpdateApply() {
	if (!state.updateSnapshot?.applyReady || state.updateBusy) {
		return;
	}

	const stagedVersion = updateVersionLabel(state.updateSnapshot.stagedVersion);
	const confirmation = window.confirm(
		`Apply staged firmware ${stagedVersion} now?\n\nThis will reboot the device, drop the panel connection, and require a fresh login after startup.`
	);
	if (!confirmation) {
		return;
	}

	setUpdateCardBusy(true, {
		uploadLabel: 'Stage local firmware',
		applyLabel: 'Applying update…',
		clearLabel: 'Clear staged image',
	});
	setAlert('Queueing the staged firmware image for reboot…', 'warn');

	try {
		const { response, data } = await requestJson(routes.updateApply, { method: 'POST' });

		if (response.status === 401) {
			showLoginView(SESSION_EXPIRED_MESSAGE, 'warn');
			return;
		}

		if (!response.ok || !data?.accepted) {
			throw new Error(data?.detail || `Apply staged update failed (${response.status})`);
		}

		state.updateSnapshot = {
			...(state.updateSnapshot || {}),
			state: 'apply-requested',
			applyReady: false,
			pendingWarning: data.detail || 'The staged firmware image has been queued for reboot.',
			lastResult: {
				...(state.updateSnapshot?.lastResult || {}),
				recorded: true,
				code: 'apply-requested',
				detail: data.detail || 'The staged firmware image has been queued for reboot.',
			},
		};
		setUpdateFeedback(data.detail || 'Staged firmware apply requested.', 'warn');
		setAlert(data.detail || 'Staged firmware apply requested.', 'warn');
	} catch (error) {
		const message = error instanceof Error ? error.message : 'Apply staged update failed.';
		setUpdateFeedback(message, 'error');
		setAlert(message, 'error');
	} finally {
		setUpdateCardBusy(false);
		renderUpdateSurface(state.updateSnapshot);
	}
}

async function bootstrapSession() {
	const flash = consumeFlashMessage();
	const defaultLoginMessage = 'Sign in to continue.';
	const unavailableMessage = 'Panel unavailable.';

	if (flash?.message) {
		setAlert(flash.message, flash.tone || 'info');
	} else if (isLoginPage()) {
		setAlert('Checking session...', 'info');
	} else {
		clearAlert();
	}

	try {
		const { response, data } = await requestJson(routes.session, { method: 'GET' });
		if (response.ok && data?.authenticated) {
			state.sessionUsername = data.username || 'operator';
			if (isLoginPage()) {
				navigateTo(requestedNextPath());
				return;
			}

			showDashboardView();
			renderCurrentPage();
			void refreshDashboard({ silent: true });
			return;
		}
		showLoginView(flash?.message || defaultLoginMessage, flash?.tone || 'info');
	} catch (error) {
		showLoginView(error instanceof Error ? error.message : (flash?.message || unavailableMessage), flash?.tone || 'warn');
	}
}

async function handleLogin(event) {
	event.preventDefault();
	const username = elements.loginUsername?.value?.trim() || '';
	const password = elements.loginPassword?.value || '';

	if (!username || !password) {
			setAlert('Enter username and password.', 'warn');
		return;
	}

	setBusy(elements.loginSubmit, true, 'Authenticating…');
	setAlert('Signing in...', 'info');

	try {
		const { response, data } = await requestJson(routes.login, {
			method: 'POST',
			body: JSON.stringify({ username, password }),
		});

		if (response.ok && data?.authenticated) {
			state.sessionUsername = username;
			elements.loginForm?.reset();
			setBusy(elements.loginSubmit, true, 'Redirecting…');
				setAlert('Signed in.', 'success');
			navigateTo(requestedNextPath());
			return;
		}

		if (response.status === 429) {
			const retryAfterMs = data?.retryAfterMs || 0;
			setAlert(`Too many wrong-password attempts. Wait ${Math.ceil(retryAfterMs / 1000)} seconds and try again.`, 'warn');
			return;
		}

		setAlert('Invalid credentials.', 'error');
	} catch (error) {
		setAlert(error instanceof Error ? error.message : 'Sign-in failed.', 'error');
	} finally {
		setBusy(elements.loginSubmit, false, 'Sign in');
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
		setSchedulerFeedback(null);
		resetSchedulerForm();
			showLoginView('Signed out.', 'success');
	} catch (error) {
		setAlert('Sign-out failed.', 'warn');
			showLoginView('Session reset.', 'warn');
	} finally {
		setBusy(elements.logoutButton, false, 'Logout');
	}
}

function handleActionCardInput(event) {
	const form = event.target.closest('[data-action-form]');
	if (!form) {
		return;
	}

	syncActionFormFromDom(form);
	if (state.actionFeedback?.tone === 'error') {
		setActionFeedback(null);
	}
}

async function handleActionCardSubmit(event) {
	const form = event.target.closest('[data-action-form]');
	if (!form) {
		return;
	}

	event.preventDefault();
	syncActionFormFromDom(form);
	await handleActionFormSubmit();
}

function handleSchedulerCardInput(event) {
	const form = event.target.closest('[data-scheduler-form]');
	if (!form) {
		return;
	}

	syncSchedulerFormFromDom(form);
	if (state.schedulerFeedback?.tone === 'error') {
		setSchedulerFeedback(null);
	}
}

async function handleSchedulerCardClick(event) {
	const toggleButton = event.target.closest('[data-scheduler-toggle]');
	if (toggleButton) {
		await handleSchedulerToggle(toggleButton.dataset.schedulerToggle || '', toggleButton.dataset.enabled === 'true');
		return;
	}

	const deleteButton = event.target.closest('[data-scheduler-delete]');
	if (deleteButton) {
		await handleSchedulerDelete(deleteButton.dataset.schedulerDelete || '');
		return;
	}
}

async function handleSchedulerCardSubmit(event) {
	const form = event.target.closest('[data-scheduler-form]');
	if (!form) {
		return;
	}

	event.preventDefault();
	syncSchedulerFormFromDom(form);
	await handleSchedulerFormSubmit();
}

async function handleUpdateCardClick(event) {
	if (event.target.closest('[data-update-upload]')) {
		await handleUpdateUpload();
		return;
	}

	if (event.target.closest('[data-update-now]')) {
		await handleUpdateNow();
		return;
	}

	if (event.target.closest('[data-update-apply]')) {
		await handleUpdateApply();
		return;
	}

	if (event.target.closest('[data-update-clear]')) {
		await handleUpdateClear();
	}
}

async function handleRelayCardClick(event) {
	const relayButton = event.target.closest('[data-relay-set]');
	if (!relayButton) {
		return;
	}

	await handleRelaySet(relayButton.dataset.relaySet === 'true');
}

function attachEvents() {
	elements.loginForm?.addEventListener('submit', handleLogin);
	elements.refreshButton?.addEventListener('click', () => { void refreshDashboard({ silent: false }); });
	elements.logoutButton?.addEventListener('click', handleLogout);
	elements.cards.relay?.addEventListener('input', handleActionCardInput);
	elements.cards.relay?.addEventListener('change', handleActionCardInput);
	elements.cards.relay?.addEventListener('submit', (event) => { void handleActionCardSubmit(event); });
	elements.cards.relay?.addEventListener('click', (event) => { void handleRelayCardClick(event); });
	elements.cards.scheduler?.addEventListener('input', handleSchedulerCardInput);
	elements.cards.scheduler?.addEventListener('change', handleSchedulerCardInput);
	elements.cards.scheduler?.addEventListener('click', (event) => { void handleSchedulerCardClick(event); });
	elements.cards.scheduler?.addEventListener('submit', (event) => { void handleSchedulerCardSubmit(event); });
	elements.cards.update?.addEventListener('change', (event) => {
		if (event.target.closest('[data-update-file]')) {
			handleUpdateFileChange();
		}
	});
	elements.cards.update?.addEventListener('click', (event) => { void handleUpdateCardClick(event); });
	window.addEventListener('pagehide', () => {
		if (!state.pageAbortController.signal.aborted) {
			state.pageAbortController.abort(new DOMException('Page is unloading.', 'AbortError'));
		}
	}, { once: true });
}

function startPolling() {
	if (state.refreshTimer) {
		clearInterval(state.refreshTimer);
	}

	state.refreshTimer = window.setInterval(() => {
		if (document.visibilityState === 'hidden') {
			return;
		}

		if (state.authenticated && !state.actionsBusy && !state.schedulerBusy && !state.relayCommandPending &&
			!state.updateBusy && !updateInputHasSelectedFile()) {
			void refreshDashboard({ silent: true });
		}
	}, 15000);
}

updateNavigationState();
attachEvents();
startPolling();
bootstrapSession();
