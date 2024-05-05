const memory = new WebAssembly.Memory({ initial: 2 });
/**
 * @type {((fd: number, data: string) => void)[]}
 */
const write_callbacks = [];
/**
 * @param {function(number, string): void} callback
 */
function on_write(callback) {
  write_callbacks.push(callback);
}

const decoder = new TextDecoder("utf8");

/** @param {number} fd @param {string} data */
function out(fd, data) {
  for (let cb of write_callbacks) {
    cb(fd, data);
  }
}

/**
 * @param {number} fd
 * @param {number} iovs
 * @param {number} iovs_len
 * @param {number} ret_ptr
 */
function fd_write(fd, iovs, iovs_len, ret_ptr) {
  const view = new Uint32Array(memory.buffer);

  let nwritten = 0;
  for (let i = 0; i < iovs_len; i++) {
    const offset = i * 8; // = jump over 2 i32 values per iteration
    const iov = new Uint32Array(view.buffer, iovs + offset, 2);
    // use the iovs to read the data from the memory
    const bytes = new Uint8Array(view.buffer, iov[0], iov[1]);
    const data = decoder.decode(bytes);
    out(fd, data);
    nwritten += iov[1];
  }

  // Set the nwritten in ret_ptr
  const bytes_written = new Uint32Array(view.buffer, ret_ptr, 1);
  bytes_written[0] = nwritten;

  return 0;
}

const importObject = {
  env: { memory },
  wasi_snapshot_preview1: {
    args_get: (/** @type {any} */ ...args) => { out(1, `not implemented: args_get(${args})\n`); return 0; },
    args_sizes_get: (/** @type {any} */ ...args) => { out(1, `not implemented: args_sizes_get(${args})\n`); return 0; },
    environ_get: (/** @type {any} */ ...args) => { out(1, `not implemented: environ_get(${args})\n`); return 0; },
    environ_sizes_get: (/** @type {any} */ ...args) => { out(1, `not implemented: environ_sizes_get(${args})\n`); return 0; },
    clock_res_get: (/** @type {any} */ ...args) => { out(1, `not implemented: clock_res_get(${args})\n`); return 0; },
    clock_time_get: (/** @type {any} */ ...args) => { out(1, `not implemented: clock_time_get(${args})\n`); return 0; },
    fd_advise: (/** @type {any} */ ...args) => { out(1, `not implemented: fd_advise(${args})\n`); return 0; },
    fd_allocate: (/** @type {any} */ ...args) => { out(1, `not implemented: fd_allocate(${args})\n`); return 0; },
    fd_close: (/** @type {any} */ ...args) => { out(1, `not implemented: fd_close(${args})\n`); return 0; },
    fd_datasync: (/** @type {any} */ ...args) => { out(1, `not implemented: fd_datasync(${args})\n`); return 0; },
    fd_fdstat_get: (/** @type {any} */ ...args) => { out(1, `not implemented: fd_fdstat_get(${args})\n`); return 0; },
    fd_fdstat_set_flags: (/** @type {any} */ ...args) => { out(1, `not implemented: fd_fdstat_set_flags(${args})\n`); return 0; },
    fd_fdstat_set_rights: (/** @type {any} */ ...args) => { out(1, `not implemented: fd_fdstat_set_rights(${args})\n`); return 0; },
    fd_filestat_get: (/** @type {any} */ ...args) => { out(1, `not implemented: fd_filestat_get(${args})\n`); return 0; },
    fd_filestat_set_size: (/** @type {any} */ ...args) => { out(1, `not implemented: fd_filestat_set_size(${args})\n`); return 0; },
    fd_filestat_set_times: (/** @type {any} */ ...args) => { out(1, `not implemented: fd_filestat_set_times(${args})\n`); return 0; },
    fd_pread: (/** @type {any} */ ...args) => { out(1, `not implemented: fd_pread(${args})\n`); return 0; },
    fd_prestat_get: (/** @type {any} */ ...args) => { out(1, `not implemented: fd_prestat_get(${args})\n`); return 0; },
    fd_prestat_dir_name: (/** @type {any} */ ...args) => { out(1, `not implemented: fd_prestat_dir_name(${args})\n`); return 0; },
    fd_pwrite: (/** @type {any} */ ...args) => { out(1, `not implemented: fd_pwrite(${args})\n`); return 0; },
    fd_read: (/** @type {any} */ ...args) => { out(1, `not implemented: fd_read(${args})\n`); return 0; },
    fd_readdir: (/** @type {any} */ ...args) => { out(1, `not implemented: fd_readdir(${args})\n`); return 0; },
    fd_renumber: (/** @type {any} */ ...args) => { out(1, `not implemented: fd_renumber(${args})\n`); return 0; },
    fd_seek: (/** @type {any} */ ...args) => { out(1, `not implemented: fd_seek(${args})\n`); return 0; },
    fd_sync: (/** @type {any} */ ...args) => { out(1, `not implemented: fd_sync(${args})\n`); return 0; },
    fd_tell: (/** @type {any} */ ...args) => { out(1, `not implemented: fd_tell(${args})\n`); return 0; },
    fd_write,
    path_create_directory: (/** @type {any} */ ...args) => { out(1, `not implemented: path_create_directory(${args})\n`); return 0; },
    path_filestat_get: (/** @type {any} */ ...args) => { out(1, `not implemented: path_filestat_get(${args})\n`); return 0; },
    path_filestat_set_times: (/** @type {any} */ ...args) => { out(1, `not implemented: path_filestat_set_times(${args})\n`); return 0; },
    path_link: (/** @type {any} */ ...args) => { out(1, `not implemented: path_link(${args})\n`); return 0; },
    path_open: (/** @type {any} */ ...args) => { out(1, `not implemented: path_open(${args})\n`); return 0; },
    path_readlink: (/** @type {any} */ ...args) => { out(1, `not implemented: path_readlink(${args})\n`); return 0; },
    path_remove_directory: (/** @type {any} */ ...args) => { out(1, `not implemented: path_remove_directory(${args})\n`); return 0; },
    path_rename: (/** @type {any} */ ...args) => { out(1, `not implemented: path_rename(${args})\n`); return 0; },
    path_symlink: (/** @type {any} */ ...args) => { out(1, `not implemented: path_symlink(${args})\n`); return 0; },
    path_unlink_file: (/** @type {any} */ ...args) => { out(1, `not implemented: path_unlink_file(${args})\n`); return 0; },
    poll_oneoff: (/** @type {any} */ ...args) => { out(1, `not implemented: poll_oneoff(${args})\n`); return 0; },
    proc_exit: (/** @type {any} */ ...args) => { out(1, `not implemented: proc_exit(${args})\n`); return 0; },
    sched_yield: (/** @type {any} */ ...args) => { out(1, `not implemented: sched_yield(${args})\n`); return 0; },
    random_get: (/** @type {any} */ ...args) => { out(1, `not implemented: random_get(${args})\n`); return 0; },
    sock_accept: (/** @type {any} */ ...args) => { out(1, `not implemented: sock_accept(${args})\n`); return 0; },
    sock_recv: (/** @type {any} */ ...args) => { out(1, `not implemented: sock_recv(${args})\n`); return 0; },
    sock_send: (/** @type {any} */ ...args) => { out(1, `not implemented: sock_send(${args})\n`); return 0; },
    sock_shutdown: (/** @type {any} */ ...args) => { out(1, `not implemented: sock_shutdown(${args})\n`); return 0; },
  }
};

// Encode string into memory starting at address base.
const encode = (/** @type {Uint8Array} */ memory, /** @type {number} */ base, /** @type {string} */ string) => {
  for (let i = 0; i < string.length; i++) {
    memory[base + i] = string.charCodeAt(i);
  }

  memory[base + string.length] = 0;
};

// Decode a string from memory starting at address base.
const decode = (/** @type {number[]} */ memory, /** @type {number} */ base) => {
  let cursor = base;
  let result = '';

  while (memory[cursor] !== 0) {
    result += String.fromCharCode(memory[cursor++]);
  }

  return result;
};

// * @param {function(number, string): void} callback

/**  @typedef bindings
 * @property {function(string): void} debug
 * @property {function(string): void} info
 * @property {function(string): void} warn
 * @property {function(string): void} error
 */
/** @returns {Promise<bindings>} */
async function load() {
  const module = await WebAssembly.instantiateStreaming(fetch('/lib.wasm'), importObject)
  const instance = module.instance;
  /** @type any */
  const f = instance.exports;


  const print_helper = (/** @type {any} */ printer, /** @type {string} */ str) => {
    const view = new Uint8Array(memory.buffer);
    const c = f.malloc(str.length + 1);
    encode(view, c, str);
    printer(c);
    f.free(c);
  }

  return {
    debug: (/** @type {string} */ str) => print_helper(f.debug, str),
    info: (/** @type {string} */ str) => print_helper(f.info, str),
    warn: (/** @type {string} */ str) => print_helper(f.warn, str),
    error: (/** @type {string} */ str) => print_helper(f.error, str),
  }
}

export {
  on_write,
  load,
}
