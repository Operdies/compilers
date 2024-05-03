/** @type {WebAssembly.WebAssemblyInstantiatedSource} */
let lib = null;
/** @type {WebAssembly.Exports} */
let functions = null;
/** @type {WebAssembly.Instance} */
let instance = null;

const memory = new WebAssembly.Memory({ initial: 10, maximum: 100 });
const stdin = 0;
const stdout = 1;
const stderr = 2;

function out(stream, data) {
  if (stream === stdout) {
    console.log(data)
  }

  if (stream === stderr) {
    console.error(data);
  }
}

function fd_write(fd, iovs, iovs_len, ret_ptr) {
  const view = new Uint32Array(memory.buffer);

  let nwritten = 0;
  for (let i = 0; i < iovs_len; i++) {
    const offset = i * 8; // = jump over 2 i32 values per iteration
    const iov = new Uint32Array(view.buffer, iovs + offset, 2);
    // use the iovs to read the data from the memory
    const bytes = new Uint8Array(view.buffer, iov[0], iov[1]);
    const data = new TextDecoder("utf8").decode(bytes);
    out(fd, data);
    nwritten += iov[1];
  }

  // Set the nwritten in ret_ptr
  const bytes_written = new Uint32Array(view.buffer, ret_ptr, 1);
  bytes_written[0] = nwritten;
  out(fd, `bytes written: ${view[ret_ptr]}`)

  return 0;
}
const importObject = {
  env: { memory },
  wasi_snapshot_preview1: {
    args_get: (...args) => { console.log(`args_get(${args})`); return 0; },
    args_sizes_get: (...args) => { console.log(`args_sizes_get(${args})`); return 0; },
    environ_get: (...args) => { console.log(`environ_get(${args})`); return 0; },
    environ_sizes_get: (...args) => { console.log(`environ_sizes_get(${args})`); return 0; },
    clock_res_get: (...args) => { console.log(`clock_res_get(${args})`); return 0; },
    clock_time_get: (...args) => { console.log(`clock_time_get(${args})`); return 0; },
    fd_advise: (...args) => { console.log(`fd_advise(${args})`); return 0; },
    fd_allocate: (...args) => { console.log(`fd_allocate(${args})`); return 0; },
    fd_close: (...args) => { console.log(`fd_close(${args})`); return 0; },
    fd_datasync: (...args) => { console.log(`fd_datasync(${args})`); return 0; },
    fd_fdstat_get: (...args) => { console.log(`fd_fdstat_get(${args})`); return 0; },
    fd_fdstat_set_flags: (...args) => { console.log(`fd_fdstat_set_flags(${args})`); return 0; },
    fd_fdstat_set_rights: (...args) => { console.log(`fd_fdstat_set_rights(${args})`); return 0; },
    fd_filestat_get: (...args) => { console.log(`fd_filestat_get(${args})`); return 0; },
    fd_filestat_set_size: (...args) => { console.log(`fd_filestat_set_size(${args})`); return 0; },
    fd_filestat_set_times: (...args) => { console.log(`fd_filestat_set_times(${args})`); return 0; },
    fd_pread: (...args) => { console.log(`fd_pread(${args})`); return 0; },
    fd_prestat_get: (...args) => { console.log(`fd_prestat_get(${args})`); return 0; },
    fd_prestat_dir_name: (...args) => { console.log(`fd_prestat_dir_name(${args})`); return 0; },
    fd_pwrite: (...args) => { console.log(`fd_pwrite(${args})`); return 0; },
    fd_read: (...args) => { console.log(`fd_read(${args})`); return 0; },
    fd_readdir: (...args) => { console.log(`fd_readdir(${args})`); return 0; },
    fd_renumber: (...args) => { console.log(`fd_renumber(${args})`); return 0; },
    fd_seek: (...args) => { console.log(`fd_seek(${args})`); return 0; },
    fd_sync: (...args) => { console.log(`fd_sync(${args})`); return 0; },
    fd_tell: (...args) => { console.log(`fd_tell(${args})`); return 0; },
    fd_write: fd_write,
    path_create_directory: (...args) => { console.log(`path_create_directory(${args})`); return 0; },
    path_filestat_get: (...args) => { console.log(`path_filestat_get(${args})`); return 0; },
    path_filestat_set_times: (...args) => { console.log(`path_filestat_set_times(${args})`); return 0; },
    path_link: (...args) => { console.log(`path_link(${args})`); return 0; },
    path_open: (...args) => { console.log(`path_open(${args})`); return 0; },
    path_readlink: (...args) => { console.log(`path_readlink(${args})`); return 0; },
    path_remove_directory: (...args) => { console.log(`path_remove_directory(${args})`); return 0; },
    path_rename: (...args) => { console.log(`path_rename(${args})`); return 0; },
    path_symlink: (...args) => { console.log(`path_symlink(${args})`); return 0; },
    path_unlink_file: (...args) => { console.log(`path_unlink_file(${args})`); return 0; },
    poll_oneoff: (...args) => { console.log(`poll_oneoff(${args})`); return 0; },
    proc_exit: (...args) => { console.log(`proc_exit(${args})`); return 0; },
    sched_yield: (...args) => { console.log(`sched_yield(${args})`); return 0; },
    random_get: (...args) => { console.log(`random_get(${args})`); return 0; },
    sock_accept: (...args) => { console.log(`sock_accept(${args})`); return 0; },
    sock_recv: (...args) => { console.log(`sock_recv(${args})`); return 0; },
    sock_send: (...args) => { console.log(`sock_send(${args})`); return 0; },
    sock_shutdown: (...args) => { console.log(`sock_shutdown(${args})`); return 0; },
  }
};

const lib_path = 'lib.wasm';
console.log("Load ", lib_path);
let program = fetch(lib_path, { cache: 'no-store' });
WebAssembly.instantiateStreaming(program, importObject).then(result => {
  instance = result.instance;
  lib = result;
  console.log(instance)
  functions = instance.exports;
  json_format();

}).catch(ex => {
  console.log("Failed.")
  console.log(ex)
})


// Encode string into memory starting at address base.
const encode = (memory, base, string) => {
  for (let i = 0; i < string.length; i++) {
    memory[base + i] = string.charCodeAt(i);
  }

  memory[base + string.length] = 0;
};

// Decode a string from memory starting at address base.
const decode = (memory, base) => {
  let cursor = base;
  let result = '';

  while (memory[cursor] !== 0) {
    result += String.fromCharCode(memory[cursor++]);
  }

  return result;
};

function add_numbers(a, b) {
  let val = functions.add(a, b)
  console.log(`${a} + ${b} = ${val}`)
}

function json_format() {
  const text = JSON.stringify({ thing: 1, otherThing: [1, 2, "banana"] });
  const view = new Uint8Array(memory.buffer);
  const pInput = functions.malloc(text.length);
  const pOutput = functions.malloc(text.length * 2);
  encode(view, pInput, text);

  functions.format(pInput, pOutput, text.length)

  const result = decode(view, pOutput);

  console.log(` IN:\n${text}`)
  console.log(`OUT:\n${result}`)

  functions.free(pInput);
  functions.free(pOutput);
}
