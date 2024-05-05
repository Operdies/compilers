<script>
	import GrammarEditor from '$lib/GrammarEditor.svelte';
	import LogPanel from '$lib/LogPanel.svelte';
	import { load } from '$lib/bindings.js';

	/** @type {Promise<import("$lib/bindings").bindings>} */
	let bindings = load();
</script>

<div class="container">
	{#await bindings}
		<p>Loading bindings..</p>
	{:then b}
		<div class="bottom">
			<LogPanel />
		</div>
		<GrammarEditor bindings={b} />
	{:catch err}
		<p>Error loading bindings: {err}</p>
	{/await}
</div>

<style>
	.container {
		height: calc(100vh - 320px);
		margin: 5px auto;
		padding: 5px;
		padding-left: 2px;
	}
	.bottom {
		position: fixed;
		left: 10px;
		bottom: 0px;
		width: calc(100% - 30px);
		text-align: center;
	}
</style>
