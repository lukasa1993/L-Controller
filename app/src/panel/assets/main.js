const shellStatus = document.getElementById('panel-shell-status');

const checkpoints = [
	'HTTP service online for the public shell.',
	'Auth routes and protected APIs are intentionally deferred.',
	'Future dashboard views will hydrate from local device state.',
];

function formatTimestamp(date) {
	return new Intl.DateTimeFormat(undefined, {
		dateStyle: 'medium',
		timeStyle: 'medium',
	}).format(date);
}

function renderShellReady() {
	if (!shellStatus) {
		return;
	}

	const items = checkpoints.map((checkpoint) => `<li>${checkpoint}</li>`).join('');

	shellStatus.innerHTML = `
		<div class="space-y-3">
			<p class="font-semibold text-white">Panel shell ready</p>
			<p>Loaded from embedded firmware assets at ${formatTimestamp(new Date())}.</p>
			<ul class="list-disc space-y-2 pl-5 text-slate-200">${items}</ul>
		</div>
	`;
}

renderShellReady();
