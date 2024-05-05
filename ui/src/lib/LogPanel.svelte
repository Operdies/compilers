<script>
	import { on_write } from '$lib/bindings.js';
	let logs = '';

	const headers = ['[DEBUG]', '[INFO ]', '[WARN ]', '[ERROR]'];
	const logarr = ['', '', '', ''];
	let previousLine = 0;

	on_write((fd, data) => {
		logs += data;
		if (data.startsWith('[')) {
			for (let i = 0; i < headers.length; i++) {
				if (data.startsWith(headers[i])) {
					previousLine = i;
				}
			}
		}
		logarr[previousLine] += data;
	});
</script>

<div class="horizontal">
	{#each logs.split('\n') as line}
		<pre>{line}</pre>
	{/each}
</div>

<style lang="scss">
	.horizontal {
		border: 1px solid black;
		display: inline-block;
		text-align: left;
		width: 100%;
		line-height: 0.1em;
		flex-shrink: 1;
		overflow-y: scroll;
		height: 20%;
		margin: 5px auto;
		padding: 5px;
		max-height: 200px;
		min-height: 200px;
	}
</style>
