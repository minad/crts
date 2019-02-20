const memory = new WebAssembly.Memory({initial: 20});
const utf8decoder = new TextDecoder("utf8");
const utf8encoder = new TextEncoder("utf8");
const node = typeof process !== "undefined";

let util, fs, args;
if (node) {
    util = require('util');
    fs = require('fs');
    args = process.argv.slice(2);
    if (process.stdout.fd !== 1 || process.stderr.fd !== 2)
        console.error("Invalid output stream fds");
} else {
    args = ["run.wasm", "-Rstats"];
}

if (node) {
    const clock_start = process.hrtime.bigint();
    function wasm_clock(time) {
        const ns = process.hrtime.bigint() - clock_start;
        const u32 = new Uint32Array(memory.buffer);
        u32[time>>2] = Number(ns & BigInt(0xFFFFFFFF));
        u32[(time+4)>>2] = Number(ns >> BigInt(32));
    }
    function strlen(s) {
        const u8 = new Uint8Array(memory.buffer);
        let len = 0;
        while (u8[s++])
            ++len;
        return len;
    }
    function copy_string(s) {
        return utf8decoder.decode(new Uint8Array(memory.buffer, s, strlen(s)));
    }
    function wasm_sink_open(file) {
        try {
            return fs.openSync(copy_string(file), "w");
        } catch (e) {
            console.log(e);
        }
        return 0xFFFFFFFF;
    }
    function wasm_sink_close(fd) {
        fs.closeSync(fd);
    }
    function wasm_sink_write(fd, buf, size) {
        return fs.writeSync(fd, new Uint8Array(memory.buffer, buf, size), null);
    }
} else {
    const clock_start = Date.now();
    function wasm_clock(time) {
        const ms = Date.now() - clock_start;
        const u32 = new Uint32Array(memory.buffer);
        u32[time>>2] = ms * 1000000;
        u32[(time+4)>>2] = 0;
    }
    function wasm_sink_close(fd) {
    }
    function wasm_sink_open(file) {
        return 0xFFFFFFFF;
    }
    function wasm_sink_write(fd, buf, size) {
        console.log(utf8decoder.decode(new Uint8Array(memory.buffer, buf, size)));
        return size;
    }
}

function wasm_throw(event) {
    throw event;
}

function wasm_args_copy(args_buf) {
    let arg_pos = args_buf, str_pos = args_buf + (args.length + 1) * 4;
    const u8 = new Uint8Array(memory.buffer);
    const u32 = new Uint32Array(memory.buffer);
    for (let i = 0; i < args.length; ++i) {
        u32[arg_pos >> 2] = str_pos;
        arg_pos += 4;
        const s = utf8encoder.encode(args[i]);
        for (let j = 0; j < s.byteLength; ++j)
            u8[str_pos + j] = s[j];
        str_pos += s.byteLength;
        u8[str_pos++] = 0;
    }
    u32[arg_pos >> 2] = 0;
    return args.length;
}

function wasm_args_size() {
    let size = (args.length + 1) * 4;
    for (let i = 0; i < args.length; ++i)
        size += utf8encoder.encode(args[i]).byteLength + 1;
    return size;
}

const env = {
    memory,
    wasm_clock,
    wasm_throw,
    wasm_sink_write,
    wasm_sink_close,
    wasm_sink_open,
    wasm_args_size,
    wasm_args_copy,
};

let wasm_enter;

function loop() {
    try {
        wasm_enter();
    } catch (e) {
        if (typeof e === "number") {
            const u32 = new Uint32Array(memory.buffer);
            const event = u32[e>>2], payload = u32[(e+4)>>2];
            if (event === 0) {
                if (node)
                    process.exit(payload);
            } else if (event === 1) {
                setTimeout(loop, payload)
            } else {
                console.log("Unknown event " + event);
            }
        } else {
            console.error(e);
        }
    }
}

function run(result) {
    wasm_enter = result.instance.exports.wasm_enter;
    loop();
}

if (node) {
    fs.readFile(args[0], (err, data) =>
                WebAssembly.instantiate(data, {env}).then(run));
} else {
    WebAssembly.instantiateStreaming(fetch(args[0]), {env}).then(run);
}
