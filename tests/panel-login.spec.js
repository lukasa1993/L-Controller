const { test, expect } = require('@playwright/test');

const { resolvePanelBaseUrl, resolvePanelCredentials } = require('./helpers/panel-target');

function escapeRegex(value) {
	return value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

test('redirects through /login and lands on the authenticated dashboard', async ({ page }) => {
	const baseUrl = await resolvePanelBaseUrl(process.env);
	const credentials = resolvePanelCredentials(baseUrl, process.env);
	const loginUrlPattern = new RegExp(`^${escapeRegex(`${baseUrl}/login`)}(?:\\?.*)?$`);
	const dashboardUrlPattern = new RegExp(`^${escapeRegex(baseUrl)}/?(?:\\?.*)?$`);

	await page.goto(`${baseUrl}/`, { waitUntil: 'domcontentloaded' });

	await expect(page).toHaveURL(loginUrlPattern);
	await expect(page.locator('#login-view')).toBeVisible();
	await expect(page.getByLabel('Username')).toBeVisible();
	await expect(page.getByLabel('Password')).toBeVisible();

	await page.getByLabel('Username').fill(credentials.username);
	await page.getByLabel('Password').fill(credentials.password);

	const loginResponsePromise = page.waitForResponse(
		(response) => response.url() === `${baseUrl}/api/auth/login` && response.request().method() === 'POST',
	);
	const statusResponsePromise = page.waitForResponse(
		(response) => response.url() === `${baseUrl}/api/status` && response.request().method() === 'GET',
	);
	const updateResponsePromise = page.waitForResponse(
		(response) => response.url() === `${baseUrl}/api/update` && response.request().method() === 'GET',
	);
	const schedulesResponsePromise = page.waitForResponse(
		(response) => response.url() === `${baseUrl}/api/schedules` && response.request().method() === 'GET',
	);

	await page.getByRole('button', { name: 'Authenticate locally' }).click();

	const [loginResponse, statusResponse, updateResponse, schedulesResponse] = await Promise.all([
		loginResponsePromise,
		statusResponsePromise,
		updateResponsePromise,
		schedulesResponsePromise,
	]);

	await expect
		.poll(() => loginResponse.status(), { message: 'Expected login to succeed.' })
		.toBe(200);
	await expect
		.poll(() => statusResponse.status(), { message: 'Expected authenticated status refresh to succeed.' })
		.toBe(200);
	await expect
		.poll(() => updateResponse.status(), { message: 'Expected authenticated update refresh to succeed.' })
		.toBe(200);
	await expect
		.poll(() => schedulesResponse.status(), { message: 'Expected authenticated schedule refresh to succeed.' })
		.toBe(200);

	await expect(page).toHaveURL(dashboardUrlPattern);
	await expect(page.locator('#login-view')).toHaveCount(0);
	await expect(page.locator('#dashboard-view')).not.toHaveClass(/hidden/);
	await expect(page.locator('#session-chip')).toContainText(`Authenticated as ${credentials.username}`);
	await expect(page.locator('#panel-alert')).toContainText('Protected status, schedules, and OTA truth refreshed from the device.');
	await expect(page.getByRole('heading', { name: 'Device shell' })).toBeVisible();
	await expect(page.getByRole('heading', { name: 'Connectivity' })).toBeVisible();
	await expect(page.getByRole('button', { name: 'Refresh status' })).toBeEnabled();
});
