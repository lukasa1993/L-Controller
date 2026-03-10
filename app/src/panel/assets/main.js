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

const state = {
	authenticated: false,
	sessionUsername: '',
	status: null,
	scheduleSnapshot: null,
	updateSnapshot: null,
	refreshTimer: null,
	relayCommandPending: false,
	relayCommandDesiredState: false,
	relayFeedback: null,
	schedulerBusy: false,
	schedulerFeedback: null,
	updateBusy: false,
	updateFeedback: null,
	schedulerForm: createSchedulerFormState(),
};

const routes = {
	session: '/api/auth/session',
	login: '/api/auth/login',
	logout: '/api/auth/logout',
	status: '/api/status',
	updateStatus: '/api/update',
	updateUpload: '/api/update/upload',
	updateApply: '/api/update/apply',
	updateClear: '/api/update/clear',
	relayDesiredState: '/api/relay/desired-state',
	schedules: '/api/schedules',
	scheduleCreate: '/api/schedules/create',
	scheduleUpdate: '/api/schedules/update',
	scheduleDelete: '/api/schedules/delete',
	scheduleSetEnabled: '/api/schedules/set-enabled',
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
		return 'badge--ok';
	case 'degraded':
		return 'badge--warn';
	default:
		return 'badge--danger';
	}
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

function resetSchedulerForm(overrides = {}) {
	state.schedulerForm = createSchedulerFormState(overrides);
}

function showLoginView(message, tone = 'info') {
	state.authenticated = false;
	state.status = null;
	state.scheduleSnapshot = null;
	state.updateSnapshot = null;
	state.relayCommandPending = false;
	state.relayCommandDesiredState = false;
	state.schedulerBusy = false;
	state.updateBusy = false;
	setRelayFeedback(null);
	setSchedulerFeedback(null);
	setUpdateFeedback(null);
	resetSchedulerForm();
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
	const applyButton = elements.cards.update?.querySelector('[data-update-apply]');
	const clearButton = elements.cards.update?.querySelector('[data-update-clear]');

	state.updateBusy = busy;
	setBusy(uploadButton, busy, labels.uploadLabel || 'Stage local firmware');
	setBusy(applyButton, busy, labels.applyLabel || 'Apply staged update');
	setBusy(clearButton, busy, labels.clearLabel || 'Clear staged image');
}

function updateFeedbackMarkup() {
	if (!state.updateFeedback) {
		return '';
	}

	return `<div class="update-feedback" data-tone="${escapeHtml(state.updateFeedback.tone)}">${escapeHtml(state.updateFeedback.message)}</div>`;
}

function renderUpdateSurface(snapshot) {
	if (!elements.cards.update) {
		return;
	}

	if (!snapshot) {
		elements.cards.update.innerHTML = renderCard('Firmware updates', 'Phase 8 OTA surface', [
			{ label: 'State', value: 'Loading' },
		], '<div class="placeholder-copy">Authenticated update routes will populate this surface once the device responds.</div>');
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
	const applyReady = Boolean(snapshot.applyReady);
	const clearAvailable = snapshot.state && snapshot.state !== 'idle';
	const rollbackDetected = Boolean(snapshot.lastResult?.rollbackDetected);
	const badges = [
		`<span class="badge ${applyReady ? 'badge--warn' : ''}">State ${escapeHtml(stateLabel)}</span>`,
		`<span class="badge ${snapshot.imageConfirmed ? 'badge--ok' : 'badge--warn'}">Image ${snapshot.imageConfirmed ? 'Confirmed' : 'Unconfirmed'}</span>`,
		`<span class="badge ${applyReady ? 'badge--warn' : 'badge--ok'}">Apply ${applyReady ? 'Ready' : 'Not ready'}</span>`,
		rollbackDetected ? '<span class="badge badge--warn">Rollback flagged</span>' : '',
	].filter(Boolean).join('');

	elements.cards.update.innerHTML = `
		<p class="card-eyebrow">Phase 8 OTA surface</p>
		<div class="update-shell">
			<div>
				<h3>Firmware updates</h3>
				<p class="muted">Local uploads stream straight into the shared OTA pipeline, stage first, and only reboot after one explicit operator apply.</p>
			</div>
			<div class="relay-badge-row">${badges}</div>
			<div class="update-warning" data-tone="${applyReady || snapshot.state === 'apply-requested' ? 'warn' : 'info'}">
				<strong>${escapeHtml(snapshot.pendingWarning || 'No staged firmware image is waiting.')}</strong>
				<div class="update-note">${escapeHtml(snapshot.sessionWarning || 'Applying an update clears the current browser session after reboot.')}</div>
			</div>
			<div class="update-summary-grid">
				<div class="update-summary-card">
					<span class="card-eyebrow">Current version</span>
					<strong>${escapeHtml(currentVersion)}</strong>
					<small>${snapshot.currentVersion?.available ? 'Running image from device truth' : 'Running image metadata unavailable'}</small>
				</div>
				<div class="update-summary-card">
					<span class="card-eyebrow">Staged version</span>
					<strong>${escapeHtml(stagedVersion)}</strong>
					<small>${snapshot.stagedVersion?.available ? 'Ready to apply after confirmation' : 'No staged image eligible for apply'}</small>
				</div>
				<div class="update-summary-card">
					<span class="card-eyebrow">Last result</span>
					<strong>${escapeHtml(lastResultCode)}</strong>
					<small>${escapeHtml(lastResultDetail)}</small>
				</div>
				<div class="update-summary-card">
					<span class="card-eyebrow">Staging bytes</span>
					<strong>${escapeHtml(String(snapshot.lastResult?.bytesWritten || 0))}</strong>
					<small>${rollbackDetected ? `Rollback reason ${snapshot.lastResult?.rollbackReason || 0}` : 'Latest OTA attempt byte count'}</small>
				</div>
			</div>
			${updateFeedbackMarkup()}
			<div class="update-layout">
				<div class="update-panel">
					<p class="card-eyebrow">Stage local firmware</p>
					<h3>Upload a newer signed image</h3>
					<p class="update-note">The device rejects same-version reinstall and downgrade attempts before the staged image becomes apply-ready.</p>
					<div class="update-upload-actions">
						<div class="field">
							<label for="update-file">Firmware image</label>
							<input id="update-file" class="input" data-update-file type="file" accept=".bin,.hex,.img,application/octet-stream" ${state.updateBusy || snapshot.state === 'apply-requested' ? 'disabled' : ''}>
						</div>
						<button class="button" type="button" data-update-upload ${state.updateBusy || snapshot.state === 'apply-requested' ? 'disabled' : ''}>Stage local firmware</button>
					</div>
					<div class="update-file-meta" data-update-file-meta>Choose a signed firmware image from this computer. Same-version and downgrade uploads are rejected.</div>
				</div>
				<div class="update-panel">
					<p class="card-eyebrow">Apply or clear</p>
					<h3>Explicit reboot boundary</h3>
					<p class="update-note">Applying the staged update reboots the device, drops the panel connection, and requires a fresh login once startup completes.</p>
					<div class="button-row">
						<button class="button" type="button" data-update-apply ${state.updateBusy || !applyReady ? 'disabled' : ''}>Apply staged update</button>
						<button class="button button--ghost" type="button" data-update-clear ${state.updateBusy || !clearAvailable ? 'disabled' : ''}>Clear staged image</button>
					</div>
					<div class="update-note">The rest of the dashboard remains usable while a staged image waits for explicit apply.</div>
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
		<div class="scheduler-summary-card">
			<span class="card-eyebrow">${escapeHtml(title)}</span>
			<strong>${escapeHtml(value)}</strong>
			<small>${escapeHtml(detail)}</small>
		</div>
	`;
}

function schedulerFeedbackMarkup() {
	if (!state.schedulerFeedback) {
		return '';
	}

	return `<div class="scheduler-feedback" data-tone="${escapeHtml(state.schedulerFeedback.tone)}">${escapeHtml(state.schedulerFeedback.message)}</div>`;
}

function renderSchedulerRows(snapshot) {
	if (!snapshot.schedules?.length) {
		return '<div class="scheduler-empty placeholder-copy">No saved schedules yet. Create one below to start the scheduler flow.</div>';
	}

	return snapshot.schedules.map((schedule) => `
		<div class="scheduler-row">
			<div class="scheduler-row-copy">
				<div class="scheduler-row-title">
					<strong>${escapeHtml(schedule.scheduleId)}</strong>
					<span class="badge ${schedule.enabled ? 'badge--ok' : 'badge--warn'}">${schedule.enabled ? 'Enabled' : 'Disabled'}</span>
					${schedule.isNextRun ? '<span class="badge badge--ok">Next run</span>' : ''}
				</div>
				<div class="scheduler-row-meta">
					<span>${escapeHtml(schedule.actionLabel)}</span>
					<code>${escapeHtml(schedule.cronExpression)}</code>
				</div>
			</div>
			<div class="scheduler-row-actions">
				<button class="button button--ghost" type="button" data-scheduler-edit="${escapeHtml(schedule.scheduleId)}" ${state.schedulerBusy ? 'disabled' : ''}>Edit</button>
				<button class="button button--ghost" type="button" data-scheduler-toggle="${escapeHtml(schedule.scheduleId)}" data-enabled="${schedule.enabled ? 'false' : 'true'}" ${state.schedulerBusy ? 'disabled' : ''}>${schedule.enabled ? 'Disable' : 'Enable'}</button>
				<button class="button button--ghost" type="button" data-scheduler-delete="${escapeHtml(schedule.scheduleId)}" ${state.schedulerBusy ? 'disabled' : ''}>Delete</button>
			</div>
		</div>
	`).join('');
}

function renderSchedulerProblems(snapshot) {
	if (!snapshot.problems?.length) {
		return '<div class="placeholder-copy">No recent scheduler problems are recorded right now.</div>';
	}

	return `
		<ul class="chrome-list scheduler-problems">
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

	return `
		<div class="scheduler-panel">
			<p class="card-eyebrow">${escapeHtml(formState.mode === 'edit' ? 'Edit schedule' : 'Create schedule')}</p>
			<h3>${escapeHtml(heading)}</h3>
			<form class="scheduler-form-grid" data-scheduler-form>
				<div class="field">
					<label for="schedule-id">Schedule ID</label>
					<input id="schedule-id" class="input" name="scheduleId" value="${escapeHtml(formState.scheduleId)}" ${formState.mode === 'edit' ? 'readonly' : ''} required>
				</div>
				<div class="field">
					<label for="schedule-action">Action</label>
					<select id="schedule-action" class="input" name="actionKey">
						${actionChoices.map((choice) => `<option value="${escapeHtml(choice.key)}" ${choice.key === formState.actionKey ? 'selected' : ''}>${escapeHtml(choice.label)}</option>`).join('')}
					</select>
				</div>
				<div class="scheduler-field-grid">
					<div class="field">
						<label for="cron-minute">Minute</label>
						<input id="cron-minute" class="input" name="minute" value="${escapeHtml(formState.minute)}" placeholder="0" required>
					</div>
					<div class="field">
						<label for="cron-hour">Hour</label>
						<input id="cron-hour" class="input" name="hour" value="${escapeHtml(formState.hour)}" placeholder="*" required>
					</div>
					<div class="field">
						<label for="cron-dom">Day of month</label>
						<input id="cron-dom" class="input" name="dayOfMonth" value="${escapeHtml(formState.dayOfMonth)}" placeholder="*" required>
					</div>
					<div class="field">
						<label for="cron-month">Month</label>
						<input id="cron-month" class="input" name="month" value="${escapeHtml(formState.month)}" placeholder="*" required>
					</div>
					<div class="field">
						<label for="cron-dow">Day of week</label>
						<input id="cron-dow" class="input" name="dayOfWeek" value="${escapeHtml(formState.dayOfWeek)}" placeholder="*" required>
					</div>
				</div>
				<label class="scheduler-toggle">
					<input type="checkbox" name="enabled" ${formState.enabled ? 'checked' : ''}>
					<span>Enabled immediately after save</span>
				</label>
				<div class="scheduler-help">
					All schedule fields are explicit <code>UTC</code>. Preview: <code>${escapeHtml(cronPreview)}</code>
				</div>
				<div class="button-row">
					<button class="button" type="submit" ${state.schedulerBusy ? 'disabled' : ''}>${escapeHtml(primaryLabel)}</button>
					${formState.mode === 'edit' ? `<button class="button button--ghost" type="button" data-scheduler-cancel ${state.schedulerBusy ? 'disabled' : ''}>Cancel edit</button>` : ''}
				</div>
			</form>
		</div>
	`;
}

function renderSchedulerSurface(snapshot) {
	if (!elements.cards.scheduler) {
		return;
	}

	if (!snapshot) {
		elements.cards.scheduler.innerHTML = renderCard('Schedule management', 'Phase 7 live surface', [
			{ label: 'State', value: 'Loading' },
		], '<div class="placeholder-copy">Authenticated schedule routes will populate this surface once the device responds.</div>');
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

	elements.cards.scheduler.innerHTML = `
		<p class="card-eyebrow">Phase 7 live surface</p>
		<div class="scheduler-shell">
			<div>
				<h3>Schedule management</h3>
				<p class="muted">Create, edit, enable, disable, and delete UTC cron schedules without exposing internal action wiring. Manual relay control semantics stay unchanged.</p>
			</div>
			<div class="scheduler-header">
				<div class="scheduler-panel">
					<p class="card-eyebrow">Clock and automation</p>
					<div class="scheduler-badge-row">
						<span class="badge ${escapeHtml(schedulerClockTone(snapshot.clockState))}">Clock ${escapeHtml(humanizeHyphenated(snapshot.clockState))}</span>
						<span class="badge ${snapshot.automationActive ? 'badge--ok' : 'badge--warn'}">Automation ${snapshot.automationActive ? 'Active' : 'Inactive'}</span>
						<span class="badge">UTC only</span>
					</div>
					<div class="scheduler-help">
						${escapeHtml(clockCopy)}${snapshot.degradedReason && snapshot.degradedReason !== 'none' ? ` · ${escapeHtml(humanizeHyphenated(snapshot.degradedReason))}` : ''}
					</div>
				</div>
				<div class="scheduler-panel">
					<p class="card-eyebrow">Scheduler history</p>
					<div class="scheduler-help">${escapeHtml(historyCopy)}</div>
					<div class="scheduler-help">${escapeHtml(automationCopy)}</div>
				</div>
			</div>
			<div class="scheduler-summary-grid">
				${schedulerSummaryCard('Next run', nextRunCopy, snapshot.nextRun?.available ? `${snapshot.nextRun.scheduleId} · ${snapshot.nextRun.actionKey}` : 'Trusted time is required before a next run can appear')}
				${schedulerSummaryCard('Last result', lastResultCopy, snapshot.lastResult?.available ? `${snapshot.lastResult.scheduleId || 'scheduler'} · ${snapshot.lastResult.actionLabel}` : 'A due schedule minute must run before last result appears')}
				${schedulerSummaryCard('Clock', clockCopy, snapshot.degradedReason && snapshot.degradedReason !== 'none' ? humanizeHyphenated(snapshot.degradedReason) : 'Scheduler uses future-only UTC baselines')}
				${schedulerSummaryCard('Schedule counts', `${snapshot.enabledCount}/${snapshot.scheduleCount} enabled`, `Up to ${snapshot.maxSchedules} schedules in this phase`) }
			</div>
			${schedulerFeedbackMarkup()}
			<div class="scheduler-layout">
				<div class="scheduler-panel">
					<p class="card-eyebrow">Saved schedules</p>
					<div class="scheduler-help">Next run, create, edit, disable, and delete flows all refresh from live device truth after each mutation.</div>
					<div class="scheduler-list">${renderSchedulerRows(snapshot)}</div>
				</div>
				${renderSchedulerForm(snapshot)}
			</div>
			<div class="scheduler-panel">
				<p class="card-eyebrow">Recent problems and history</p>
				${renderSchedulerProblems(snapshot)}
			</div>
		</div>
	`;
}

function renderStatus(statusPayload, updatePayload = null) {
	state.status = statusPayload;
	state.updateSnapshot = updatePayload;
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
	renderUpdateSurface(updatePayload || statusPayload.update || null);
}

async function refreshDashboard({ silent = false } = {}) {
	try {
		const statusResult = await requestJson(routes.status, { method: 'GET' });
		if (statusResult.response.status === 401) {
			showLoginView('The device no longer trusts this browser session. Log in again.', 'warn');
			return false;
		}

		if (!statusResult.response.ok || !statusResult.data) {
			throw new Error(`Status refresh failed (${statusResult.response.status})`);
		}

		const updateResult = await requestJson(routes.updateStatus, { method: 'GET' });
		if (updateResult.response.status === 401) {
			showLoginView('The device no longer trusts this browser session. Log in again.', 'warn');
			return false;
		}

		if (!updateResult.response.ok || !updateResult.data) {
			throw new Error(`Update refresh failed (${updateResult.response.status})`);
		}

		const schedulesResult = await requestJson(routes.schedules, { method: 'GET' });
		if (schedulesResult.response.status === 401) {
			showLoginView('The device no longer trusts this browser session. Log in again.', 'warn');
			return false;
		}

		if (!schedulesResult.response.ok || !schedulesResult.data) {
			throw new Error(`Schedule refresh failed (${schedulesResult.response.status})`);
		}

		showDashboardView();
		state.scheduleSnapshot = schedulesResult.data;
		state.updateSnapshot = updateResult.data;
		renderStatus(statusResult.data, updateResult.data);
		renderSchedulerSurface(schedulesResult.data);
		setAlert('Protected status, schedules, and OTA truth refreshed from the device.', silent ? 'info' : 'success');
		return true;
	} catch (error) {
		if (state.updateSnapshot?.state === 'apply-requested') {
			setAlert('Device reboot is in progress for the staged update. Log in again once it returns.', 'warn');
			return false;
		}

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

		const refreshed = await refreshDashboard({ silent: true });
		if (!refreshed) {
			setRelayFeedback('Relay command was accepted, but live refresh did not complete yet.', 'warn');
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

function schedulerApiErrorMessage(data, fallbackPrefix) {
	const fieldPrefix = data?.field ? `${humanizeHyphenated(String(data.field).replace(/[A-Z]/g, (match) => `-${match.toLowerCase()}`))}: ` : '';
	return `${fallbackPrefix}${fieldPrefix}${data?.detail || data?.error || 'Unknown error.'}`;
}

async function runSchedulerMutation(route, payload) {
	state.schedulerBusy = true;
	setSchedulerFeedback('Refreshing scheduler truth after this change…', 'warn');
	renderSchedulerSurface(state.scheduleSnapshot);

	try {
		const { response, data } = await requestJson(route, {
			method: 'POST',
			body: JSON.stringify(payload),
		});

		if (response.status === 401) {
			showLoginView('The device no longer trusts this browser session. Log in again.', 'warn');
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
		renderSchedulerSurface(state.scheduleSnapshot);
	}
}

async function handleSchedulerFormSubmit() {
	const formState = state.schedulerForm;
	const payload = {
		scheduleId: formState.scheduleId.trim(),
		cronExpression: schedulerCronExpressionFromForm(formState),
		actionKey: formState.actionKey,
		enabled: formState.enabled,
	};

	if (!payload.scheduleId || !payload.actionKey) {
		setSchedulerFeedback('Enter a schedule ID and choose an action before saving.', 'warn');
		setAlert('Enter a schedule ID and choose an action before saving.', 'warn');
		renderSchedulerSurface(state.scheduleSnapshot);
		return;
	}

	const route = formState.mode === 'edit' ? routes.scheduleUpdate : routes.scheduleCreate;
	const ok = await runSchedulerMutation(route, payload);
	if (ok) {
		resetSchedulerForm({ actionKey: state.scheduleSnapshot?.actionChoices?.[0]?.key || 'relay-on' });
		renderSchedulerSurface(state.scheduleSnapshot);
	}
}

function loadSchedulerEdit(scheduleId) {
	const schedule = state.scheduleSnapshot?.schedules?.find((entry) => entry.scheduleId === scheduleId);
	if (!schedule) {
		setAlert('That schedule is no longer available to edit.', 'warn');
		return;
	}

	resetSchedulerForm({
		mode: 'edit',
		scheduleId: schedule.scheduleId,
		actionKey: schedule.actionKey,
		enabled: schedule.enabled,
		...splitCronExpression(schedule.cronExpression),
	});
	setSchedulerFeedback(`Editing ${schedule.scheduleId}. Save to keep the new cron or action values.`, 'info');
	renderSchedulerSurface(state.scheduleSnapshot);
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
		renderSchedulerSurface(state.scheduleSnapshot);
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
			showLoginView('The device no longer trusts this browser session. Log in again.', 'warn');
			return;
		}

		if (!response.ok || !data?.accepted) {
			throw new Error(data?.detail || `Firmware upload failed (${response.status})`);
		}

		setUpdateFeedback(data.detail || 'Firmware image staged.', 'success');
		setAlert(data.detail || 'Firmware image staged.', 'success');
		await refreshDashboard({ silent: true });
	} catch (error) {
		const message = error instanceof Error ? error.message : 'Firmware upload failed.';
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
			showLoginView('The device no longer trusts this browser session. Log in again.', 'warn');
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
			showLoginView('The device no longer trusts this browser session. Log in again.', 'warn');
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
	setAlert('Checking whether this browser already has a valid local session…', 'info');
	try {
		const { response, data } = await requestJson(routes.session, { method: 'GET' });
		if (response.ok && data?.authenticated) {
			state.sessionUsername = data.username || 'operator';
			await refreshDashboard({ silent: true });
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
			await refreshDashboard({ silent: false });
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
		setSchedulerFeedback(null);
		resetSchedulerForm();
		showLoginView('Session cleared. Log in again to view protected status.', 'success');
	} catch (error) {
		setAlert('Logout request failed, but the browser session may already be invalid.', 'warn');
		showLoginView('Session reset locally. Log in again if needed.', 'warn');
	} finally {
		setBusy(elements.logoutButton, false, 'Logout');
	}
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
	const editButton = event.target.closest('[data-scheduler-edit]');
	if (editButton) {
		loadSchedulerEdit(editButton.dataset.schedulerEdit || '');
		return;
	}

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

	if (event.target.closest('[data-scheduler-cancel]')) {
		resetSchedulerForm({ actionKey: state.scheduleSnapshot?.actionChoices?.[0]?.key || 'relay-on' });
		setSchedulerFeedback(null);
		renderSchedulerSurface(state.scheduleSnapshot);
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

	if (event.target.closest('[data-update-apply]')) {
		await handleUpdateApply();
		return;
	}

	if (event.target.closest('[data-update-clear]')) {
		await handleUpdateClear();
	}
}

function attachEvents() {
	elements.loginForm?.addEventListener('submit', handleLogin);
	elements.refreshButton?.addEventListener('click', () => refreshDashboard({ silent: false }));
	elements.logoutButton?.addEventListener('click', handleLogout);
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
}

function startPolling() {
	if (state.refreshTimer) {
		clearInterval(state.refreshTimer);
	}

	state.refreshTimer = window.setInterval(() => {
		if (state.authenticated && !state.schedulerBusy && !state.relayCommandPending &&
			!state.updateBusy && !updateInputHasSelectedFile()) {
			refreshDashboard({ silent: true });
		}
	}, 15000);
}

attachEvents();
startPolling();
bootstrapSession();
