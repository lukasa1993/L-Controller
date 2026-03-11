const { test, expect } = require('@playwright/test');

const { resolvePanelBaseUrl, resolvePanelCredentials } = require('./helpers/panel-target');

function escapeRegex(value) {
	return value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

test('redirects through /login and reaches the dedicated panel routes', async ({ page }) => {
	const baseUrl = await resolvePanelBaseUrl(process.env);
	const credentials = resolvePanelCredentials(baseUrl, process.env);
	const loginUrlPattern = new RegExp(`^${escapeRegex(`${baseUrl}/login`)}(?:\\?.*)?$`);
	const actionsUrlPattern = new RegExp(`^${escapeRegex(baseUrl)}/?(?:\\?.*)?$`);
	const actionEditorUrlPattern = new RegExp(`^${escapeRegex(`${baseUrl}/actions/new`)}(?:\\?.*)?$`);
	const schedulesUrlPattern = new RegExp(`^${escapeRegex(`${baseUrl}/schedules`)}(?:\\?.*)?$`);
	const scheduleEditorUrlPattern = new RegExp(`^${escapeRegex(`${baseUrl}/schedules/new`)}(?:\\?.*)?$`);
	const updatesUrlPattern = new RegExp(`^${escapeRegex(`${baseUrl}/updates`)}(?:\\?.*)?$`);

	await page.goto(`${baseUrl}/`, { waitUntil: 'domcontentloaded' });

	await expect(page).toHaveURL(loginUrlPattern);
	await expect(page.locator('#login-form')).toBeVisible();
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
	const actionsResponsePromise = page.waitForResponse(
		(response) => response.url() === `${baseUrl}/api/actions` && response.request().method() === 'GET',
	);
	const updateResponsePromise = page.waitForResponse(
		(response) => response.url() === `${baseUrl}/api/update` && response.request().method() === 'GET',
	);
	const schedulesResponsePromise = page.waitForResponse(
		(response) => response.url() === `${baseUrl}/api/schedules` && response.request().method() === 'GET',
	);

	await page.getByRole('button', { name: 'Sign in' }).click();

	const [loginResponse, statusResponse, actionsResponse, updateResponse, schedulesResponse] = await Promise.all([
		loginResponsePromise,
		statusResponsePromise,
		actionsResponsePromise,
		updateResponsePromise,
		schedulesResponsePromise,
	]);

	await expect.poll(() => loginResponse.status(), { message: 'Expected login to succeed.' }).toBe(200);
	await expect.poll(() => statusResponse.status(), { message: 'Expected authenticated status refresh to succeed.' }).toBe(200);
	await expect.poll(() => actionsResponse.status(), { message: 'Expected authenticated actions refresh to succeed.' }).toBe(200);
	await expect.poll(() => updateResponse.status(), { message: 'Expected authenticated update refresh to succeed.' }).toBe(200);
	await expect.poll(() => schedulesResponse.status(), { message: 'Expected authenticated schedule refresh to succeed.' }).toBe(200);

	await expect(page).toHaveURL(actionsUrlPattern);
	await expect(page.locator('body')).toHaveAttribute('data-panel-page', 'actions');
	await expect(page.locator('#session-chip')).toContainText(`Authenticated as ${credentials.username}`);
	await expect(page.locator('#panel-alert')).toContainText('Protected status, actions, schedules, and OTA truth refreshed from the device.');
	await expect(page.locator('[data-panel-nav="actions"][aria-current="page"]').first()).toBeVisible();
	await expect(page.getByRole('heading', { name: 'Configured actions' })).toBeVisible();
	await expect(page.getByRole('link', { name: 'New action' })).toBeVisible();

	await page.getByRole('link', { name: 'New action' }).click();
	await expect(page).toHaveURL(actionEditorUrlPattern);
	await expect(page.locator('body')).toHaveAttribute('data-panel-page', 'action-editor');
	await expect(page.getByRole('heading', { name: 'Create action' })).toBeVisible();

	await page.getByRole('link', { name: 'Schedules' }).click();
	await expect(page).toHaveURL(schedulesUrlPattern);
	await expect(page.locator('body')).toHaveAttribute('data-panel-page', 'schedules');
	await expect(page.locator('[data-panel-nav="schedules"][aria-current="page"]').first()).toBeVisible();
	await expect(page.getByRole('heading', { name: 'Schedule management' })).toBeVisible();

	await page.getByRole('link', { name: 'New schedule' }).click();
	await expect(page).toHaveURL(scheduleEditorUrlPattern);
	await expect(page.locator('body')).toHaveAttribute('data-panel-page', 'schedule-editor');
	await expect(page.getByRole('heading', { name: 'Create schedule' })).toBeVisible();

	await page.getByRole('link', { name: 'Updates' }).click();
	await expect(page).toHaveURL(updatesUrlPattern);
	await expect(page.locator('body')).toHaveAttribute('data-panel-page', 'updates');
	await expect(page.locator('[data-panel-nav="updates"][aria-current="page"]').first()).toBeVisible();
	await expect(page.getByRole('heading', { name: 'Firmware updates' })).toBeVisible();

	await page.getByRole('link', { name: 'Actions' }).click();
	await expect(page).toHaveURL(actionsUrlPattern);
	await expect(page.locator('body')).toHaveAttribute('data-panel-page', 'actions');
	await expect(page.getByRole('heading', { name: 'Configured actions' })).toBeVisible();
});
