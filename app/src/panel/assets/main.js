const routes = {
	session: '/api/auth/session',
	login: '/api/auth/login',
	logout: '/api/auth/logout',
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
	refreshButton: document.getElementById('refresh-button'),
	logoutButton: document.getElementById('logout-button'),
};

function setAlert(message, tone = 'info') {
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
	elements.loginView.classList.remove('hidden');
	elements.dashboardView.classList.add('hidden');
	elements.sessionChip.textContent = 'Authentication required';
	if (message) {
		setAlert(message, tone);
	}
}

function showDashboardView(username) {
	elements.loginView.classList.add('hidden');
	elements.dashboardView.classList.remove('hidden');
	elements.sessionChip.textContent = `Authenticated as ${username || 'operator'}`;
	setAlert('Session is active. Refresh and logout stay tied to the server-issued cookie.', 'success');
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

async function bootstrapSession() {
	setAlert('Checking whether this browser already has a valid session…', 'info');
	try {
		const { response, data } = await requestJson(routes.session, { method: 'GET' });
		if (response.ok && data?.authenticated) {
			showDashboardView(data.username);
			return;
		}
		showLoginView('Log in with the configured local admin credentials to continue.', 'info');
	} catch (error) {
		showLoginView('The panel could not confirm session state yet. Try again once the device is reachable.', 'warn');
	}
}

async function handleLogin(event) {
	event.preventDefault();
	const username = elements.loginUsername.value.trim();
	const password = elements.loginPassword.value;
	if (!username || !password) {
		setAlert('Enter both the username and password before continuing.', 'warn');
		return;
	}
	setBusy(elements.loginSubmit, true, 'Authenticating…');
	try {
		const { response, data } = await requestJson(routes.login, {
			method: 'POST',
			body: JSON.stringify({ username, password }),
		});
		if (response.ok && data?.authenticated) {
			elements.loginForm.reset();
			showDashboardView(username);
			return;
		}
		if (response.status === 429) {
			setAlert(`Too many wrong-password attempts. Wait ${Math.ceil((data?.retryAfterMs || 0) / 1000)} seconds and try again.`, 'warn');
			return;
		}
		setAlert('Authentication failed. Confirm the local admin credentials and try again.', 'error');
	} catch (error) {
		setAlert('Authentication failed because the device did not respond cleanly.', 'error');
	} finally {
		setBusy(elements.loginSubmit, false, 'Authenticate locally');
	}
}

async function handleLogout() {
	setBusy(elements.logoutButton, true, 'Logging out…');
	try {
		await fetch(routes.logout, { method: 'POST', credentials: 'same-origin' });
		showLoginView('Session cleared. Log in again to continue.', 'success');
	} catch (error) {
		showLoginView('The logout request failed, but the session may already be invalid.', 'warn');
	} finally {
		setBusy(elements.logoutButton, false, 'Logout');
	}
}

elements.loginForm?.addEventListener('submit', handleLogin);
	elements.refreshButton?.addEventListener('click', bootstrapSession);
	elements.logoutButton?.addEventListener('click', handleLogout);

bootstrapSession();
