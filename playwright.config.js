const { defineConfig, devices } = require('@playwright/test');

module.exports = defineConfig({
	testDir: './tests',
	timeout: 45_000,
	expect: {
		timeout: 15_000,
	},
	fullyParallel: false,
	reporter: [['list']],
	use: {
		...devices['Desktop Chrome'],
		headless: true,
		trace: 'on-first-retry',
	},
	projects: [
		{
			name: 'chromium',
			use: {
				...devices['Desktop Chrome'],
			},
		},
	],
});
