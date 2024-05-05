<script>
	/** @type {import("$lib/bindings").bindings} */
	export let bindings;
	let text = '';
</script>

<div>
	<h2>Grammar</h2>
	<textarea placeholder="<create grammar>" value={text}></textarea>
</div>

<style>
	div {
		display: inline-block;
		height: 100%;
	}
	textarea {
		width: 600px;
		height: 100%;
		overflow: scroll;
		resize: horizontal;
	}
</style>
